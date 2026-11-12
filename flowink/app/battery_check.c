#include "battery_check.h"
#include <zephyr/kernel.h>
#include <stdbool.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include "show_picture.h"
#include "pwr_manage.h"

LOG_MODULE_REGISTER(battery_check, LOG_LEVEL_DBG);

#define BATTERY_CHECK_NODE DT_PATH(zephyr_user)
#define BATTERY_CHECK_BUF_SIZE 5

static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(BATTERY_CHECK_NODE);
static const struct gpio_dt_spec sel_gpio = GPIO_DT_SPEC_GET(BATTERY_CHECK_NODE, adc_sel_gpios);

static void battery_check_init(void)
{
    int err;

    if (!gpio_is_ready_dt(&sel_gpio) || !adc_is_ready_dt(&adc_channel))
    {
        LOG_ERR("Battery check module is not ready");
        return;
    }

    err = gpio_pin_configure_dt(&sel_gpio, GPIO_OUTPUT_ACTIVE);
    if (err < 0)
    {
        LOG_ERR("Failed to configure SEL GPIO: %d", err);
    }
}

static bool battery_voltage_below_threshold(void)
{
    uint8_t valid_cnt = 0;
    int err;
    int32_t value = 0, average = 0;
    struct adc_sequence sequence = {
        .buffer = &value,
        .buffer_size = sizeof(value),
    };

    err = adc_sequence_init_dt(&adc_channel, &sequence);
    if (err < 0)
    {
        LOG_ERR("Failed to initialize ADC sequence: %d", err);
        return false;
    }

    for (int i = 0; i < BATTERY_CHECK_BUF_SIZE; i++)
    {
        err = adc_read_dt(&adc_channel, &sequence);
        if (err < 0)
        {
            LOG_ERR("Failed to read ADC: %d", err);
            continue;
        }

        err = adc_raw_to_millivolts_dt(&adc_channel, &value);
        if (err < 0)
        {
            LOG_ERR("Failed to convert ADC raw value to millivolts: %d", err);
            continue;
        }

        average += value;
        valid_cnt++;
    }

    average /= valid_cnt;
    LOG_DBG("Average battery voltage: %d mV", average);

    return average < CONFIG_BATTERY_ALARM_THRESHOLD_MV;
}

static bool is_battery_low(void)
{
    if (!battery_voltage_below_threshold())
    {
        return false;
    }

    k_sleep(K_MSEC(100));
    return battery_voltage_below_threshold();
}

void battery_check(void)
{
    battery_check_init();

    gpio_pin_set_dt(&sel_gpio, 0);
    k_sleep(K_MSEC(10));

    if (is_battery_low())
    {
        LOG_ERR("Battery voltage is low");
        show_pic_battery_alarm();
        pwr_enter_sleep(false);
        gpio_pin_set_dt(&sel_gpio, 1);
    }

    gpio_pin_set_dt(&sel_gpio, 1);
}
