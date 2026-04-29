/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <limits.h>
#include <net/if.h>

#include "../modem_5g_core.h"

#define MR880A_DEFAULT_BAUD 9600
#define MR880A_AT_TIMEOUT_MS 2000
#define MR880A_MAX_PDP 20

enum mr880a_net_mode {
    MR880A_NET_UNKNOWN = 0,
    MR880A_NET_ECM,
    MR880A_NET_NCM,
    MR880A_NET_RNDIS,
};

struct mr880a_priv {
    char dev_path[PATH_MAX];
    uint32_t baud;
    int fd;
    enum mr880a_net_mode net_mode;
    char net_ifname[32];
    bool kernel_ecm_ok;
    bool kernel_ncm_ok;
    enum modem_5g_power_state power_state;
    enum modem_5g_data_state data_state;
    struct modem_5g_pdp_context pdp_ctx[MR880A_MAX_PDP + 1];
    bool pdp_valid[MR880A_MAX_PDP + 1];
};

static speed_t baud_to_speed(uint32_t baud)
{
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default: return B115200;
    }
}

static void str_trim(char *s)
{
    char *start;
    char *end;

    if (!s)
        return;

    start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1)))
        end--;
    *end = '\0';
}

static int read_sysfs_int(const char *path, int *value)
{
    char buf[32];
    int fd;
    ssize_t n;

    if (!path || !value)
        return -1;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';
    *value = atoi(buf);
    return 0;
}

static int read_sysfs_str(const char *path, char *buf, size_t len)
{
    int fd;
    ssize_t n;

    if (!path || !buf || len == 0)
        return -1;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, len - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';
    str_trim(buf);
    return 0;
}

static bool resp_has_ok(const char *buf)
{
    return buf && (strstr(buf, "\r\nOK\r\n") || strstr(buf, "\nOK\n") ||
        (strncmp(buf, "OK\r\n", 4) == 0));
}

static bool resp_has_error(const char *buf)
{
    if (!buf)
        return false;
    if (strstr(buf, "ERROR") || strstr(buf, "+CME ERROR"))
        return true;
    return false;
}

