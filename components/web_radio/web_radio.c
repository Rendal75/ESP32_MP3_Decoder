/*
 * web_radio.c
 *
 *  Created on: 13.03.2017
 *      Author: michaelboeckling
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_http_client.h"

#include "vector.h"
#include "web_radio.h"

#include "controls.h"
#include "playlist.h"
#include "screen.h"

#define TAG "web_radio"
#define HDR_KV_BUFF_LEN 128
#define MSG_QUEUE_SIZE 20
#define MSG_TICKS_TO_WAIT 1000

typedef enum { S_CONNECTING, S_STREAMING, S_FINISHED  } state;

typedef enum { MSG_HTTP, MSG_CTRL } msg_type;
typedef enum { MSG_HTTP_CONNECTED, MSG_HTTP_BODY_DATA, MSG_HTTP_HEADER_FINISHED, MSG_HTTP_CONNECTION_FINISHED } msg_type_http;
typedef enum { MSG_CTRL_STOP_REQ } msg_type_ctrl;

typedef struct msg_http {
	esp_http_client_event_t evt;
} msg_http;

typedef struct msg_ctrl {
	msg_type_ctrl id;
} msg_ctrl;

typedef struct msg {
	msg_type type;
	union  {
		msg_http http;
		msg_ctrl ctrl;
	};
} msg;

static int icymeta_interval;

static QueueHandle_t msg_queue;
static web_radio_t *radio_conf;
static state current_state;

static void stringtolower(char* pstr) {
	for(char *p = pstr;*p;++p) *p=*p>0x40&&*p<0x5b?*p|0x60:*p;
}

static void set_state(state new_state) {
	ESP_LOGE(TAG, "State: %d", new_state);
	current_state = new_state;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	char *key;
	char *value;

	static bool first_data_received = false;

	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			first_data_received = 0;
			ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			key = evt->header_key;
			value = evt->header_value;
			ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", key, value);

			stringtolower(key);
			if (strcmp(key, "content-type") == 0) {
				content_type_t content_type = MIME_UNKNOWN;
				stringtolower(value);
				if (strstr(value, "application/octet-stream")) content_type = OCTET_STREAM;
				if (strstr(value, "audio/aac")) content_type = AUDIO_AAC;
				if (strstr(value, "audio/mp4")) content_type = AUDIO_MP4;
				if (strstr(value, "audio/x-m4a")) content_type = AUDIO_MP4;
				if (strstr(value, "audio/mpeg")) content_type = AUDIO_MPEG;

				radio_conf->player_config->media_stream->content_type = content_type;
				if(content_type == MIME_UNKNOWN) {
					ESP_LOGE(TAG, "unknown content-type: %s", value);
				}
			}
			else if (strcmp(key, "icy-metaint") == 0) {
				icymeta_interval = atoi(value);
				ESP_LOGW(TAG, "icymeta_interval=%d", icymeta_interval);
			}
			break;
		case HTTP_EVENT_ON_DATA: {
			if(!first_data_received) {
				audio_player_start(icymeta_interval);
				first_data_received = 1;
			}
			bool is_chunked_response = esp_http_client_is_chunked_response(evt->client);
			ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d, chunked=%d", evt->data_len, is_chunked_response);
			//if (!is_chunked_response) {
				// Write out data
				audio_stream_consumer((char*)evt->data,  evt->data_len, radio_conf->player_config);
			//}
			}
			break;
		case HTTP_EVENT_ON_FINISH:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
			break;
		case HTTP_EVENT_DISCONNECTED:
			ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
			break;
	}

    return ESP_OK;
}


static esp_http_client_handle_t client;
static void http_get_task()
{
    for(;;) {
		set_state(S_CONNECTING);
		radio_conf->player_config->media_stream->eof = false;
		icymeta_interval = 0;

		// blocks until end of stream
		playlist_entry_t *curr_track = playlist_curr_track(radio_conf->playlist);
		screen_on_entry_changed(curr_track);

		ESP_LOGW(TAG, "Playing track: %s", curr_track->name);
		esp_http_client_config_t config = {
			.url = curr_track->url,
			.event_handler = _http_event_handler,
			.buffer_size = 10000
		};
		client = esp_http_client_init(&config);
		esp_http_client_set_header(client, "User-Agent", "ESP32");
		esp_http_client_set_header(client, "Accept", "audio/mpeg, audio/x-mpeg, audio/mp3, audio/x-mp3, audio/mpeg3, audio/x-mpeg3, audio/mpg, audio/x-mpg, audio/x-mpegaudio, application/octet-stream, audio/mpegurl, audio/mpeg-url, audio/x-mpegurl, audio/x-scpls, audio/scpls, application/pls, application/x-scpls, application/pls+xml, */*");
		esp_http_client_set_header(client, "Icy-MetaData", "1");

		set_state(S_STREAMING);
		// Blocking call
		esp_err_t err = esp_http_client_perform(client);
		if(err) {
			ESP_LOGE(TAG, "esp_http_client_perform - >%s", esp_err_to_name(err));
			continue;
		}
		radio_conf->player_config->media_stream->eof = true;
		set_state(S_FINISHED);
        while(radio_conf->player_config->decoder_status != STOPPED) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
		esp_http_client_cleanup(client);
	}

    vTaskDelete(NULL);
}

void web_radio_start()
{
	ESP_LOGI(TAG, "web_radio_start: RAM left %d", esp_get_free_heap_size());
	http_get_task();
    // start reader task
    //xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 2560, NULL, 20,   NULL, 0);

    for(;;) {
    	msg m;
    	if(xQueueReceive(msg_queue, &m, 100000/portTICK_PERIOD_MS)) {
    		ESP_LOGD(TAG, "Got message");
    		switch(m.type) {
    		case MSG_HTTP:
				break;
			case MSG_CTRL:
				ESP_LOGI(TAG, "MSG_CONTROL not implemented yet !");
				break;
    		}
    	}
    	else {
    		ESP_LOGI(TAG, "No message");
    	}
    }

}

void web_radio_gpio_handler_task(void *pvParams)
{
    gpio_handler_param_t *params = pvParams;
    web_radio_t *config = params->user_data;
    xQueueHandle gpio_evt_queue = params->gpio_evt_queue;

    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            ESP_LOGI(TAG, "GPIO[%d] intr, val: %d", io_num, gpio_get_level(io_num));
            playlist_next(config->playlist);
            esp_http_client_close(client);
        }
    }
}

void web_radio_init(web_radio_t *config)
{
	radio_conf = config;
	msg_queue = xQueueCreate(MSG_QUEUE_SIZE, sizeof(msg));
    controls_init(web_radio_gpio_handler_task, 2048, config);
    audio_player_init(config->player_config);
}

void web_radio_destroy(web_radio_t *config)
{
    controls_destroy(config);
    audio_player_destroy(config->player_config);
}
