/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MODEM_5G_H  // NOLINT(build/header_guard)
#define MODEM_5G_H  // NOLINT(build/header_guard)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* constants */
#define MODEM_5G_MANUFACTURER_MAX_LEN  32
#define MODEM_5G_MODEL_MAX_LEN         32
#define MODEM_5G_REVISION_MAX_LEN      64
#define MODEM_5G_IMEI_LEN              15
#define MODEM_5G_IMSI_LEN              15
#define MODEM_5G_ICCID_LEN             20
#define MODEM_5G_MSISDN_MAX_LEN        16
#define MODEM_5G_OPERATOR_MAX_LEN      32
#define MODEM_5G_APN_MAX_LEN           64
#define MODEM_5G_USERNAME_MAX_LEN      32
#define MODEM_5G_PASSWORD_MAX_LEN      32
#define MODEM_5G_IP_MAX_LEN            46
#define MODEM_5G_BAND_MAX_LEN          16
#define MODEM_5G_MCC_MNC_LEN            3

/* status code */
enum modem_5g_status {
    MODEM_5G_STATUS_SUCCESS = 0,
    MODEM_5G_STATUS_FAIL = -1,
    MODEM_5G_STATUS_NOT_READY = -2,
    MODEM_5G_STATUS_NOMEM = -3,
    MODEM_5G_STATUS_BUSY = -4,
    MODEM_5G_STATUS_UNSUPPORTED = -5,
    MODEM_5G_STATUS_INVALID = -6,
    MODEM_5G_STATUS_TIMEOUT = -7,
};

/* power state */
enum modem_5g_power_state {
    MODEM_5G_POWER_OFF = 0,
    MODEM_5G_POWER_ON,
    MODEM_5G_POWER_RESETTING,
};

/* SIM state */
enum modem_5g_sim_state {
    MODEM_5G_SIM_UNKNOWN = 0,
    MODEM_5G_SIM_ABSENT,
    MODEM_5G_SIM_READY,
    MODEM_5G_SIM_PIN_REQUIRED,
    MODEM_5G_SIM_PUK_REQUIRED,
    MODEM_5G_SIM_ERROR,
};

/* registration state */
enum modem_5g_reg_state {
    MODEM_5G_REG_NOT_REGISTERED = 0,
    MODEM_5G_REG_REGISTERED_HOME,
    MODEM_5G_REG_SEARCHING,
    MODEM_5G_REG_DENIED,
    MODEM_5G_REG_UNKNOWN,
    MODEM_5G_REG_REGISTERED_ROAMING,
};

/* radio access technology */
enum modem_5g_rat {
    MODEM_5G_RAT_UNKNOWN = 0,
    MODEM_5G_RAT_NR5G_SA,
    MODEM_5G_RAT_NR5G_NSA,
    MODEM_5G_RAT_LTE,
    MODEM_5G_RAT_WCDMA,
    MODEM_5G_RAT_GSM,
};

/* PDP type */
enum modem_5g_pdp_type {
    MODEM_5G_PDP_UNKNOWN = 0,
    MODEM_5G_PDP_IPV4,
    MODEM_5G_PDP_IPV6,
    MODEM_5G_PDP_IPV4V6,
};

/* data call state */
enum modem_5g_data_state {
    MODEM_5G_DATA_DISCONNECTED = 0,
    MODEM_5G_DATA_CONNECTING,
    MODEM_5G_DATA_CONNECTED,
};

/* basic module info */
struct modem_5g_basic_info {
    char manufacturer[MODEM_5G_MANUFACTURER_MAX_LEN + 1];
    char model[MODEM_5G_MODEL_MAX_LEN + 1];
    char revision[MODEM_5G_REVISION_MAX_LEN + 1];
    char imei[MODEM_5G_IMEI_LEN + 1];
};

/* SIM info */
struct modem_5g_sim_info {
    enum modem_5g_sim_state state;
    char iccid[MODEM_5G_ICCID_LEN + 1];
    char imsi[MODEM_5G_IMSI_LEN + 1];
    char msisdn[MODEM_5G_MSISDN_MAX_LEN + 1];
};

/* signal info, unit in dBm or dB */
struct modem_5g_signal_info {
    int16_t rssi;
    int16_t rsrp;
    int16_t rsrq;
    int16_t sinr;
};

