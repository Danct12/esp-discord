#ifndef _DISCORD_PRIVATE_DISCORD_H_
#define _DISCORD_PRIVATE_DISCORD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event_base.h"
#include "esp_websocket_client.h"
#include "esp_http_client.h"
#include "_models.h"
#include "discord.h"

#ifndef CONFIG_IDF_TARGET
#define CONFIG_IDF_TARGET "esp32"
#endif

#define DISCORD_GW_WS_BUFFER_SIZE        (512)
#define DISCORD_DEFAULT_BUFFER_SIZE      (3 * 1024)
#define DISCORD_DEFAULT_TASK_STACK_SIZE  (6 * 1024)
#define DISCORD_DEFAULT_TASK_PRIORITY    (4)
#define DISCORD_API_URL                  "https://discord.com/api/v8"
#define DISCORD_API_BUFFER_SIZE          (3 * 1024)
#define DISCORD_API_TIMEOUT_MS           (8000)
#define DISCORD_QUEUE_SIZE               (3)

#define DISCORD_LOG_TAG "DISCORD"

#define DISCORD_LOG_DEFINE_BASE() static const char* TAG = DISCORD_LOG_TAG
#define DISCORD_LOG(esp_log_foo, format, ...) esp_log_foo(TAG, "%s: " format, __func__, ##__VA_ARGS__)
#define DISCORD_LOGE(format, ...) DISCORD_LOG(ESP_LOGE, format, ##__VA_ARGS__)
#define DISCORD_LOGW(format, ...) DISCORD_LOG(ESP_LOGW, format, ##__VA_ARGS__)
#define DISCORD_LOGI(format, ...) DISCORD_LOG(ESP_LOGI, format, ##__VA_ARGS__)
#define DISCORD_LOGD(format, ...) DISCORD_LOG(ESP_LOGD, format, ##__VA_ARGS__)
#define DISCORD_LOGV(format, ...) DISCORD_LOG(ESP_LOGV, format, ##__VA_ARGS__)
#define DISCORD_LOG_FOO() DISCORD_LOGD("...")

#define DISCORD_EVENT_EMIT(event, data) client->event_handler(client, event, data)

#define STRDUP(str) (str ? strdup(str) : NULL)

typedef enum {
    DISCORD_CLIENT_STATE_ERROR = -2,
    DISCORD_CLIENT_STATE_DISCONNECTED = -1,
    DISCORD_CLIENT_STATE_UNKNOWN,
    DISCORD_CLIENT_STATE_INIT,
    DISCORD_CLIENT_STATE_CONNECTING,
    DISCORD_CLIENT_STATE_CONNECTED,
    DISCORD_CLIENT_STATE_DISCONNECTING
} discord_client_state_t;

typedef enum {
    DISCORD_CLOSE_REASON_NOT_REQUESTED,
    DISCORD_CLOSE_REASON_HEARTBEAT_ACK_NOT_RECEIVED,
    DISCORD_CLOSE_REASON_LOGOUT
} discord_close_reason_t;

enum {
    DISCORD_OP_DISPATCH,                    /*!< [Receive] An event was dispatched */
    DISCORD_OP_HEARTBEAT,                   /*!< [Send/Receive] An event was dispatched */
    DISCORD_OP_IDENTIFY,                    /*!< [Send] Starts a new session during the initial handshake */
    DISCORD_OP_PRESENCE_UPDATE,             /*!< [Send] Update the client's presence */
    DISCORD_OP_VOICE_STATE_UPDATE,          /*!< [Send] Used to join/leave or move between voice channels */
    DISCORD_OP_RESUME = 6,                  /*!< [Send] Resume a previous session that was disconnected */
    DISCORD_OP_RECONNECT,                   /*!< [Receive] You should attempt to reconnect and resume immediately */
    DISCORD_OP_REQUEST_GUILD_MEMBERS,       /*!< [Send] Request information about offline guild members in a large guild */
    DISCORD_OP_INVALID_SESSION,             /*!< [Receive] The session has been invalidated. You should reconnect and identify/resume accordingly */
    DISCORD_OP_HELLO,                       /*!< [Receive] Sent immediately after connecting, contains the heartbeat_interval to use */
    DISCORD_OP_HEARTBEAT_ACK,               /*!< [Receive] Sent in response to receiving a heartbeat to acknowledge that it has been received */
};

