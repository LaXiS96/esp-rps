#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/gpio.h"
#include "driver/hw_timer.h"

#include "httpd.h"
#include "wifi.h"

static const char *TAG = "app";

typedef enum
{
    POWER_ON = 'i',
    POWER_OFF = 'o',
    POWER_HARDOFF = 'h',
} power_state_t;

typedef enum
{
    PULSE_SHORT,
    PULSE_LONG,
} pulse_t;

// TODO use built-in led to show requests?

// TODO possible state changes
// off -> on = short pulse
// on -> shutdown = short pulse to trigger os shutdown
// on -> hard off = long pulse (but how long?)

// TODO /act could set location header to go back to index with query that includes result message
// TODO store this in flash possibly
// static const char *index_body = "<form action=/act method=post><input name=key> <button name=action value=i>ON <button name=action value=o>OFF <button name=action value=h>HARDOFF</form><script>setTimeout(()=>window.location.reload(),5000)</script>";

static void hw_timer_callback(void *arg)
{
    // TODO mode which checks GPIO_IN status before deasserting GPIO_OUT?
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_APP_GPIO_OUT_NUM, 0));
}

static void trigger_pulse(pulse_t pulse_type)
{
    switch (pulse_type)
    {
    case PULSE_SHORT:
        ESP_ERROR_CHECK(gpio_set_level(CONFIG_APP_GPIO_OUT_NUM, 1));
        ESP_ERROR_CHECK(hw_timer_alarm_us(500000, false)); // 500ms
        break;
    case PULSE_LONG:
        ESP_ERROR_CHECK(gpio_set_level(CONFIG_APP_GPIO_OUT_NUM, 1));
        ESP_ERROR_CHECK(hw_timer_alarm_us(5000000, false)); // 5s
        break;
    default:
        ESP_LOGE(TAG, "unexpected pulse type %d", pulse_type);
    }
}

// Trigger power state transition if valid for current power state
static esp_err_t power_state_transition(power_state_t to_state)
{
    int level = gpio_get_level(CONFIG_APP_GPIO_IN_NUM);
    if (level)
        switch (to_state)
        {
        case POWER_ON:
            return ESP_ERR_INVALID_STATE;
        case POWER_OFF:
            trigger_pulse(PULSE_SHORT);
            return ESP_OK;
        case POWER_HARDOFF:
            trigger_pulse(PULSE_LONG);
            return ESP_OK;
        default:
            return ESP_ERR_INVALID_ARG;
        }
    else
        switch (to_state)
        {
        case POWER_ON:
            trigger_pulse(PULSE_SHORT);
            return ESP_OK;
        case POWER_OFF:
        case POWER_HARDOFF:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_ERR_INVALID_ARG;
        }
}

static esp_err_t get_index_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s %d", req->uri, req->content_len);

    // TODO new endpoint for state (js fetch)

    int level = gpio_get_level(CONFIG_APP_GPIO_IN_NUM);
    httpd_resp_send_chunk(req, level ? "ON" : "OFF", -1);
    // TODO read index.html from spiffs
    // httpd_resp_send_chunk(req, index_body, -1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t post_act_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST %s %d", req->uri, req->content_len);

    if (req->content_len == 0 || req->content_len > 64)
        return httpd_send_badrequest(req, "Invalid body length");

    char body[65];
    int ret;
    if ((ret = httpd_req_recv(req, body, sizeof(body))) <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            httpd_resp_send_408(req);

        return ESP_FAIL;
    }
    body[ret < sizeof(body) ? ret : sizeof(body) - 1] = '\0';

    char key_val[17];
    if (httpd_get_form_value(body, "key=", key_val, sizeof(key_val)) == 0)
        return httpd_send_badrequest(req, "Missing key");
    key_val[sizeof(key_val) - 1] = '\0';
    ESP_LOGI(TAG, "key: %s", key_val);
    if (strcmp(key_val, CONFIG_APP_HTTPD_KEY) != 0)
        return httpd_send_forbidden(req, "Invalid key");

    char action_val[2];
    if (httpd_get_form_value(body, "action=", action_val, sizeof(action_val)) == 0)
        return httpd_send_badrequest(req, "Missing action");
    ESP_LOGI(TAG, "action: %s", action_val);

    esp_err_t err = power_state_transition(action_val[0]);
    switch (err)
    {
    case ESP_ERR_INVALID_ARG:
        return httpd_send_badrequest(req, "Unknown action");
    case ESP_ERR_INVALID_STATE:
        return httpd_send_conflict(req, "Invalid action for current state");
    }

    httpd_resp_send(req, "OK", -1);
    return ESP_OK;
}

static void spiffs_init()
{
    esp_vfs_spiffs_conf_t spiffs_config = {
        .base_path = "/spiffs",
        .format_if_mount_failed = true,
        .max_files = 4,
        .partition_label = NULL,
    };
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&spiffs_config));

    size_t total = 0, used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(NULL, &total, &used));
    ESP_LOGI(TAG, "spiffs size: %d, used: %d", total, used);
}

static void gpio_init()
{
    gpio_config_t gpio_out_config = {0};
    gpio_out_config.mode = GPIO_MODE_OUTPUT;
    gpio_out_config.pin_bit_mask = 1 << CONFIG_APP_GPIO_OUT_NUM;
    gpio_out_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&gpio_out_config);

    gpio_config_t gpio_in_config = {0};
    gpio_in_config.mode = GPIO_MODE_INPUT;
    gpio_in_config.pin_bit_mask = 1 << CONFIG_APP_GPIO_IN_NUM;
    // TODO pull?
    gpio_config(&gpio_in_config);
}

void app_main()
{
    spiffs_init();
    ESP_ERROR_CHECK(hw_timer_init(hw_timer_callback, NULL));
    gpio_init();
    wifi_init();
    // TODO mdns
    httpd_init();

    httpd_uri_t get_index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = get_index_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd, &get_index_uri));

    httpd_uri_t post_act_uri = {
        .uri = "/act",
        .method = HTTP_POST,
        .handler = post_act_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(httpd, &post_act_uri));

    ESP_LOGI(TAG, "main finished");
}
