/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "modem_5g_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct modem_5g_dev *modem_5g_dev_alloc(const char *name, size_t priv_size)
{
    struct modem_5g_dev *dev;
    void *priv = NULL;
    char *name_copy = NULL;

    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;

    if (priv_size) {
        priv = calloc(1, priv_size);
        if (!priv) {
            free(dev);
            return NULL;
        }
        dev->priv_data = priv;
    }

    if (name) {
        size_t n = strlen(name);
        name_copy = calloc(1, n + 1);
        if (!name_copy) {
            free(priv);
            free(dev);
            return NULL;
        }
        memcpy(name_copy, name, n);
        name_copy[n] = '\0';
        dev->name = name_copy;
    }

    return dev;
}

enum modem_5g_status modem_5g_init(struct modem_5g_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->init)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->init(dev);
}

enum modem_5g_status modem_5g_deinit(struct modem_5g_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->deinit)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->deinit(dev);
}

void modem_5g_free(struct modem_5g_dev *dev)
{
    if (!dev)
        return;

    if (dev->ops && dev->ops->free) {
        dev->ops->free(dev);
        return;
    }

    if (dev->priv_data)
        free(dev->priv_data);
    if (dev->name)
        free((void *)dev->name);
    free(dev);
}

void modem_5g_set_event_cb(struct modem_5g_dev *dev,
    modem_5g_event_cb_t cb, void *ctx)
{
    if (dev) {
        dev->cb = cb;
        dev->cb_ctx = ctx;
    }
}

enum modem_5g_status modem_5g_power_on(struct modem_5g_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->power_on)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->power_on(dev);
}

enum modem_5g_status modem_5g_power_off(struct modem_5g_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->power_off)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->power_off(dev);
}

enum modem_5g_status modem_5g_reset(struct modem_5g_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->reset)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->reset(dev);
}

enum modem_5g_status modem_5g_set_flight_mode(struct modem_5g_dev *dev,
    bool enable)
{
    if (!dev || !dev->ops || !dev->ops->set_flight_mode)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->set_flight_mode(dev, enable);
}

enum modem_5g_status modem_5g_get_power_state(struct modem_5g_dev *dev,
    enum modem_5g_power_state *state)
{
    if (!dev || !dev->ops || !dev->ops->get_power_state)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_power_state(dev, state);
}

enum modem_5g_status modem_5g_get_basic_info(struct modem_5g_dev *dev,
    struct modem_5g_basic_info *info)
{
    if (!dev || !dev->ops || !dev->ops->get_basic_info || !info)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_basic_info(dev, info);
}

enum modem_5g_status modem_5g_get_sim_info(struct modem_5g_dev *dev,
    struct modem_5g_sim_info *info)
{
    if (!dev || !dev->ops || !dev->ops->get_sim_info || !info)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_sim_info(dev, info);
}

enum modem_5g_status modem_5g_get_reg_info(struct modem_5g_dev *dev,
    struct modem_5g_reg_info *info)
{
    if (!dev || !dev->ops || !dev->ops->get_reg_info || !info)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_reg_info(dev, info);
}

enum modem_5g_status modem_5g_get_signal_info(struct modem_5g_dev *dev,
    struct modem_5g_signal_info *info)
{
    if (!dev || !dev->ops || !dev->ops->get_signal_info || !info)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_signal_info(dev, info);
}

enum modem_5g_status modem_5g_set_prefer_rat(struct modem_5g_dev *dev,
    enum modem_5g_rat rat)
{
    if (!dev || !dev->ops || !dev->ops->set_prefer_rat)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->set_prefer_rat(dev, rat);
}

enum modem_5g_status modem_5g_set_pdp_context(struct modem_5g_dev *dev,
    const struct modem_5g_pdp_context *ctx)
{
    if (!dev || !dev->ops || !dev->ops->set_pdp_context || !ctx)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->set_pdp_context(dev, ctx);
}

enum modem_5g_status modem_5g_get_pdp_context(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_pdp_context *ctx)
{
    if (!dev || !dev->ops || !dev->ops->get_pdp_context || !ctx)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_pdp_context(dev, cid, ctx);
}

enum modem_5g_status modem_5g_data_start(struct modem_5g_dev *dev, uint8_t cid)
{
    if (!dev || !dev->ops || !dev->ops->data_start)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->data_start(dev, cid);
}

enum modem_5g_status modem_5g_data_stop(struct modem_5g_dev *dev, uint8_t cid)
{
    if (!dev || !dev->ops || !dev->ops->data_stop)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->data_stop(dev, cid);
}

enum modem_5g_status modem_5g_get_data_state(struct modem_5g_dev *dev,
    enum modem_5g_data_state *state)
{
    if (!dev || !dev->ops || !dev->ops->get_data_state || !state)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_data_state(dev, state);
}

enum modem_5g_status modem_5g_get_ip_info(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_ip_info *info)
{
    if (!dev || !dev->ops || !dev->ops->get_ip_info || !info)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->get_ip_info(dev, cid, info);
}

enum modem_5g_status modem_5g_send_at(struct modem_5g_dev *dev,
    const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    if (!dev || !dev->ops || !dev->ops->send_at || !cmd)
        return MODEM_5G_STATUS_INVALID;

    return dev->ops->send_at(dev, cmd, resp, resp_len, timeout_ms);
}

/* --- driver registry --- */

static struct driver_info *g_driver_list = NULL;

void modem_5g_driver_register(struct driver_info *info)
{
    if (!info)
        return;
    info->next = g_driver_list;
    g_driver_list = info;
}

static struct driver_info *find_driver(const char *name,
    enum modem_5g_driver_type type)
{
    struct driver_info *curr = g_driver_list;
    while (curr) {
        if (curr->name && name && strcmp(curr->name, name) == 0) {
            if (curr->type == type)
                return curr;
            printf("[5G] driver '%s' type mismatch (expected %d got %d)\n",
                name, (int)type, (int)curr->type);
            return NULL;
        }
        curr = curr->next;
    }
    return NULL;
}

static int split_driver_instance(const char *name,
    char *driver, size_t driver_sz,
    const char **instance)
{
    const char *sep;
    size_t len;

    if (!name || !driver || !driver_sz || !instance)
        return -1;

    sep = strchr(name, ':');
    if (!sep)
        return 0;

    len = (size_t)(sep - name);
    if (len == 0 || len + 1 > driver_sz || !*(sep + 1))
        return -1;

    memcpy(driver, name, len);
    driver[len] = '\0';
    *instance = sep + 1;
    return 1;
}

struct modem_5g_dev *modem_5g_alloc_uart(const char *name,
    const char *uart_dev, uint32_t baud)
{
    struct driver_info *drv;
    struct modem_5g_args_uart args;
    char driver[64];
    const char *instance = NULL;
    int r;

    if (!name)
        return NULL;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    if (r == 0) {
        strncpy(driver, name, sizeof(driver) - 1);
        driver[sizeof(driver) - 1] = '\0';
        instance = name;
        drv = find_driver(driver, MODEM_5G_DRV_UART);
        if (!drv) {
            strncpy(driver, "MR880A", sizeof(driver) - 1);
            driver[sizeof(driver) - 1] = '\0';
            instance = name;
        }
    }

    drv = find_driver(driver, MODEM_5G_DRV_UART);
    if (!drv || !drv->factory) {
        printf("[5G] driver '%s' not found\n", driver);
        return NULL;
    }

    args.instance = instance;
    args.dev_path = uart_dev;
    args.baud = baud;
    return drv->factory(&args);
}