/* registration/cell info */
struct modem_5g_reg_info {
    enum modem_5g_reg_state state;
    enum modem_5g_rat rat;
    char mcc[MODEM_5G_MCC_MNC_LEN + 1];
    char mnc[MODEM_5G_MCC_MNC_LEN + 1];
    uint32_t tac;
    uint32_t cell_id;
    uint16_t pci;
    uint32_t arfcn;
    char band[MODEM_5G_BAND_MAX_LEN + 1];
    char operator_name[MODEM_5G_OPERATOR_MAX_LEN + 1];
};

/* PDP context */
struct modem_5g_pdp_context {
    uint8_t cid;
    enum modem_5g_pdp_type pdp_type;
    char apn[MODEM_5G_APN_MAX_LEN + 1];
    char username[MODEM_5G_USERNAME_MAX_LEN + 1];
    char password[MODEM_5G_PASSWORD_MAX_LEN + 1];
};

/* IP info */
struct modem_5g_ip_info {
    char ip[MODEM_5G_IP_MAX_LEN + 1];
    char gateway[MODEM_5G_IP_MAX_LEN + 1];
    char dns1[MODEM_5G_IP_MAX_LEN + 1];
    char dns2[MODEM_5G_IP_MAX_LEN + 1];
};

/* event ID */
enum modem_5g_event_id {
    MODEM_5G_EVENT_POWER = 0,
    MODEM_5G_EVENT_SIM,
    MODEM_5G_EVENT_REG,
    MODEM_5G_EVENT_SIGNAL,
    MODEM_5G_EVENT_DATA,
    MODEM_5G_EVENT_URC,
};

/* event data */
struct modem_5g_event {
    enum modem_5g_event_id id;
    union {
        enum modem_5g_power_state power_state;
        enum modem_5g_sim_state sim_state;
        struct modem_5g_reg_info reg_info;
        struct modem_5g_signal_info signal_info;
        enum modem_5g_data_state data_state;
    } data;
    void *private_data;
};

struct modem_5g_dev; /* opaque handle */

typedef void (*modem_5g_event_cb_t)(struct modem_5g_dev *dev,
    const struct modem_5g_event *event, void *ctx);

/* --- lifecycle --- */

struct modem_5g_dev *modem_5g_alloc_uart(const char *name,
    const char *uart_dev, uint32_t baud);

enum modem_5g_status modem_5g_init(struct modem_5g_dev *dev);
enum modem_5g_status modem_5g_deinit(struct modem_5g_dev *dev);
void modem_5g_free(struct modem_5g_dev *dev);

void modem_5g_set_event_cb(struct modem_5g_dev *dev,
    modem_5g_event_cb_t cb, void *ctx);

/* --- power control --- */

enum modem_5g_status modem_5g_power_on(struct modem_5g_dev *dev);
enum modem_5g_status modem_5g_power_off(struct modem_5g_dev *dev);
enum modem_5g_status modem_5g_reset(struct modem_5g_dev *dev);
enum modem_5g_status modem_5g_set_flight_mode(struct modem_5g_dev *dev,
    bool enable);
enum modem_5g_status modem_5g_get_power_state(struct modem_5g_dev *dev,
    enum modem_5g_power_state *state);

/* --- information --- */

enum modem_5g_status modem_5g_get_basic_info(struct modem_5g_dev *dev,
    struct modem_5g_basic_info *info);
enum modem_5g_status modem_5g_get_sim_info(struct modem_5g_dev *dev,
    struct modem_5g_sim_info *info);
enum modem_5g_status modem_5g_get_reg_info(struct modem_5g_dev *dev,
    struct modem_5g_reg_info *info);
enum modem_5g_status modem_5g_get_signal_info(struct modem_5g_dev *dev,
    struct modem_5g_signal_info *info);
enum modem_5g_status modem_5g_set_prefer_rat(struct modem_5g_dev *dev,
    enum modem_5g_rat rat);

/* --- data service --- */

enum modem_5g_status modem_5g_set_pdp_context(struct modem_5g_dev *dev,
    const struct modem_5g_pdp_context *ctx);
enum modem_5g_status modem_5g_get_pdp_context(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_pdp_context *ctx);
enum modem_5g_status modem_5g_data_start(struct modem_5g_dev *dev, uint8_t cid);
enum modem_5g_status modem_5g_data_stop(struct modem_5g_dev *dev, uint8_t cid);
enum modem_5g_status modem_5g_get_data_state(struct modem_5g_dev *dev,
    enum modem_5g_data_state *state);
enum modem_5g_status modem_5g_get_ip_info(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_ip_info *info);

/* --- AT passthrough --- */

enum modem_5g_status modem_5g_send_at(struct modem_5g_dev *dev,
    const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MODEM_5G_H */
