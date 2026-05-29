/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * MR880A 5G modem demo (UART AT)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "../include/5g.h"

#define PING_HOST "www.baidu.com"

static void print_usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -d <dev>    AT device path (default: auto, driver auto-detect)\n");
    printf("  -b <baud>   Baud rate (default: 115200)\n");
    printf("  -c <cid>    PDP context id (default: 1)\n");
    printf("  -a <apn>    APN string (default: empty)\n");
    printf("  -u <user>   APN username (default: empty)\n");
    printf("  -p <pass>   APN password (default: empty)\n");
    printf("  -t <type>   PDP type: IP, IPV6, IPV4V6 (default: IPV4V6)\n");
    printf("  -h          Show this help\n");
}

static enum modem_5g_pdp_type parse_pdp_type(const char *s)
{
    if (!s)
        return MODEM_5G_PDP_IPV4V6;
    if (strcasecmp(s, "IP") == 0)
        return MODEM_5G_PDP_IPV4;
    if (strcasecmp(s, "IPV6") == 0)
        return MODEM_5G_PDP_IPV6;
    if (strcasecmp(s, "IPV4V6") == 0)
        return MODEM_5G_PDP_IPV4V6;
    return MODEM_5G_PDP_IPV4V6;
}

static void print_basic_info(struct modem_5g_basic_info *info)
{
    printf("Manufacturer: %s\n", info->manufacturer);
    printf("Model:        %s\n", info->model);
    printf("Revision:     %s\n", info->revision);
    printf("IMEI:         %s\n", info->imei);
}

static void print_sim_info(struct modem_5g_sim_info *info)
{
    printf("SIM state:    %d\n", (int)info->state);
    printf("ICCID:        %s\n", info->iccid);
    printf("IMSI:         %s\n", info->imsi);
    printf("MSISDN:       %s\n", info->msisdn);
}

static void print_reg_info(struct modem_5g_reg_info *info)
{
    printf("Reg state:    %d\n", (int)info->state);
    printf("RAT:          %d\n", (int)info->rat);
    printf("TAC:          %u\n", info->tac);
    printf("Cell ID:      %u\n", info->cell_id);
    printf("Operator:     %s\n", info->operator_name);
    if (info->mcc[0] || info->mnc[0])
        printf("MCC/MNC:      %s/%s\n", info->mcc, info->mnc);
}

static void print_signal_info(struct modem_5g_signal_info *info)
{
    printf("RSSI:         %d dBm\n", info->rssi);
    printf("RSRP:         %d dBm\n", info->rsrp);
    printf("RSRQ:         %d dB\n", info->rsrq);
    printf("SINR:         %d dB\n", info->sinr);
}

int main(int argc, char *argv[])
{
    const char *dev_path = "auto";
    uint32_t baud = 9600;
    uint8_t cid = 1;
    char apn[MODEM_5G_APN_MAX_LEN + 1] = {0};
    char user[MODEM_5G_USERNAME_MAX_LEN + 1] = {0};
    char pass[MODEM_5G_PASSWORD_MAX_LEN + 1] = {0};
    char pdp_type_str[16] = "IPV4V6";
    char ping_host[128] = {0};
    int opt;

    while ((opt = getopt(argc, argv, "d:b:c:a:u:p:t:s:h")) != -1) {
        switch (opt) {
        case 'd':
            dev_path = optarg;
            break;
        case 'b':
            baud = (uint32_t)atoi(optarg);
            break;
        case 'c':
            cid = (uint8_t)atoi(optarg);
            break;
        case 'a':
            strncpy(apn, optarg, sizeof(apn) - 1);
            break;
        case 'u':
            strncpy(user, optarg, sizeof(user) - 1);
            break;
        case 'p':
            strncpy(pass, optarg, sizeof(pass) - 1);
            break;
        case 't':
            strncpy(pdp_type_str, optarg, sizeof(pdp_type_str) - 1);
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return (opt == 'h') ? 0 : -1;
        }
    }

    if (strcmp(dev_path, "auto") == 0)
        printf("AT dev: auto (driver auto-detect), baud: %u\n", baud);
    else
        printf("AT dev: %s, baud: %u\n", dev_path, baud);

    struct modem_5g_dev *dev = modem_5g_alloc_uart("MR880A:mr880a0", dev_path, baud);
    if (!dev) {
        printf("Failed to allocate modem\n");
        return -1;
    }

    if (modem_5g_init(dev) != MODEM_5G_STATUS_SUCCESS) {
        printf("Init failed\n");
        modem_5g_free(dev);
        return -1;
    }

    struct modem_5g_basic_info basic;
    struct modem_5g_sim_info sim;
    struct modem_5g_reg_info reg;
    struct modem_5g_signal_info sig;

    if (modem_5g_get_basic_info(dev, &basic) == MODEM_5G_STATUS_SUCCESS)
        print_basic_info(&basic);
    if (modem_5g_get_sim_info(dev, &sim) == MODEM_5G_STATUS_SUCCESS)
        print_sim_info(&sim);
    if (modem_5g_get_reg_info(dev, &reg) == MODEM_5G_STATUS_SUCCESS)
        print_reg_info(&reg);
    if (modem_5g_get_signal_info(dev, &sig) == MODEM_5G_STATUS_SUCCESS)
        print_signal_info(&sig);

    if (apn[0] || user[0] || pass[0]) {
        struct modem_5g_pdp_context ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.cid = cid;
        ctx.pdp_type = parse_pdp_type(pdp_type_str);
        strncpy(ctx.apn, apn, sizeof(ctx.apn) - 1);
        strncpy(ctx.username, user, sizeof(ctx.username) - 1);
        strncpy(ctx.password, pass, sizeof(ctx.password) - 1);
        if (modem_5g_set_pdp_context(dev, &ctx) != MODEM_5G_STATUS_SUCCESS)
            printf("Set PDP context failed\n");
    }

    if (modem_5g_data_start(dev, cid) != MODEM_5G_STATUS_SUCCESS) {
        printf("Data start failed\n");
    } else {
        struct modem_5g_ip_info ip;
        enum modem_5g_data_state state;
        if (modem_5g_get_data_state(dev, &state) == MODEM_5G_STATUS_SUCCESS)
            printf("Data state: %d\n", (int)state);
        if (modem_5g_get_ip_info(dev, cid, &ip) == MODEM_5G_STATUS_SUCCESS) {
            printf("IP: %s, GW: %s\n", ip.ip, ip.gateway);
            printf("DNS1: %s, DNS2: %s\n", ip.dns1, ip.dns2);
        }

        char cmd[160];
        sleep(1);
        snprintf(cmd, sizeof(cmd), "ping -c 3 %s", PING_HOST);
        printf("Running: %s\n", cmd);
        (void)system(cmd);
    }

    /*if want to disable*/
    // sleep(1);
    // modem_5g_power_off(dev);
    // sleep(0.1);

    modem_5g_deinit(dev);
    modem_5g_free(dev);
    return 0;
}
