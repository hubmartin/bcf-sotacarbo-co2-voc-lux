#include <application.h>

#define SERVICE_INTERVAL_INTERVAL (60 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.2f
#define TEMPERATURE_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define TEMPERATURE_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 5.0f
#define HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL (5 * 1000)
#define HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL (10 * 1000)

#define BAROMETER_TAG_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define BAROMETER_TAG_PUB_VALUE_CHANGE 20.0f
#define BAROMETER_TAG_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define BAROMETER_TAG_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

#define CO2_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define CO2_PUB_VALUE_CHANGE 50.0f
#define CO2_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define CO2_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

#define CALIBRATION_DELAY (10 * 60 * 1000)

#define VOC_TAG_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define VOC_TAG_PUB_VALUE_CHANGE 5.0f
#define VOC_TAG_UPDATE_SERVICE_INTERVAL (1 * 60 * 1000)
#define VOC_TAG_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

#define LUX_METER_TAG_PUB_NO_CHANGE_INTERVAL (15 * 60 * 1000)
#define LUX_METER_TAG_PUB_VALUE_CHANGE 10.0f
#define LUX_METER_TAG_UPDATE_SERVICE_INTERVAL (1 * 30 * 1000)
#define LUX_METER_TAG_UPDATE_NORMAL_INTERVAL  (5 * 60 * 1000)

// LED instance
bc_led_t led;

// Button instance
bc_button_t button;

// Thermometer instance
bc_tmp112_t tmp112;

// Temperature tag instance
bc_tag_temperature_t temperature;
event_param_t temperature_event_param = { .next_pub = 0 };

// Humidity tag instance
bc_tag_humidity_t humidity;
event_param_t humidity_event_param = { .next_pub = 0 };

// Barometer tag instance
bc_tag_barometer_t barometer;
event_param_t barometer_event_param = { .next_pub = 0 };

// CO2
event_param_t co2_event_param = { .next_pub = 0 };

// VOC tag instance
bc_tag_voc_t tag_voc;
event_param_t voc_event_param = { .next_pub = 0 };

// Luxmeter tag instance
bc_tag_lux_meter_t lux_meter;
event_param_t lux_meter_event_param = { .next_pub = 0 };


void calibration_task(void *param)
{
    (void) param;

    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_module_co2_calibration(BC_LP8_CALIBRATION_BACKGROUND_FILTERED);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_led_pulse(&led, 100);

        static uint16_t event_count = 0;
        bc_radio_pub_push_button(&event_count);
        event_count++;
    }
    else if (event == BC_BUTTON_EVENT_HOLD)
    {
        bc_led_set_mode(&led, BC_LED_MODE_BLINK);

        bc_scheduler_register(calibration_task, NULL, bc_tick_get() + CALIBRATION_DELAY);
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}