#define _DISCORD_CLOSEOP_MIN DISCORD_CLOSEOP_UNKNOWN_ERROR
#define _DISCORD_CLOSEOP_MAX DISCORD_CLOSEOP_DISALLOWED_INTENTS

typedef enum {
    DISCORD_CLOSEOP_NO_CODE,
    DISCORD_CLOSEOP_UNKNOWN_ERROR = 4000,   /*!< We're not sure what went wrong. Try reconnecting? */
    DISCORD_CLOSEOP_UNKNOWN_OPCODE,         /*!< You sent an invalid Gateway opcode or an invalid payload for an opcode. Don't do that! */
    DISCORD_CLOSEOP_DECODE_ERROR,           /*!< You sent an invalid payload to us. Don't do that! */
    DISCORD_CLOSEOP_NOT_AUTHENTICATED,      /*!< You sent us a payload prior to identifying. */
    DISCORD_CLOSEOP_AUTHENTICATION_FAILED,  /*!< The account token sent with your identify payload is incorrect. */
    DISCORD_CLOSEOP_ALREADY_AUTHENTICATED,  /*!< You sent more than one identify payload. Don't do that! */
    DISCORD_CLOSEOP_INVALID_SEQ = 4007,     /*!< The sequence sent when resuming the session was invalid. Reconnect and start a new session. */
    DISCORD_CLOSEOP_RATE_LIMITED,           /*!< Woah nelly! You're sending payloads to us too quickly. Slow it down! You will be disconnected on receiving this. */
    DISCORD_CLOSEOP_SESSION_TIMED_OUT,      /*!< Your session timed out. Reconnect and start a new one. */
    DISCORD_CLOSEOP_INVALID_SHARD,          /*!< You sent us an invalid shard when identifying. */
    DISCORD_CLOSEOP_SHARDING_REQUIRED,      /*!< The session would have handled too many guilds - you are required to shard your connection in order to connect. */
    DISCORD_CLOSEOP_INVALID_API_VERSION,    /*!< You sent an invalid version for the gateway. */
    DISCORD_CLOSEOP_INVALID_INTENTS,        /*!< You sent an invalid intent for a Gateway Intent. You may have incorrectly calculated the bitwise value. */
    DISCORD_CLOSEOP_DISALLOWED_INTENTS      /*!< You sent a disallowed intent for a Gateway Intent. You may have tried to specify an intent that you have not enabled or are not whitelisted for. */
} discord_close_code_t;

typedef struct {
    bool running;
    int interval;
    uint64_t tick_ms;
    bool received_ack;
} discord_heartbeater_t;

typedef esp_err_t(*discord_event_handler_t)(discord_client_handle_t client, discord_event_t event, discord_event_data_ptr_t data_ptr);

struct discord_client {
    bool running;
    discord_client_state_t state;
    TaskHandle_t task_handle;
    QueueHandle_t queue;
    esp_event_loop_handle_t event_handle;
    discord_event_handler_t event_handler;
    discord_client_config_t* config;
    esp_websocket_client_handle_t ws;
    SemaphoreHandle_t api_lock;
    esp_http_client_handle_t http;
    char* http_buffer;
    int http_buffer_size;
    bool http_buffer_record;
    esp_err_t http_buffer_record_status;
    discord_heartbeater_t heartbeater;
    discord_session_t* session;
    int last_sequence_number;
    char* buffer;
    int buffer_len;
    discord_close_reason_t close_reason;
    discord_close_code_t close_code;
};

#ifdef __cplusplus
}
#endif

#endif