static enum modem_5g_status mr880a_at_exec(struct mr880a_priv *priv,
    const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    char local[2048];
    char sendbuf[256];
    size_t buf_len = 0;
    size_t buf_cap = resp_len;
    char *buf = resp;
    struct timeval start_tv, cur_tv;

    if (!priv || priv->fd < 0 || !cmd)
        return MODEM_5G_STATUS_NOT_READY;

    if (!buf || buf_cap == 0) {
        buf = local;
        buf_cap = sizeof(local);
    }
    buf[0] = '\0';

    if (strlen(cmd) >= sizeof(sendbuf) - 2)
        return MODEM_5G_STATUS_INVALID;

    snprintf(sendbuf, sizeof(sendbuf), "%s", cmd);
    if (sendbuf[strlen(sendbuf) - 1] != '\r' &&
        sendbuf[strlen(sendbuf) - 1] != '\n') {
        strncat(sendbuf, "\r", sizeof(sendbuf) - strlen(sendbuf) - 1);
    }

    tcflush(priv->fd, TCIOFLUSH);
    if (write(priv->fd, sendbuf, strlen(sendbuf)) < 0)
        return MODEM_5G_STATUS_FAIL;

    if (timeout_ms == 0)
        timeout_ms = MR880A_AT_TIMEOUT_MS;

    gettimeofday(&start_tv, NULL);

    while (1) {
        fd_set rfds;
        struct timeval tv;
        char chunk[256];
        ssize_t n;
        int64_t elapsed_ms;

        FD_ZERO(&rfds);
        FD_SET(priv->fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000;

        (void)select(priv->fd + 1, &rfds, NULL, NULL, &tv);
        if (FD_ISSET(priv->fd, &rfds)) {
            n = read(priv->fd, chunk, sizeof(chunk) - 1);
            if (n > 0) {
                chunk[n] = '\0';
                if (buf_len + (size_t)n + 1 < buf_cap) {
                    memcpy(buf + buf_len, chunk, (size_t)n);
                    buf_len += (size_t)n;
                    buf[buf_len] = '\0';
                }
                if (resp_has_ok(chunk) || resp_has_ok(buf))
                    return MODEM_5G_STATUS_SUCCESS;
                if (resp_has_error(chunk) || resp_has_error(buf))
                    return MODEM_5G_STATUS_FAIL;
            }
        }

        gettimeofday(&cur_tv, NULL);
        elapsed_ms = (int64_t)(cur_tv.tv_sec - start_tv.tv_sec) * 1000 +
            (cur_tv.tv_usec - start_tv.tv_usec) / 1000;
        if (elapsed_ms >= (int64_t)timeout_ms)
            break;
    }

    return MODEM_5G_STATUS_TIMEOUT;
}

static int copy_line(const char *start, char *out, size_t out_len)
{
    size_t i = 0;

    if (!start || !out || out_len == 0)
        return -1;

    while (start[i] && start[i] != '\r' && start[i] != '\n') {
        if (i + 1 < out_len)
            out[i] = start[i];
        i++;
    }

    if (out_len > 0) {
        if (i >= out_len)
            out[out_len - 1] = '\0';
        else
            out[i] = '\0';
    }

    return (int)i;
}

static int extract_line(const char *resp, const char *prefix,
    char *out, size_t out_len)
{
    const char *p = resp;
    size_t prelen;

    if (!resp || !prefix || !out || out_len == 0)
        return -1;

    prelen = strlen(prefix);
    while (p && *p) {
        while (*p == '\r' || *p == '\n')
            p++;
        if (!*p)
            break;
        if (strncmp(p, prefix, prelen) == 0)
            return copy_line(p, out, out_len);
        while (*p && *p != '\n')
            p++;
    }

    return -1;
}

static int first_data_line(const char *resp, char *out, size_t out_len)
{
    const char *p = resp;
    char line[256];

    if (!resp || !out || out_len == 0)
        return -1;

    while (p && *p) {
        while (*p == '\r' || *p == '\n')
            p++;
        if (!*p)
            break;
        if (copy_line(p, line, sizeof(line)) > 0) {
            if (strncmp(line, "AT", 2) != 0 &&
                strcmp(line, "OK") != 0 &&
                strcmp(line, "ERROR") != 0 &&
                strncmp(line, "+CME ERROR", 10) != 0) {
                strncpy(out, line, out_len - 1);
                out[out_len - 1] = '\0';
                return 0;
            }
        }
        while (*p && *p != '\n')
            p++;
    }

    return -1;
}

static enum mr880a_net_mode net_mode_from_unetmode(int mode)
{
    switch (mode) {
    case 0:
    case 3:
    case 4:
    case 7:
    case 8:
    case 11:
    case 12:
    case 15:
        return MR880A_NET_NCM;
    case 2:
    case 6:
    case 10:
    case 14:
        return MR880A_NET_ECM;
    case 1:
    case 5:
    case 9:
    case 13:
        return MR880A_NET_RNDIS;
    default:
        return MR880A_NET_UNKNOWN;
    }
}

static int detect_at_port(char *out, size_t out_len)
{
    DIR *dir;
    struct dirent *ent;
    char best_dev[PATH_MAX] = {0};

    printf("[MR880A] scanning AT port in /sys/bus/usb/devices\n");

    dir = opendir("/sys/bus/usb/devices");
    if (!dir)
        return -1;

    while ((ent = readdir(dir)) != NULL) {
        char proto_path[PATH_MAX];
        char tty_path[PATH_MAX];
        DIR *tty_dir;
        struct dirent *tty_ent;
        int proto = -1;

        if (ent->d_name[0] == '.')
            continue;
        if (!strchr(ent->d_name, ':'))
            continue;

        snprintf(proto_path, sizeof(proto_path),
            "/sys/bus/usb/devices/%s/bInterfaceProtocol", ent->d_name);
        if (read_sysfs_int(proto_path, &proto) != 0 || proto != 12)
            continue;

        snprintf(tty_path, sizeof(tty_path),
            "/sys/bus/usb/devices/%s", ent->d_name);
        tty_dir = opendir(tty_path);
        if (!tty_dir)
            continue;

        while ((tty_ent = readdir(tty_dir)) != NULL) {
            if (strncmp(tty_ent->d_name, "ttyUSB", 6) == 0 ||
                strncmp(tty_ent->d_name, "ttyACM", 6) == 0) {
                snprintf(best_dev, sizeof(best_dev), "/dev/%s", tty_ent->d_name);
                printf("[MR880A] AT port match: %s (iface=%s)\n",
                    best_dev, ent->d_name);
                closedir(tty_dir);
                closedir(dir);
                strncpy(out, best_dev, out_len - 1);
                out[out_len - 1] = '\0';
                return 0;
            }
        }

        closedir(tty_dir);
    }

    closedir(dir);
    printf("[MR880A] AT port auto-detect failed\n");
    return -1;
}

static int detect_net_iface(char *ifname, size_t ifname_len,
    enum mr880a_net_mode *mode)
{
    DIR *dir;
    struct dirent *ent;
    char best_ifname[IFNAMSIZ] = {0};
    enum mr880a_net_mode best_mode = MR880A_NET_UNKNOWN;

    if (!ifname || ifname_len == 0 || !mode)
        return -1;

    memset(ifname, 0, ifname_len);
    *mode = MR880A_NET_UNKNOWN;

    dir = opendir("/sys/class/net");
    if (!dir) {
        printf("[MR880A] net iface auto-detect failed\n");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        char iface_path[PATH_MAX];
        char iface_str[128];

        if (ent->d_name[0] == '.')
            continue;
        if (strcmp(ent->d_name, "lo") == 0)
            continue;

        snprintf(iface_path, sizeof(iface_path),
            "/sys/class/net/%s/device/interface", ent->d_name);
        if (read_sysfs_str(iface_path, iface_str, sizeof(iface_str)) != 0)
            continue;

        if (strstr(iface_str, "NCM Network Control Model")) {
            strncpy(best_ifname, ent->d_name, sizeof(best_ifname) - 1);
            best_mode = MR880A_NET_NCM;
            printf("[MR880A] net iface match: %s (%s)\n",
                best_ifname, iface_str);
            break;
        }
    }

    closedir(dir);

    if (best_ifname[0]) {
        strncpy(ifname, best_ifname, ifname_len - 1);
        ifname[ifname_len - 1] = '\0';
        *mode = best_mode;
        return 0;
    }

    printf("[MR880A] net iface auto-detect failed\n");
    return -1;
}

struct mr880a_kernel_cfg {
    bool netdevices;
    bool usb_net_drivers;
    bool usb_usbnet;
    bool usb_cdcether;
    bool usb_serial;
    bool usb_serial_option;
    bool usb_cdc_ncm;
};

static void update_kernel_cfg(struct mr880a_kernel_cfg *cfg, const char *line)
{
    char val;

    if (sscanf(line, "CONFIG_NETDEVICES=%c", &val) == 1)
        cfg->netdevices = (val == 'y');
    else if (sscanf(line, "CONFIG_USB_NET_DRIVERS=%c", &val) == 1)
        cfg->usb_net_drivers = (val == 'y' || val == 'm');
    else if (sscanf(line, "CONFIG_USB_USBNET=%c", &val) == 1)
        cfg->usb_usbnet = (val == 'y' || val == 'm');
    else if (sscanf(line, "CONFIG_USB_NET_CDCETHER=%c", &val) == 1)
        cfg->usb_cdcether = (val == 'y' || val == 'm');
    else if (sscanf(line, "CONFIG_USB_SERIAL=%c", &val) == 1)
        cfg->usb_serial = (val == 'y' || val == 'm');
    else if (sscanf(line, "CONFIG_USB_SERIAL_OPTION=%c", &val) == 1)
        cfg->usb_serial_option = (val == 'y' || val == 'm');
    else if (sscanf(line, "CONFIG_USB_NET_CDC_NCM=%c", &val) == 1)
        cfg->usb_cdc_ncm = (val == 'y' || val == 'm');
}

static enum modem_5g_status check_kernel_config(struct mr880a_priv *priv,
    enum mr880a_net_mode required)
{
    FILE *fp;
    char line[256];
    struct mr880a_kernel_cfg cfg = {0};
    bool ecm_ok;
    bool ncm_ok;

    fp = popen("zcat /proc/config.gz 2>/dev/null", "r");
    if (!fp) {
        printf("[MR880A] failed to run zcat /proc/config.gz: %s\n",
            strerror(errno));
        return MODEM_5G_STATUS_FAIL;
    }

    while (fgets(line, sizeof(line), fp))
        update_kernel_cfg(&cfg, line);

    pclose(fp);

    ecm_ok = cfg.netdevices && cfg.usb_net_drivers &&
        cfg.usb_usbnet && cfg.usb_cdcether;
    ncm_ok = cfg.usb_serial && cfg.usb_serial_option &&
        cfg.usb_usbnet && cfg.usb_cdcether && cfg.usb_cdc_ncm;

    priv->kernel_ecm_ok = ecm_ok;
    priv->kernel_ncm_ok = ncm_ok;
    printf("[MR880A] kernel cfg: ECM=%s NCM=%s\n",
        ecm_ok ? "OK" : "MISSING",
        ncm_ok ? "OK" : "MISSING");

    if (required == MR880A_NET_ECM && !ecm_ok)
        return MODEM_5G_STATUS_FAIL;
    if (required == MR880A_NET_NCM && !ncm_ok)
        return MODEM_5G_STATUS_FAIL;
    if (required == MR880A_NET_UNKNOWN && !ecm_ok && !ncm_ok)
        return MODEM_5G_STATUS_FAIL;

    return MODEM_5G_STATUS_SUCCESS;
}

static void mr880a_run_udhcpc(struct mr880a_priv *priv)
{
    char cmd[96];
    int ret;

    if (!priv->net_ifname[0]) {
        printf("[MR880A] udhcpc skipped: net iface unknown\n");
        return;
    }

    snprintf(cmd, sizeof(cmd), "udhcpc -i %s", priv->net_ifname);
    printf("[MR880A] running: %s\n", cmd);
    ret = system(cmd);
    if (ret == -1) {
        printf("[MR880A] udhcpc failed to start\n");
        return;
    }
    if (WIFEXITED(ret)) {
        printf("[MR880A] udhcpc exit code: %d\n", WEXITSTATUS(ret));
    } else {
        printf("[MR880A] udhcpc exit status: %d\n", ret);
    }
}

static void mr880a_refresh_net_iface(struct mr880a_priv *priv, bool run_dhcp)
{
    enum mr880a_net_mode mode = MR880A_NET_UNKNOWN;
    char ifname[32] = {0};
    int retries = run_dhcp ? 10 : 3;
    int i;

    if (!priv)
        return;

    for (i = 0; i < retries; i++) {
        if (detect_net_iface(ifname, sizeof(ifname), &mode) == 0) {
            strncpy(priv->net_ifname, ifname, sizeof(priv->net_ifname) - 1);
            priv->net_ifname[sizeof(priv->net_ifname) - 1] = '\0';
            if (mode != MR880A_NET_UNKNOWN) {
                priv->net_mode = mode;
            }
            break;
        }
        usleep(200 * 1000);
    }

    if (run_dhcp)
        mr880a_run_udhcpc(priv);
}

static enum modem_5g_status mr880a_get_unetmode(struct mr880a_priv *priv,
    int *mode)
{
    char resp[256];
    char line[128];

    if (!priv || !mode)
        return MODEM_5G_STATUS_INVALID;

    if (mr880a_at_exec(priv, "AT+UNETMODECFG?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    if (extract_line(resp, "+UNETMODECFG:", line, sizeof(line)) < 0)
        return MODEM_5G_STATUS_FAIL;

    if (sscanf(line, "+UNETMODECFG: %d", mode) != 1)
        return MODEM_5G_STATUS_FAIL;

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_power_state(struct mr880a_priv *priv,
    enum modem_5g_power_state *state)
{
    char resp[128];
    char line[64];
    int fun = 0;

    if (!priv || !state)
        return MODEM_5G_STATUS_INVALID;

    if (mr880a_at_exec(priv, "AT+CFUN?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    if (extract_line(resp, "+CFUN:", line, sizeof(line)) < 0)
        return MODEM_5G_STATUS_FAIL;

    if (sscanf(line, "+CFUN: %d", &fun) != 1)
        return MODEM_5G_STATUS_FAIL;

    if (fun == 0)
        *state = MODEM_5G_POWER_OFF;
    else
        *state = MODEM_5G_POWER_ON;

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_sim_state map_cpin_state(const char *state)
{
    if (!state)
        return MODEM_5G_SIM_UNKNOWN;
    if (strstr(state, "READY"))
        return MODEM_5G_SIM_READY;
    if (strstr(state, "SIM PIN"))
        return MODEM_5G_SIM_PIN_REQUIRED;
    if (strstr(state, "SIM PUK"))
        return MODEM_5G_SIM_PUK_REQUIRED;
    if (strstr(state, "NOT INSERTED"))
        return MODEM_5G_SIM_ABSENT;
    return MODEM_5G_SIM_ERROR;
}

static enum modem_5g_rat map_act_to_rat(int act)
{
    switch (act) {
    case 9:
    case 10:
        return MODEM_5G_RAT_NR5G_SA;
    case 7:
        return MODEM_5G_RAT_LTE;
    case 2:
    case 4:
    case 5:
    case 6:
        return MODEM_5G_RAT_WCDMA;
    case 0:
    case 1:
    case 3:
    case 8:
        return MODEM_5G_RAT_GSM;
    default:
        return MODEM_5G_RAT_UNKNOWN;
    }
}

static enum modem_5g_status parse_reg_line(const char *line,
    struct modem_5g_reg_info *info)
{
    char buf[256];
    char *p;
    int idx = 0;
    int act = -1;

    if (!line || !info)
        return MODEM_5G_STATUS_INVALID;

    snprintf(buf, sizeof(buf), "%s", line);
    p = strchr(buf, ':');
    if (!p)
        return MODEM_5G_STATUS_FAIL;
    p++;

    while (p && *p) {
        char *comma = strchr(p, ',');
        if (comma)
            *comma = '\0';
        str_trim(p);
        if (*p == '\0') {
            idx++;
        } else if (idx == 1) {
            info->state = (enum modem_5g_reg_state)atoi(p);
        } else if (idx == 2) {
            info->tac = (uint32_t)strtoul(p, NULL, 16);
        } else if (idx == 3) {
            info->cell_id = (uint32_t)strtoul(p, NULL, 16);
        } else if (idx == 4) {
            act = atoi(p);
        }
        idx++;
        if (!comma)
            break;
        p = comma + 1;
    }

    if (act >= 0)
        info->rat = map_act_to_rat(act);
    else
        info->rat = MODEM_5G_RAT_UNKNOWN;

    return MODEM_5G_STATUS_SUCCESS;
}

static void hex_ipv4_to_str(const char *hex, char *out, size_t len)
{
    uint32_t val;
    unsigned int b0, b1, b2, b3;

    if (!hex || !out || len == 0) {
        if (out && len > 0)
            out[0] = '\0';
        return;
    }

    val = (uint32_t)strtoul(hex, NULL, 16);
    b0 = val & 0xff;
    b1 = (val >> 8) & 0xff;
    b2 = (val >> 16) & 0xff;
    b3 = (val >> 24) & 0xff;
    snprintf(out, len, "%u.%u.%u.%u", b0, b1, b2, b3);
}

static enum modem_5g_status mr880a_init(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv;
    struct termios tty;
    speed_t speed;
    char auto_dev[PATH_MAX];
    int mode = -1;
    enum mr880a_net_mode unet_mode = MR880A_NET_UNKNOWN;

    if (!dev || !dev->priv_data)
        return MODEM_5G_STATUS_INVALID;

    priv = dev->priv_data;

    if (priv->dev_path[0] == '\0' || strcmp(priv->dev_path, "auto") == 0) {
        if (detect_at_port(auto_dev, sizeof(auto_dev)) != 0) {
            printf("[MR880A] failed to auto-detect AT port\n");
            return MODEM_5G_STATUS_FAIL;
        }
        strncpy(priv->dev_path, auto_dev, sizeof(priv->dev_path) - 1);
        priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    }

    if (priv->baud == 0)
        priv->baud = MR880A_DEFAULT_BAUD;

    priv->fd = open(priv->dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (priv->fd < 0) {
        printf("[MR880A] failed to open %s: %s\n",
            priv->dev_path, strerror(errno));
        if (strcmp(priv->dev_path, "auto") != 0) {
            if (detect_at_port(auto_dev, sizeof(auto_dev)) == 0) {
                strncpy(priv->dev_path, auto_dev,
                    sizeof(priv->dev_path) - 1);
                priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
                printf("[MR880A] retry AT port: %s\n", priv->dev_path);
                priv->fd = open(priv->dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
            }
        }
        if (priv->fd < 0)
            return MODEM_5G_STATUS_FAIL;
    }

    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(priv->fd, &tty) != 0) {
        printf("[MR880A] tcgetattr failed: %s\n", strerror(errno));
        close(priv->fd);
        priv->fd = -1;
        return MODEM_5G_STATUS_FAIL;
    }

    speed = baud_to_speed(priv->baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(priv->fd, TCSANOW, &tty) != 0) {
        printf("[MR880A] tcsetattr failed: %s\n", strerror(errno));
        close(priv->fd);
        priv->fd = -1;
        return MODEM_5G_STATUS_FAIL;
    }

    tcflush(priv->fd, TCIOFLUSH);

    (void)mr880a_at_exec(priv, "AT", NULL, 0, 1000);
    (void)mr880a_at_exec(priv, "ATE0", NULL, 0, 1000);
    (void)mr880a_at_exec(priv, "AT+CMEE=2", NULL, 0, 1000);

    if (mr880a_get_unetmode(priv, &mode) == MODEM_5G_STATUS_SUCCESS)
        unet_mode = net_mode_from_unetmode(mode);

    if (detect_net_iface(priv->net_ifname,
        sizeof(priv->net_ifname), &priv->net_mode) == 0) {
        if (priv->net_mode == MR880A_NET_UNKNOWN)
            priv->net_mode = unet_mode;
    } else {
        priv->net_mode = unet_mode;
    }

    if (priv->net_mode == MR880A_NET_RNDIS) {
        printf("[MR880A] RNDIS mode detected, only ECM/NCM supported\n");
        return MODEM_5G_STATUS_UNSUPPORTED;
    }

    if (check_kernel_config(priv, priv->net_mode) != MODEM_5G_STATUS_SUCCESS) {
        printf("[MR880A] kernel config missing for %s mode\n",
            (priv->net_mode == MR880A_NET_NCM) ? "NCM" :
            (priv->net_mode == MR880A_NET_ECM) ? "ECM" : "unknown");
        return MODEM_5G_STATUS_FAIL;
    }

    printf("[MR880A] AT port: %s\n", priv->dev_path);
    if (priv->net_ifname[0]) {
        printf("[MR880A] net iface: %s (mode=%s)\n",
            priv->net_ifname,
            (priv->net_mode == MR880A_NET_NCM) ? "NCM" :
            (priv->net_mode == MR880A_NET_ECM) ? "ECM" :
            (priv->net_mode == MR880A_NET_RNDIS) ? "RNDIS" : "UNKNOWN");
    }

    (void)mr880a_power_state(priv, &priv->power_state);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_deinit(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv;

    if (!dev || !dev->priv_data)
        return MODEM_5G_STATUS_INVALID;

    priv = dev->priv_data;
    if (priv->fd >= 0) {
        close(priv->fd);
        priv->fd = -1;
    }

    return MODEM_5G_STATUS_SUCCESS;
}

static void mr880a_free(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv;

    if (!dev)
        return;

    priv = dev->priv_data;
    if (priv && priv->fd >= 0)
        close(priv->fd);

    if (dev->priv_data)
        free(dev->priv_data);
    if (dev->name)
        free((void *)dev->name);
    free(dev);
}

static enum modem_5g_status mr880a_power_on(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv = dev->priv_data;
    enum modem_5g_status ret;

    ret = mr880a_at_exec(priv, "AT+CFUN=1", NULL, 0, MR880A_AT_TIMEOUT_MS);
    if (ret == MODEM_5G_STATUS_SUCCESS)
        priv->power_state = MODEM_5G_POWER_ON;
    if (ret == MODEM_5G_STATUS_SUCCESS)
        mr880a_refresh_net_iface(priv, false);
    return ret;
}

static enum modem_5g_status mr880a_power_off(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv = dev->priv_data;
    enum modem_5g_status ret;

    ret = mr880a_at_exec(priv, "AT+CFUN=0", NULL, 0, MR880A_AT_TIMEOUT_MS);
    if (ret == MODEM_5G_STATUS_SUCCESS)
        priv->power_state = MODEM_5G_POWER_OFF;
    return ret;
}

static enum modem_5g_status mr880a_reset(struct modem_5g_dev *dev)
{
    struct mr880a_priv *priv = dev->priv_data;
    return mr880a_at_exec(priv, "AT+CFUN=1,1", NULL, 0, MR880A_AT_TIMEOUT_MS);
}

static enum modem_5g_status mr880a_set_flight_mode(struct modem_5g_dev *dev,
    bool enable)
{
    struct mr880a_priv *priv = dev->priv_data;
    const char *cmd = enable ? "AT+CFUN=4" : "AT+CFUN=1";
    return mr880a_at_exec(priv, cmd, NULL, 0, MR880A_AT_TIMEOUT_MS);
}

static enum modem_5g_status mr880a_get_power_state(struct modem_5g_dev *dev,
    enum modem_5g_power_state *state)
{
    struct mr880a_priv *priv = dev->priv_data;
    return mr880a_power_state(priv, state);
}

static enum modem_5g_status mr880a_get_basic_info(struct modem_5g_dev *dev,
    struct modem_5g_basic_info *info)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];

    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));

    if (mr880a_at_exec(priv, "AT+CGMI", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        first_data_line(resp, line, sizeof(line)) == 0) {
        strncpy(info->manufacturer, line, sizeof(info->manufacturer) - 1);
    }

    if (mr880a_at_exec(priv, "AT+CGMM", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        first_data_line(resp, line, sizeof(line)) == 0) {
        strncpy(info->model, line, sizeof(info->model) - 1);
    }

    if (mr880a_at_exec(priv, "AT+CGMR", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        first_data_line(resp, line, sizeof(line)) == 0) {
        strncpy(info->revision, line, sizeof(info->revision) - 1);
    }

    if (mr880a_at_exec(priv, "AT+CGSN", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        first_data_line(resp, line, sizeof(line)) == 0) {
        char *q = line;
        if (*q == '"') {
            q++;
            q[strcspn(q, "\"")] = '\0';
        }
        strncpy(info->imei, q, sizeof(info->imei) - 1);
    }

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_sim_info(struct modem_5g_dev *dev,
    struct modem_5g_sim_info *info)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];

    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_SIM_UNKNOWN;

    if (mr880a_at_exec(priv, "AT+CPIN?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+CPIN:", line, sizeof(line)) == 0) {
        char *p = strchr(line, ':');
        if (p) {
            p++;
            str_trim(p);
            info->state = map_cpin_state(p);
        }
    }

    if (mr880a_at_exec(priv, "AT^ICCID?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "^ICCID:", line, sizeof(line)) == 0) {
        char *p = strchr(line, ':');
        if (p) {
            p++;
            str_trim(p);
            strncpy(info->iccid, p, sizeof(info->iccid) - 1);
        }
    }

    if (mr880a_at_exec(priv, "AT+CIMI", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        first_data_line(resp, line, sizeof(line)) == 0) {
        strncpy(info->imsi, line, sizeof(info->imsi) - 1);
    }

    if (mr880a_at_exec(priv, "AT+CNUM", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+CNUM:", line, sizeof(line)) == 0) {
        char *p = strchr(line, ',');
        if (p) {
            p++;
            if (*p == '"') {
                p++;
                p[strcspn(p, "\"")] = '\0';
            }
            strncpy(info->msisdn, p, sizeof(info->msisdn) - 1);
        }
    }

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_reg_info(struct modem_5g_dev *dev,
    struct modem_5g_reg_info *info)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];
    enum modem_5g_status ret;

    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_REG_UNKNOWN;
    info->rat = MODEM_5G_RAT_UNKNOWN;

    ret = mr880a_at_exec(priv, "AT+C5GREG?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS);
    if (ret == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+C5GREG:", line, sizeof(line)) == 0) {
        (void)parse_reg_line(line, info);
    } else {
        ret = mr880a_at_exec(priv, "AT+CEREG?", resp, sizeof(resp),
            MR880A_AT_TIMEOUT_MS);
        if (ret == MODEM_5G_STATUS_SUCCESS &&
            extract_line(resp, "+CEREG:", line, sizeof(line)) == 0) {
            (void)parse_reg_line(line, info);
        } else if (mr880a_at_exec(priv, "AT+CGREG?", resp, sizeof(resp),
            MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
            extract_line(resp, "+CGREG:", line, sizeof(line)) == 0) {
            (void)parse_reg_line(line, info);
        }
    }

    if (mr880a_at_exec(priv, "AT+COPS?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+COPS:", line, sizeof(line)) == 0) {
        int mode = 0;
        int fmt = 0;
        int rat = 0;
        char oper[64] = {0};
        if (sscanf(line, "+COPS: %d,%d,\"%63[^\"]\",%d",
            &mode, &fmt, oper, &rat) >= 3) {
            strncpy(info->operator_name, oper,
                sizeof(info->operator_name) - 1);
            if (fmt == 2 && strlen(oper) >= 5) {
                strncpy(info->mcc, oper, 3);
                info->mcc[3] = '\0';
                strncpy(info->mnc, oper + 3, 3);
                info->mnc[3] = '\0';
            }
        }
    }

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_signal_info(struct modem_5g_dev *dev,
    struct modem_5g_signal_info *info)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];
    int rxlev = 99;
    int rscp = 99;
    int ecno = 99;
    int rsrq = 255;
    int rsrp = 255;
    int csq_rssi = -1;
    int rssi_dbm = 0;
    int rsrq_db = 0;
    int rsrp_dbm = 0;

    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));

    if (mr880a_at_exec(priv, "AT+CESQ", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+CESQ:", line, sizeof(line)) == 0) {
        (void)sscanf(line, "+CESQ: %d,%*d,%d,%d,%d,%d",
            &rxlev, &rscp, &ecno, &rsrq, &rsrp);
    }

    if (rxlev == 99 &&
        mr880a_at_exec(priv, "AT+CSQ", resp, sizeof(resp),
            MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "+CSQ:", line, sizeof(line)) == 0) {
        (void)sscanf(line, "+CSQ: %d,%*d", &csq_rssi);
        if (csq_rssi >= 0 && csq_rssi <= 31)
            rxlev = csq_rssi;
    }

    if (rxlev >= 0 && rxlev <= 31)
        rssi_dbm = -113 + 2 * rxlev;
    if (rsrq >= 0 && rsrq <= 34)
        rsrq_db = (rsrq * 5 - 195) / 10;
    if (rsrp >= 0 && rsrp <= 97)
        rsrp_dbm = -140 + rsrp;

    info->rssi = rssi_dbm;
    info->rsrq = rsrq_db;
    info->rsrp = rsrp_dbm;
    info->sinr = ecno;

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_set_prefer_rat(struct modem_5g_dev *dev,
    enum modem_5g_rat rat)
{
    struct mr880a_priv *priv = dev->priv_data;
    const char *cmd = NULL;

    if (rat == MODEM_5G_RAT_NR5G_SA)
        cmd = "AT^C5GOPTION=1,0,1";
    else if (rat == MODEM_5G_RAT_NR5G_NSA)
        cmd = "AT^C5GOPTION=0,1,0";
    else
        return MODEM_5G_STATUS_UNSUPPORTED;

    return mr880a_at_exec(priv, cmd, NULL, 0, MR880A_AT_TIMEOUT_MS);
}

static enum modem_5g_status mr880a_set_pdp_context(struct modem_5g_dev *dev,
    const struct modem_5g_pdp_context *ctx)
{
    struct mr880a_priv *priv = dev->priv_data;
    char cmd[256];
    const char *type;

    if (!ctx || ctx->cid == 0 || ctx->cid > MR880A_MAX_PDP)
        return MODEM_5G_STATUS_INVALID;

    switch (ctx->pdp_type) {
    case MODEM_5G_PDP_IPV4: type = "IP"; break;
    case MODEM_5G_PDP_IPV6: type = "IPV6"; break;
    case MODEM_5G_PDP_IPV4V6: type = "IPV4V6"; break;
    default: type = "IPV4V6"; break;
    }

    if (ctx->apn[0])
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%u,\"%s\",\"%s\"",
            ctx->cid, type, ctx->apn);
    else
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%u,\"%s\"",
            ctx->cid, type);

    if (mr880a_at_exec(priv, cmd, NULL, 0, MR880A_AT_TIMEOUT_MS)
        != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    priv->pdp_ctx[ctx->cid] = *ctx;
    priv->pdp_valid[ctx->cid] = true;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_pdp_context(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_pdp_context *ctx)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[512];
    char line[256];
    const char *p;
    int found = 0;

    if (!ctx || cid == 0 || cid > MR880A_MAX_PDP)
        return MODEM_5G_STATUS_INVALID;

    if (priv->pdp_valid[cid]) {
        *ctx = priv->pdp_ctx[cid];
        return MODEM_5G_STATUS_SUCCESS;
    }

    if (mr880a_at_exec(priv, "AT+CGDCONT?", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    p = resp;
    while ((p = strstr(p, "+CGDCONT:")) != NULL) {
        int lc = 0;
        char type[16] = {0};
        char apn[MODEM_5G_APN_MAX_LEN + 1] = {0};
        if (copy_line(p, line, sizeof(line)) > 0 &&
            sscanf(line, "+CGDCONT: %d,\"%15[^\"]\",\"%63[^\"]\"",
                &lc, type, apn) >= 2) {
            if ((uint8_t)lc == cid) {
                memset(ctx, 0, sizeof(*ctx));
                ctx->cid = cid;
                if (strcmp(type, "IPV6") == 0)
                    ctx->pdp_type = MODEM_5G_PDP_IPV6;
                else if (strcmp(type, "IPV4V6") == 0)
                    ctx->pdp_type = MODEM_5G_PDP_IPV4V6;
                else
                    ctx->pdp_type = MODEM_5G_PDP_IPV4;
                strncpy(ctx->apn, apn, sizeof(ctx->apn) - 1);
                found = 1;
                break;
            }
        }
        p++;
    }

    return found ? MODEM_5G_STATUS_SUCCESS : MODEM_5G_STATUS_FAIL;
}

static enum modem_5g_status mr880a_data_start(struct modem_5g_dev *dev,
    uint8_t cid)
{
    struct mr880a_priv *priv = dev->priv_data;
    char cmd[256];
    const struct modem_5g_pdp_context *ctx = NULL;

    if (cid == 0 || cid > MR880A_MAX_PDP)
        return MODEM_5G_STATUS_INVALID;

    if (priv->pdp_valid[cid])
        ctx = &priv->pdp_ctx[cid];

    if (ctx && ctx->apn[0]) {
        if (ctx->username[0] || ctx->password[0]) {
            snprintf(cmd, sizeof(cmd),
                "AT^NDISDUP=%u,1,\"%s\",\"%s\",\"%s\",1",
                cid, ctx->apn, ctx->username, ctx->password);
        } else {
            snprintf(cmd, sizeof(cmd),
                "AT^NDISDUP=%u,1,\"%s\"", cid, ctx->apn);
        }
    } else {
        snprintf(cmd, sizeof(cmd), "AT^NDISDUP=%u,1", cid);
    }

    if (mr880a_at_exec(priv, cmd, NULL, 0, MR880A_AT_TIMEOUT_MS)
        != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    priv->data_state = MODEM_5G_DATA_CONNECTED;
    mr880a_refresh_net_iface(priv, true);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_data_stop(struct modem_5g_dev *dev,
    uint8_t cid)
{
    struct mr880a_priv *priv = dev->priv_data;
    char cmd[64];

    if (cid == 0 || cid > MR880A_MAX_PDP)
        return MODEM_5G_STATUS_INVALID;

    snprintf(cmd, sizeof(cmd), "AT^NDISDUP=%u,0", cid);
    if (mr880a_at_exec(priv, cmd, NULL, 0, MR880A_AT_TIMEOUT_MS)
        != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    priv->data_state = MODEM_5G_DATA_DISCONNECTED;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_data_state(struct modem_5g_dev *dev,
    enum modem_5g_data_state *state)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];
    int ipv4_stat = 0;
    int ipv6_stat = 0;

    if (!state)
        return MODEM_5G_STATUS_INVALID;

    if (mr880a_at_exec(priv, "AT^NDISSTATQRY=1", resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) == MODEM_5G_STATUS_SUCCESS &&
        extract_line(resp, "^NDISSTATQRY:", line, sizeof(line)) == 0) {
        (void)sscanf(line, "^NDISSTATQRY: %d,%*d,,\"IPV4\",%d",
            &ipv4_stat, &ipv6_stat);
    }

    if (ipv4_stat == 1 || ipv6_stat == 1)
        *state = MODEM_5G_DATA_CONNECTED;
    else if (ipv4_stat == 2 || ipv6_stat == 2)
        *state = MODEM_5G_DATA_CONNECTING;
    else
        *state = MODEM_5G_DATA_DISCONNECTED;

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_get_ip_info(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_ip_info *info)
{
    struct mr880a_priv *priv = dev->priv_data;
    char resp[256];
    char line[128];
    char ip_hex[16] = {0};
    char gw_hex[16] = {0};
    char dns1_hex[16] = {0};
    char dns2_hex[16] = {0};
    char cmd[64];

    if (!info || cid == 0 || cid > MR880A_MAX_PDP)
        return MODEM_5G_STATUS_INVALID;

    snprintf(cmd, sizeof(cmd), "AT^DHCP=%u", cid);
    if (mr880a_at_exec(priv, cmd, resp, sizeof(resp),
        MR880A_AT_TIMEOUT_MS) != MODEM_5G_STATUS_SUCCESS)
        return MODEM_5G_STATUS_FAIL;

    if (extract_line(resp, "^DHCP:", line, sizeof(line)) < 0)
        return MODEM_5G_STATUS_FAIL;

    if (sscanf(line, "^DHCP: %15[^,],%*[^,],%15[^,],%*[^,],%15[^,],%15[^,]",
        ip_hex, gw_hex, dns1_hex, dns2_hex) < 4)
        return MODEM_5G_STATUS_FAIL;

    memset(info, 0, sizeof(*info));
    hex_ipv4_to_str(ip_hex, info->ip, sizeof(info->ip));
    hex_ipv4_to_str(gw_hex, info->gateway, sizeof(info->gateway));
    hex_ipv4_to_str(dns1_hex, info->dns1, sizeof(info->dns1));
    hex_ipv4_to_str(dns2_hex, info->dns2, sizeof(info->dns2));

    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mr880a_send_at(struct modem_5g_dev *dev,
    const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    struct mr880a_priv *priv = dev->priv_data;
    return mr880a_at_exec(priv, cmd, resp, resp_len, timeout_ms);
}

static const struct modem_5g_ops mr880a_ops = {
    .init = mr880a_init,
    .deinit = mr880a_deinit,
    .power_on = mr880a_power_on,
    .power_off = mr880a_power_off,
    .reset = mr880a_reset,
    .set_flight_mode = mr880a_set_flight_mode,
    .get_power_state = mr880a_get_power_state,
    .get_basic_info = mr880a_get_basic_info,
    .get_sim_info = mr880a_get_sim_info,
    .get_reg_info = mr880a_get_reg_info,
    .get_signal_info = mr880a_get_signal_info,
    .set_prefer_rat = mr880a_set_prefer_rat,
    .set_pdp_context = mr880a_set_pdp_context,
    .get_pdp_context = mr880a_get_pdp_context,
    .data_start = mr880a_data_start,
    .data_stop = mr880a_data_stop,
    .get_data_state = mr880a_get_data_state,
    .get_ip_info = mr880a_get_ip_info,
    .send_at = mr880a_send_at,
    .free = mr880a_free,
};

static struct modem_5g_dev *mr880a_factory(void *args)
{
    struct modem_5g_args_uart *cfg = args;
    struct modem_5g_dev *dev;
    struct mr880a_priv *priv;

    if (!cfg || !cfg->instance)
        return NULL;

    dev = modem_5g_dev_alloc(cfg->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    priv = dev->priv_data;
    priv->fd = -1;
    priv->baud = cfg->baud ? cfg->baud : MR880A_DEFAULT_BAUD;
    if (cfg->dev_path && cfg->dev_path[0]) {
        strncpy(priv->dev_path, cfg->dev_path, sizeof(priv->dev_path) - 1);
        priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    } else {
        strncpy(priv->dev_path, "auto", sizeof(priv->dev_path) - 1);
        priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    }
    priv->power_state = MODEM_5G_POWER_OFF;
    priv->data_state = MODEM_5G_DATA_DISCONNECTED;

    dev->ops = &mr880a_ops;
    return dev;
}

REGISTER_MODEM_5G_DRIVER("MR880A", MODEM_5G_DRV_UART, mr880a_factory);
