/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MODEM_5G_CORE_H
#define MODEM_5G_CORE_H

/*
 * Private header for 5G modem component.
 */

#include <stddef.h>

#include "../include/5g.h"

struct modem_5g_args_uart {
    const char *instance;
    const char *dev_path;
    uint32_t baud;
};

enum modem_5g_driver_type {
    MODEM_5G_DRV_UART = 0,
};

struct modem_5g_ops {
    enum modem_5g_status (*init)(struct modem_5g_dev *dev);
    enum modem_5g_status (*deinit)(struct modem_5g_dev *dev);
    enum modem_5g_status (*power_on)(struct modem_5g_dev *dev);
    enum modem_5g_status (*power_off)(struct modem_5g_dev *dev);
    enum modem_5g_status (*reset)(struct modem_5g_dev *dev);
    enum modem_5g_status (*set_flight_mode)(struct modem_5g_dev *dev, bool enable);
    enum modem_5g_status (*get_power_state)(struct modem_5g_dev *dev,
        enum modem_5g_power_state *state);

    enum modem_5g_status (*get_basic_info)(struct modem_5g_dev *dev,
        struct modem_5g_basic_info *info);
    enum modem_5g_status (*get_sim_info)(struct modem_5g_dev *dev,
        struct modem_5g_sim_info *info);
    enum modem_5g_status (*get_reg_info)(struct modem_5g_dev *dev,
        struct modem_5g_reg_info *info);
    enum modem_5g_status (*get_signal_info)(struct modem_5g_dev *dev,
        struct modem_5g_signal_info *info);
    enum modem_5g_status (*set_prefer_rat)(struct modem_5g_dev *dev,
        enum modem_5g_rat rat);

    enum modem_5g_status (*set_pdp_context)(struct modem_5g_dev *dev,
        const struct modem_5g_pdp_context *ctx);
    enum modem_5g_status (*get_pdp_context)(struct modem_5g_dev *dev,
        uint8_t cid, struct modem_5g_pdp_context *ctx);
    enum modem_5g_status (*data_start)(struct modem_5g_dev *dev, uint8_t cid);
    enum modem_5g_status (*data_stop)(struct modem_5g_dev *dev, uint8_t cid);
    enum modem_5g_status (*get_data_state)(struct modem_5g_dev *dev,
        enum modem_5g_data_state *state);
    enum modem_5g_status (*get_ip_info)(struct modem_5g_dev *dev,
        uint8_t cid, struct modem_5g_ip_info *info);

    enum modem_5g_status (*send_at)(struct modem_5g_dev *dev,
        const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms);

    void (*free)(struct modem_5g_dev *dev);
};

struct modem_5g_dev {
    const char *name; /* instance name */
    const struct modem_5g_ops *ops;
    void *priv_data;
    modem_5g_event_cb_t cb;
    void *cb_ctx;
};

typedef struct modem_5g_dev *(*modem_5g_factory_t)(void *args);

struct driver_info {
    const char *name;
    enum modem_5g_driver_type type;
    modem_5g_factory_t factory;
    struct driver_info *next;
};

void modem_5g_driver_register(struct driver_info *info);

#define REGISTER_MODEM_5G_DRIVER(_name, _type, _factory) \
    static struct driver_info __drv_info_##_factory = { \
        .name = _name, \
        .type = _type, \
        .factory = _factory, \
        .next = 0 \
    }; \
    __attribute__((constructor)) \
    static void __auto_reg_##_factory(void) { \
        modem_5g_driver_register(&__drv_info_##_factory); \
    }

struct modem_5g_dev *modem_5g_dev_alloc(const char *instance, size_t priv_size);

#endif /* MODEM_5G_CORE_H */
