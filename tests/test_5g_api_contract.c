/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modem_5g_core.h"

struct mock_5g_priv {
    int initialized;
    enum modem_5g_power_state power_state;
    enum modem_5g_data_state data_state;
    struct modem_5g_pdp_context pdp;
};

static int g_failures;
static int g_free_count;
static int g_event_count;
static struct modem_5g_event g_last_event;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define CHECK_INT_EQ(actual, expected) do { \
    int _actual = (int)(actual); \
    int _expected = (int)(expected); \
    if (_actual != _expected) { \
        printf("FAIL:%s:%d: expected %s == %d, got %d\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

#define CHECK_STR_EQ(actual, expected) do { \
    const char *_actual = (actual); \
    const char *_expected = (expected); \
    if (strcmp(_actual, _expected) != 0) { \
        printf("FAIL:%s:%d: expected %s == '%s', got '%s'\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

static void reset_test_state(void)
{
    g_failures = 0;
    g_free_count = 0;
    g_event_count = 0;
    memset(&g_last_event, 0, sizeof(g_last_event));
}

static void emit_event(struct modem_5g_dev *dev,
    enum modem_5g_event_id id, int value)
{
    struct modem_5g_event event;

    memset(&event, 0, sizeof(event));
    event.id = id;
    if (id == MODEM_5G_EVENT_POWER)
        event.data.power_state = (enum modem_5g_power_state)value;
    if (id == MODEM_5G_EVENT_DATA)
        event.data.data_state = (enum modem_5g_data_state)value;
    if (dev->cb)
        dev->cb(dev, &event, dev->cb_ctx);
}

static void event_cb(struct modem_5g_dev *dev,
    const struct modem_5g_event *event, void *ctx)
{
    int *seen = ctx;

    CHECK_TRUE(dev != NULL);
    CHECK_TRUE(event != NULL);
    CHECK_TRUE(seen != NULL);

    if (seen)
        (*seen)++;
    if (event) {
        g_last_event = *event;
        g_event_count++;
    }
}

static enum modem_5g_status mock_init(struct modem_5g_dev *dev)
{
    struct mock_5g_priv *priv;

    if (!dev || !dev->priv_data)
        return MODEM_5G_STATUS_INVALID;

    priv = dev->priv_data;
    priv->initialized = 1;
    priv->power_state = MODEM_5G_POWER_OFF;
    priv->data_state = MODEM_5G_DATA_DISCONNECTED;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_deinit(struct modem_5g_dev *dev)
{
    struct mock_5g_priv *priv;

    if (!dev || !dev->priv_data)
        return MODEM_5G_STATUS_INVALID;

    priv = dev->priv_data;
    priv->initialized = 0;
    priv->data_state = MODEM_5G_DATA_DISCONNECTED;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status require_ready(struct modem_5g_dev *dev,
    struct mock_5g_priv **out_priv)
{
    struct mock_5g_priv *priv;

    if (!dev || !dev->priv_data)
        return MODEM_5G_STATUS_INVALID;

    priv = dev->priv_data;
    if (!priv->initialized)
        return MODEM_5G_STATUS_NOT_READY;

    if (out_priv)
        *out_priv = priv;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_power_on(struct modem_5g_dev *dev)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;

    priv->power_state = MODEM_5G_POWER_ON;
    emit_event(dev, MODEM_5G_EVENT_POWER, MODEM_5G_POWER_ON);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_power_off(struct modem_5g_dev *dev)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;

    priv->power_state = MODEM_5G_POWER_OFF;
    emit_event(dev, MODEM_5G_EVENT_POWER, MODEM_5G_POWER_OFF);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_reset(struct modem_5g_dev *dev)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;

    priv->power_state = MODEM_5G_POWER_RESETTING;
    emit_event(dev, MODEM_5G_EVENT_POWER, MODEM_5G_POWER_RESETTING);
    priv->power_state = MODEM_5G_POWER_ON;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_set_flight_mode(struct modem_5g_dev *dev,
    bool enable)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;

    priv->power_state = enable ? MODEM_5G_POWER_OFF : MODEM_5G_POWER_ON;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_power_state(struct modem_5g_dev *dev,
    enum modem_5g_power_state *state)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!state)
        return MODEM_5G_STATUS_INVALID;

    *state = priv->power_state;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_basic_info(struct modem_5g_dev *dev,
    struct modem_5g_basic_info *info)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    strcpy(info->manufacturer, "MockVendor");
    strcpy(info->model, "MockMR");
    strcpy(info->revision, "1.2.3");
    strcpy(info->imei, "123456789012345");
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_sim_info(struct modem_5g_dev *dev,
    struct modem_5g_sim_info *info)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_SIM_READY;
    strcpy(info->iccid, "89860000000000000001");
    strcpy(info->imsi, "460001234567890");
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_reg_info(struct modem_5g_dev *dev,
    struct modem_5g_reg_info *info)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!info)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    info->state = MODEM_5G_REG_REGISTERED_HOME;
    info->rat = MODEM_5G_RAT_NR5G_SA;
    strcpy(info->operator_name, "MockCarrier");
    strcpy(info->mcc, "460");
    strcpy(info->mnc, "001");
    info->cell_id = 42;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_signal_info(struct modem_5g_dev *dev,
    struct modem_5g_signal_info *info)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!info)
        return MODEM_5G_STATUS_INVALID;

    info->rssi = -61;
    info->rsrp = -88;
    info->rsrq = -9;
    info->sinr = 18;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_set_prefer_rat(struct modem_5g_dev *dev,
    enum modem_5g_rat rat)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (rat == MODEM_5G_RAT_UNKNOWN)
        return MODEM_5G_STATUS_INVALID;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_set_pdp_context(struct modem_5g_dev *dev,
    const struct modem_5g_pdp_context *ctx)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!ctx || ctx->cid == 0)
        return MODEM_5G_STATUS_INVALID;

    priv->pdp = *ctx;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_pdp_context(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_pdp_context *ctx)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!ctx || cid == 0 || cid != priv->pdp.cid)
        return MODEM_5G_STATUS_INVALID;

    *ctx = priv->pdp;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_data_start(struct modem_5g_dev *dev, uint8_t cid)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (cid == 0)
        return MODEM_5G_STATUS_INVALID;

    priv->data_state = MODEM_5G_DATA_CONNECTED;
    emit_event(dev, MODEM_5G_EVENT_DATA, MODEM_5G_DATA_CONNECTED);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_data_stop(struct modem_5g_dev *dev, uint8_t cid)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (cid == 0)
        return MODEM_5G_STATUS_INVALID;

    priv->data_state = MODEM_5G_DATA_DISCONNECTED;
    emit_event(dev, MODEM_5G_EVENT_DATA, MODEM_5G_DATA_DISCONNECTED);
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_data_state(struct modem_5g_dev *dev,
    enum modem_5g_data_state *state)
{
    struct mock_5g_priv *priv;
    enum modem_5g_status ret = require_ready(dev, &priv);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!state)
        return MODEM_5G_STATUS_INVALID;

    *state = priv->data_state;
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_get_ip_info(struct modem_5g_dev *dev,
    uint8_t cid, struct modem_5g_ip_info *info)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!info || cid == 0)
        return MODEM_5G_STATUS_INVALID;

    memset(info, 0, sizeof(*info));
    strcpy(info->ip, "10.0.0.2");
    strcpy(info->gateway, "10.0.0.1");
    strcpy(info->dns1, "8.8.8.8");
    return MODEM_5G_STATUS_SUCCESS;
}

static enum modem_5g_status mock_send_at(struct modem_5g_dev *dev,
    const char *cmd, char *resp, size_t resp_len, uint32_t timeout_ms)
{
    enum modem_5g_status ret = require_ready(dev, NULL);

    (void)timeout_ms;

    if (ret != MODEM_5G_STATUS_SUCCESS)
        return ret;
    if (!cmd || strncmp(cmd, "AT", 2) != 0)
        return MODEM_5G_STATUS_INVALID;

    if (resp && resp_len > 0) {
        strncpy(resp, "OK", resp_len - 1);
        resp[resp_len - 1] = '\0';
    }
    return MODEM_5G_STATUS_SUCCESS;
}

static void mock_free(struct modem_5g_dev *dev)
{
    if (!dev)
        return;
    g_free_count++;
    free(dev->priv_data);
    free((void *)dev->name);
    free(dev);
}

static const struct modem_5g_ops mock_ops = {
    .init = mock_init,
    .deinit = mock_deinit,
    .power_on = mock_power_on,
    .power_off = mock_power_off,
    .reset = mock_reset,
    .set_flight_mode = mock_set_flight_mode,
    .get_power_state = mock_get_power_state,
    .get_basic_info = mock_get_basic_info,
    .get_sim_info = mock_get_sim_info,
    .get_reg_info = mock_get_reg_info,
    .get_signal_info = mock_get_signal_info,
    .set_prefer_rat = mock_set_prefer_rat,
    .set_pdp_context = mock_set_pdp_context,
    .get_pdp_context = mock_get_pdp_context,
    .data_start = mock_data_start,
    .data_stop = mock_data_stop,
    .get_data_state = mock_get_data_state,
    .get_ip_info = mock_get_ip_info,
    .send_at = mock_send_at,
    .free = mock_free,
};

static struct modem_5g_dev *mock_factory(void *args)
{
    struct modem_5g_args_uart *uart_args = args;
    struct modem_5g_dev *dev;