void temperature_tag_event_handler(bc_tag_temperature_t *self, bc_tag_temperature_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TAG_TEMPERATURE_EVENT_UPDATE)
    {
        if (bc_tag_temperature_get_temperature_celsius(self, &value))
        {
            if ((fabsf(value - param->value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_temperature(param->channel, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void humidity_tag_event_handler(bc_tag_humidity_t *self, bc_tag_humidity_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_HUMIDITY_EVENT_UPDATE)
    {
        return;
    }

    if (bc_tag_humidity_get_humidity_percentage(self, &value))
    {
        if ((fabsf(value - param->value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
        {
            bc_radio_pub_humidity(param->channel, &value);
            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTERVAL;
        }
    }
}

void barometer_tag_event_handler(bc_tag_barometer_t *self, bc_tag_barometer_event_t event, void *event_param)
{
    float pascal;
    float meter;
    event_param_t *param = (event_param_t *)event_param;

    if (event != BC_TAG_BAROMETER_EVENT_UPDATE)
    {
        return;
    }

    if (!bc_tag_barometer_get_pressure_pascal(self, &pascal))
    {
        return;
    }

    if ((fabsf(pascal - param->value) >= BAROMETER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
    {

        if (!bc_tag_barometer_get_altitude_meter(self, &meter))
        {
            return;
        }

        bc_radio_pub_barometer(param->channel, &pascal, &meter);
        param->value = pascal;
        param->next_pub = bc_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTERVAL;
    }
}

void co2_event_handler(bc_module_co2_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    float value;

    if (event == BC_MODULE_CO2_EVENT_UPDATE)
    {
        if (bc_module_co2_get_concentration_ppm(&value))
        {
            if ((fabsf(value - param->value) >= CO2_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_co2(&value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + CO2_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void voc_tag_event_handler(bc_tag_voc_t *self, bc_tag_voc_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *) event_param;
    // set to zero because bc_tag_voc_get_tvoc_ppb returns only 2 bytes but bc_radio_pub_int sends 4 byte integer
    int value = 0;

    if (event == BC_TAG_VOC_EVENT_UPDATE)
    {
        if (bc_tag_voc_get_tvoc_ppb(&tag_voc, (uint16_t*)&value))
        {
            if ((fabsf(value - param->value) >= VOC_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_int("voc-sensor/0:0/tvoc", &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + VOC_TAG_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void lux_meter_event_handler(bc_tag_lux_meter_t *self, bc_tag_lux_meter_event_t event, void *event_param)
{
    event_param_t *param = (event_param_t *)event_param;
    float value;

    if (event == BC_TAG_LUX_METER_EVENT_UPDATE)
    {
        if (bc_tag_lux_meter_get_illuminance_lux(self, &value))
        {
            if ((fabs(value - param->value) >= LUX_METER_TAG_PUB_VALUE_CHANGE) || (param->next_pub < bc_scheduler_get_spin_tick()))
            {
                bc_radio_pub_luminosity(param->channel, &value);
                param->value = value;
                param->next_pub = bc_scheduler_get_spin_tick() + LUX_METER_TAG_PUB_NO_CHANGE_INTERVAL;
            }
        }
    }
}

void switch_to_normal_mode_task(void *param)
{
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_NORMAL_INTERVAL);

    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_NORMAL_INTERVAL);

    bc_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_NORMAL_INTERVAL);

    bc_module_co2_set_update_interval(CO2_UPDATE_NORMAL_INTERVAL);

    bc_tag_voc_set_update_interval(&tag_voc, VOC_TAG_UPDATE_NORMAL_INTERVAL);

    bc_tag_lux_meter_set_update_interval(&lux_meter, LUX_METER_TAG_UPDATE_NORMAL_INTERVAL);

    bc_scheduler_unregister(bc_scheduler_get_current_task_id());
}

void application_init(void)
{
    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);

    // Initialize thermometer sensor on core module
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);

    // Initialize button
    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_hold_time(&button, 10000);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize temperature
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_temperature_init(&temperature, BC_I2C_I2C0, BC_TAG_TEMPERATURE_I2C_ADDRESS_DEFAULT);
    bc_tag_temperature_set_update_interval(&temperature, TEMPERATURE_UPDATE_SERVICE_INTERVAL);
    bc_tag_temperature_set_event_handler(&temperature, temperature_tag_event_handler, &temperature_event_param);

    // Initialize humidity
    humidity_event_param.channel = BC_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT;
    bc_tag_humidity_init(&humidity, BC_TAG_HUMIDITY_REVISION_R3, BC_I2C_I2C0, BC_TAG_HUMIDITY_I2C_ADDRESS_DEFAULT);
    bc_tag_humidity_set_update_interval(&humidity, HUMIDITY_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_humidity_set_event_handler(&humidity, humidity_tag_event_handler, &humidity_event_param);

    // Initialize barometer
    barometer_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT;
    bc_tag_barometer_init(&barometer, BC_I2C_I2C0);
    bc_tag_barometer_set_update_interval(&barometer, BAROMETER_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_barometer_set_event_handler(&barometer, barometer_tag_event_handler, &barometer_event_param);

    // Initialize CO2
    bc_module_co2_init();
    bc_module_co2_set_update_interval(CO2_UPDATE_SERVICE_INTERVAL);
    bc_module_co2_set_event_handler(co2_event_handler, &co2_event_param);

    // Initialize VOC Tag
    bc_tag_voc_init(&tag_voc, BC_I2C_I2C0);
    bc_tag_voc_set_update_interval(&tag_voc, VOC_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_voc_set_event_handler(&tag_voc, voc_tag_event_handler, &voc_event_param);

    // Initialize Luxmeter Tag
    lux_meter_event_param.channel = 0;
    bc_tag_lux_meter_init(&lux_meter, BC_I2C_I2C0, BC_TAG_LUX_METER_I2C_ADDRESS_DEFAULT);
    bc_tag_lux_meter_set_update_interval(&lux_meter, LUX_METER_TAG_UPDATE_SERVICE_INTERVAL);
    bc_tag_lux_meter_set_event_handler(&lux_meter, lux_meter_event_handler, &lux_meter_event_param);

    bc_radio_pairing_request("co2-monitor", VERSION);

    bc_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);

    bc_led_pulse(&led, 2000);
}
