#ifndef _DISCORD_PRIVATE_API_H_
#define _DISCORD_PRIVATE_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "discord.h"

#define DCAPI_POST(strcater, serializer, stream) ({ \
    char* _uri = strcater; \
    char* _json = serializer; \
    discord_api_response_t* _res = dcapi_post(client, _uri, _json, stream); \
    free(_json); \
    free(_uri); \
    _res;\
})

typedef struct {
    int code;
    char* data;
    int data_len;
} discord_api_response_t;

bool dcapi_response_is_success(discord_api_response_t* res);
esp_err_t dcapi_response_to_esp_err(discord_api_response_t* res);
void dcapi_response_free(discord_handle_t client, discord_api_response_t* res);
esp_err_t dcapi_download(discord_handle_t client, const char* url, discord_download_handler_t download_handler, discord_api_response_t** out_response);
/**
 * GET request
 * 
 * @param uri Will be automatically freed
 * @param data Will be automatically freed
 * @param stream Stream and save http response body inside of api response?
 */ 
esp_err_t dcapi_get(discord_handle_t client, char* uri, char* data, bool stream, discord_api_response_t** out_response);
/**
 * POST request
 * 
 * @param uri Will be automatically freed
 * @param data Will be automatically freed
 * @param stream Stream and save http response body inside of api response?
 */ 
esp_err_t dcapi_post(discord_handle_t client, char* uri, char* data, bool stream, discord_api_response_t** out_response);
esp_err_t dcapi_post_(discord_handle_t client, char* uri, char* data);
/**
 * PUT request
 * 
 * @param uri Will be automatically freed
 * @param data Will be automatically freed
 * @param stream Stream and save http response body inside of api response?
 */
esp_err_t dcapi_put(discord_handle_t client, char* uri, char* data, bool stream, discord_api_response_t** out_response);
esp_err_t dcapi_put_(discord_handle_t client, char* uri, char* data);

void dcapi_destroy(discord_handle_t client);

#ifdef __cplusplus
}
#endif

#endif