    if (!uart_args || !uart_args->instance || !uart_args->dev_path)
        return NULL;

    dev = modem_5g_dev_alloc(uart_args->instance, sizeof(struct mock_5g_priv));
    if (!dev)
        return NULL;

    dev->ops = &mock_ops;
    return dev;
}

REGISTER_MODEM_5G_DRIVER("MOCK", MODEM_5G_DRV_UART, mock_factory);

static void test_error_paths(void)
{
    struct modem_5g_dev *dev;
    struct modem_5g_basic_info basic;
    struct modem_5g_pdp_context pdp;
    char resp[8];

    CHECK_INT_EQ(modem_5g_init(NULL), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_power_on(NULL), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_get_basic_info(NULL, &basic), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_send_at(NULL, "AT", resp, sizeof(resp), 10),
        MODEM_5G_STATUS_INVALID);

    CHECK_TRUE(modem_5g_alloc_uart(NULL, "mock://modem", 115200) == NULL);
    CHECK_TRUE(modem_5g_alloc_uart("MOCK:", "mock://modem", 115200) == NULL);
    CHECK_TRUE(modem_5g_alloc_uart(":m0", "mock://modem", 115200) == NULL);
    CHECK_TRUE(modem_5g_alloc_uart("MISSING:m0", "mock://modem", 115200) == NULL);
    CHECK_TRUE(modem_5g_alloc_uart("MOCK:m0", NULL, 115200) == NULL);

    dev = modem_5g_alloc_uart("MOCK:not-ready", "mock://modem", 115200);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    CHECK_INT_EQ(modem_5g_power_on(dev), MODEM_5G_STATUS_NOT_READY);
    CHECK_INT_EQ(modem_5g_get_basic_info(dev, &basic), MODEM_5G_STATUS_NOT_READY);
    CHECK_INT_EQ(modem_5g_set_pdp_context(dev, &pdp), MODEM_5G_STATUS_NOT_READY);
    CHECK_INT_EQ(modem_5g_init(dev), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(modem_5g_get_basic_info(dev, NULL), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_set_prefer_rat(dev, MODEM_5G_RAT_UNKNOWN),
        MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_set_pdp_context(dev, NULL), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_data_start(dev, 0), MODEM_5G_STATUS_INVALID);
    CHECK_INT_EQ(modem_5g_send_at(dev, "BAD", resp, sizeof(resp), 10),
        MODEM_5G_STATUS_INVALID);
    modem_5g_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_functional(void)
{
    struct modem_5g_dev *dev;
    struct modem_5g_basic_info basic;
    struct modem_5g_sim_info sim;
    struct modem_5g_reg_info reg;
    struct modem_5g_signal_info signal;
    struct modem_5g_pdp_context pdp;
    struct modem_5g_pdp_context pdp_out;
    struct modem_5g_ip_info ip;
    enum modem_5g_power_state power_state;
    enum modem_5g_data_state data_state;
    char resp[8];
    int local_events = 0;

    dev = modem_5g_alloc_uart("MOCK:m0", "mock://modem", 115200);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    modem_5g_set_event_cb(dev, event_cb, &local_events);
    CHECK_INT_EQ(modem_5g_init(dev), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(modem_5g_power_on(dev), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(local_events, 1);
    CHECK_INT_EQ(g_last_event.id, MODEM_5G_EVENT_POWER);
    CHECK_INT_EQ(g_last_event.data.power_state, MODEM_5G_POWER_ON);
    CHECK_INT_EQ(modem_5g_get_power_state(dev, &power_state), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(power_state, MODEM_5G_POWER_ON);

    CHECK_INT_EQ(modem_5g_get_basic_info(dev, &basic), MODEM_5G_STATUS_SUCCESS);
    CHECK_STR_EQ(basic.manufacturer, "MockVendor");
    CHECK_STR_EQ(basic.imei, "123456789012345");
    CHECK_INT_EQ(modem_5g_get_sim_info(dev, &sim), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(sim.state, MODEM_5G_SIM_READY);
    CHECK_INT_EQ(modem_5g_get_reg_info(dev, &reg), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(reg.rat, MODEM_5G_RAT_NR5G_SA);
    CHECK_INT_EQ(modem_5g_get_signal_info(dev, &signal), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(signal.sinr, 18);

    memset(&pdp, 0, sizeof(pdp));
    pdp.cid = 1;
    pdp.pdp_type = MODEM_5G_PDP_IPV4V6;
    strcpy(pdp.apn, "internet");
    CHECK_INT_EQ(modem_5g_set_pdp_context(dev, &pdp), MODEM_5G_STATUS_SUCCESS);
    memset(&pdp_out, 0, sizeof(pdp_out));
    CHECK_INT_EQ(modem_5g_get_pdp_context(dev, 1, &pdp_out), MODEM_5G_STATUS_SUCCESS);
    CHECK_STR_EQ(pdp_out.apn, "internet");

    CHECK_INT_EQ(modem_5g_data_start(dev, 1), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(modem_5g_get_data_state(dev, &data_state), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(data_state, MODEM_5G_DATA_CONNECTED);
    CHECK_INT_EQ(modem_5g_get_ip_info(dev, 1, &ip), MODEM_5G_STATUS_SUCCESS);
    CHECK_STR_EQ(ip.ip, "10.0.0.2");
    CHECK_INT_EQ(modem_5g_send_at(dev, "AT", resp, sizeof(resp), 10),
        MODEM_5G_STATUS_SUCCESS);
    CHECK_STR_EQ(resp, "OK");
    CHECK_INT_EQ(modem_5g_data_stop(dev, 1), MODEM_5G_STATUS_SUCCESS);
    CHECK_INT_EQ(modem_5g_deinit(dev), MODEM_5G_STATUS_SUCCESS);

    modem_5g_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
    CHECK_TRUE(g_event_count >= 3);
}

static int finish_test(const char *name)
{
    if (g_failures != 0) {
        printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    printf("%s PASSED\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "all";

    if (strcmp(mode, "functional") == 0) {
        reset_test_state();
        test_functional();
        return finish_test("5g api functional test");
    }
    if (strcmp(mode, "error-paths") == 0) {
        reset_test_state();
        test_error_paths();
        return finish_test("5g api error paths test");
    }
    if (strcmp(mode, "all") == 0) {
        reset_test_state();
        test_functional();
        if (finish_test("5g api functional test") != 0)
            return 1;
        reset_test_state();
        test_error_paths();
        if (finish_test("5g api error paths test") != 0)
            return 1;
        printf("5g api contract test PASSED\n");
        return 0;
    }

    fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
