#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "discord_gateway.h"

static const char* TAG = "discord_gateway";

static discord_bot_config_t _config;
static esp_websocket_client_handle_t ws_client;
static esp_timer_handle_t heartbeat_timer;

static esp_err_t discord_gw_identify() {
    cJSON* props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "$os", "esp-idf");
    cJSON_AddStringToObject(props, "$browser", "esp32");
    cJSON_AddStringToObject(props, "$device", "esp32");

    cJSON* data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "token", _config.token);
    cJSON_AddNumberToObject(data, "intents", 512);
    cJSON_AddItemToObject(data, "properties", props);
    
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "op", 2);
    cJSON_AddItemToObject(payload, "d", data);

    char* payload_raw = cJSON_PrintUnformatted(payload);

    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Sending=%s", payload_raw);

    esp_websocket_client_send_text(ws_client, payload_raw, strlen(payload_raw), portMAX_DELAY);

    free(payload_raw);

    return ESP_OK;
}

static esp_err_t discord_gw_process_event(const cJSON* payload) {
    cJSON* t = cJSON_GetObjectItem(payload, "t");
    
    if(cJSON_IsNull(t)) {
        ESP_LOGW(TAG, "Missing event name");
        return ESP_FAIL;
    }

    char* event_name = t->valuestring;

    if(strcmp("READY", event_name) == 0) {
        ESP_LOGI(TAG, "Successfully identified");
    } else if(strcmp("MESSAGE_CREATE", event_name) == 0) {
        ESP_LOGI(TAG, "Received discord message");
    }

    return ESP_OK;
}

static esp_err_t discord_gw_send_heartbeat() {
    cJSON* payload = cJSON_CreateObject();
    cJSON_AddNumberToObject(payload, "op", 1);
    char* payload_raw = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);

    ESP_LOGI(TAG, "Sending=%s", payload_raw);
    esp_websocket_client_send_text(ws_client, payload_raw, strlen(payload_raw), portMAX_DELAY);
    free(payload_raw);

    return ESP_OK;
}

static void discord_gw_heartbeat_timer_cb(void* arg) {
    ESP_LOGI(TAG, "Heartbeat now should be sent but its seems its cannot be sent from different task... Fix this with events?.");
    //discord_gw_send_heartbeat();
}

static esp_err_t discord_gw_heartbeat_start(int interval) {
    ESP_LOGI(TAG, "Heartbeat starting");

    return esp_timer_start_periodic(heartbeat_timer, interval * 1000);
}

static esp_err_t discord_gw_heartbeat_init() {
    ESP_LOGI(TAG, "Heartbeat initialized");

    const esp_timer_create_args_t heartbeat_timer_args = {
        .callback = &discord_gw_heartbeat_timer_cb
    };

    return esp_timer_create(&heartbeat_timer_args, &heartbeat_timer);
}

static esp_err_t discord_gw_parse_payload(esp_websocket_event_data_t* data) {
    cJSON* payload = cJSON_Parse(data->data_ptr);
    int op = cJSON_GetObjectItem(payload, "op")->valueint;
    ESP_LOGW(TAG, "OP: %d", op);
    cJSON* d;
    int heartbeat_interval;

    switch(op) {
        case 10: // heartbeat and identify
            d = cJSON_GetObjectItem(payload, "d");
            heartbeat_interval = cJSON_GetObjectItem(d, "heartbeat_interval")->valueint;
            discord_gw_heartbeat_start(heartbeat_interval);
            discord_gw_identify();
            break;
        case 0: // event
            discord_gw_process_event(payload);
            break;
        default:
            ESP_LOGW(TAG, "Unknown OP code %d", op);
            break;
    }

    cJSON_Delete(payload);
    return ESP_OK;
}

static void websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*) event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_CONNECTED");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            break;
        case WEBSOCKET_EVENT_DATA:
            if(data->op_code == 1) {
                ESP_LOGI(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d", data->payload_len, data->data_len, data->payload_offset);
                ESP_LOGI(TAG, "Received=%.*s", data->data_len, (char*) data->data_ptr);

                discord_gw_parse_payload(data);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_ERROR");
            break;
        case WEBSOCKET_EVENT_CLOSED:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_CLOSED");
            break;
        default:
            ESP_LOGW(TAG, "WEBSOCKET_EVENT_UNKNOWN %d", event_id);
            break;
    }
}

esp_err_t discord_gw_init(const discord_bot_config_t config) {
    _config = config;

    ESP_ERROR_CHECK(discord_gw_heartbeat_init());

    return ESP_OK;
}

esp_err_t discord_gw_open() {
    esp_websocket_client_config_t ws_cfg = {
        .uri = "wss://gateway.discord.gg/?v=8&encoding=json"
    };

    ws_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void*) ws_client);
    esp_websocket_client_start(ws_client);

    return ESP_OK;
}