/*
 * web_radio.c
 *
 *  Created on: 13.03.2017
 *      Author: michaelboeckling
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_http_client.h"

#include "vector.h"
#include "web_radio.h"
#include "spiram_fifo.h"
#include "controls.h"
#include "playlist.h"
#include "screen.h"
#include "tft.h"
#include "icy_parser.h"

#define TAG "web_radio"
#define HDR_KV_BUFF_LEN 128
#define MSG_QUEUE_SIZE 20
#define MSG_TICKS_TO_WAIT 1000

 #define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

static bool http_stop_requested = false;
typedef enum { S_CONNECTING, S_STREAMING, S_FINISHED  } state;
static char* s_state[] = { "CONNECTING", "STREAMING" , "FINISHED" };


static int icymeta_interval;

static QueueHandle_t msg_queue;
static web_radio_t *radio_conf;
static state current_state;

#define STREAM_TITLE_LEN 60
char stream_title[STREAM_TITLE_LEN];

#define STREAM_URL_LEN 60
char stream_url[STREAM_URL_LEN];


#define RADIO_NAME_LEN 40
static char radio_name[RADIO_NAME_LEN];

static void stringtolower(char* pstr) {
	for(char *p = pstr;*p;++p) *p=*p>0x40&&*p<0x5b?*p|0x60:*p;
}

static void set_state(state new_state) {
	ESP_LOGE(TAG, "State: %s", s_state[new_state]);
	current_state = new_state;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	char *key;
	char *value;
	msg m;
	m.type = MSG_HTTP;

	static bool first_data_received = false;

	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
			m.http.id = MSG_HTTP_CONNECTED;
			first_data_received = 0;
			web_radio_post(&m);
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

static TimerHandle_t timer_handle_1s;
static TimerHandle_t timer_handle_10s;

static void timerCallback(TimerHandle_t pxTimer) {
	static int counter = 0;
	msg m;
	m.type = MSG_TIMER;
	//ESP_LOGI(TAG, "timerCallback :%p", pxTimer);
	counter++;
	if(pxTimer==timer_handle_1s) {
		m.timer.id = counter % 10 == 0 ? MSG_TIMER_10S : MSG_TIMER_1S;
	}
	else if(pxTimer==timer_handle_10s) {
		m.timer.id = MSG_TIMER_10S;
	}
	else {
		ESP_LOGE(TAG, "Unexpected timer !");
	}

	web_radio_post(&m);
}

static void init_timers(void) {
	timer_handle_1s = xTimerCreate("Timer-1s", 1000 / portTICK_PERIOD_MS, pdTRUE, (void*) 0, timerCallback);
	timer_handle_10s = xTimerCreate("Timer-10s", 10000 / portTICK_PERIOD_MS, pdTRUE, (void*) 0, timerCallback);
	ESP_LOGI(TAG, "Created timers: 1s:%p, 10s:%p", timer_handle_1s, timer_handle_10s);
	assert(timer_handle_1s);
	assert(timer_handle_10s);
}

static esp_http_client_handle_t client;
static void http_get_task(void* ptr)
{
    for(;;) {
		set_state(S_CONNECTING);
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
		esp_err_t err = esp_http_client_perform(client);
		if(err) {
			ESP_LOGE(TAG, "esp_http_client_perform - >%s", esp_err_to_name(err));
			esp_http_client_cleanup(client);
			continue;
		}
		radio_conf->player_config->media_stream->eof = true;
		set_state(S_FINISHED);
        while(radio_conf->player_config->decoder_status == RUNNING) {
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
		esp_http_client_cleanup(client);
	}

    vTaskDelete(NULL);
}


#if(WEBRADIO_TASK_STATS==1)
static TaskHandle_t cpu0_TaskHandle, cpu1_TaskHandle;

/*
 * Monitoring code.
 *
 * Create 2 low priority task, one per core.
 * At timer trigger, we show their cpu usage percentage
 */
static void monitoring_init(void) {
	cpu0_TaskHandle = xTaskGetIdleTaskHandleForCPU( 0 );
	cpu1_TaskHandle = xTaskGetIdleTaskHandleForCPU( 1 );
}

static char taskBuffer[1024];
static void screen_refreshIdle(void) {
	bzero(taskBuffer, 1024);
	ESP_LOGW(TAG, "Dumping tasks stats:");
	vTaskGetRunTimeStats(taskBuffer);
	puts(taskBuffer);
}
#endif




#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240


static void handle_ui_fifo_update(int current_fill, int size) {
	int w = (current_fill * SCREEN_WIDTH)/ size;
	TFT_drawFastHLine(0, 30, w, TFT_GREEN);
	TFT_drawFastHLine(w+1, 30, SCREEN_WIDTH-w-1, TFT_RED);
}


void screen_on_entry_changed(playlist_entry_t* entry) {
	if(entry==NULL) {
		radio_name[0] = '\0';
	}
	else {
		strncpy(radio_name, entry->name, RADIO_NAME_LEN - 1);
	}
	msg m;
	m.type = MSG_HTTP;
	m.http.id = MSG_HTTP_RADIO_UPDATE;
	web_radio_post(&m);
}


static void handle_ui_refreshTime(void) {
	char tmp_buff[64];
	time_t time_now;
	struct tm* tm_info;
    time(&time_now);
	tm_info = localtime(&time_now);
	uint32_t ramleft = esp_get_free_heap_size();
	sprintf(tmp_buff, "uptime=%02d:%02d:%02d ram-left=%06d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ramleft);

	update_header(NULL, tmp_buff);
}

void web_radio_start()
{
	ESP_LOGI(TAG, "web_radio_start: RAM left %d", esp_get_free_heap_size());

	BaseType_t r = xTimerStart(timer_handle_1s, 0);
	assert(r!=pdFAIL);

	if (xTaskCreatePinnedToCore(http_get_task, "http-get-task", 3000, NULL,
			configMAX_PRIORITIES - 2, NULL, 1) != pdPASS)
	{
		ESP_LOGE(TAG, "ERROR creating http_get_task! Out of memory?");
	}
	;
    // start reader task
    //xTaskCreatePinnedToCore(&http_get_task, "http_get_task", 2560, NULL, 20,   NULL, 0);

	int i;
	int io_num;
    for(i=0;;i++) {
    	msg m;
    	if(xQueueReceive(msg_queue, &m, 10000/portTICK_PERIOD_MS)) {
    		ESP_LOGD(TAG, "i=%d", i);
    		switch(m.type) {
    		case MSG_HTTP:
    			if(m.http.id==MSG_HTTP_RADIO_UPDATE) {
    				disp_header(radio_name);
    			}
				break;
			case MSG_CTRL:
				io_num = m.ctrl.gpio_num;
				ESP_LOGI(TAG, "GPIO[%d] intr, val: %d media_Stream=%p", io_num, gpio_get_level(io_num), radio_conf->player_config->media_stream);
				audio_player_stop();
				//radio_conf->player_config->media_stream->eof = true;
				http_stop_requested = true;
				playlist_next(radio_conf->playlist);
				esp_http_client_close(client);
				ESP_LOGI(TAG, "Client closed");
				break;
			case MSG_TIMER:
				//ESP_LOGI(TAG, "Timer id=%d", m.timer.id);
				if(m.timer.id==MSG_TIMER_1S) {
					handle_ui_refreshTime();
				}
#if(WEBRADIO_TASK_STATS==1)
				else if(m.timer.id==MSG_TIMER_10S) {
					screen_refreshIdle();
				}
#endif
				break;
			case MSG_PLAYER:
				TFT_fillWindow(TFT_BLACK);
				TFT_print(stream_title, 0, 100);
				break;
			case MSG_FIFO:
				handle_ui_fifo_update(spiRamFifoFill(), spiRamFifoLen());
				break;
    		}
    	}
    	else {
    		ESP_LOGI(TAG, "No message");
    	}
    }

}

void web_radio_init(web_radio_t *config)
{
	radio_conf = config;
	msg_queue = xQueueCreate(MSG_QUEUE_SIZE, sizeof(msg));
	init_timers();
    controls_init();
#if(WEBRADIO_TASK_STATS==1)
    monitoring_init();
#endif
    audio_player_init(config->player_config);
}

void web_radio_destroy(web_radio_t *config)
{
    controls_destroy(config);
    audio_player_destroy(config->player_config);
}

void web_radio_post_from_isr(msg* m) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendToBackFromISR(msg_queue, m, &xHigherPriorityTaskWoken);

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void web_radio_post(msg* m) {

    if(xQueueSend(msg_queue, m, 20 / portTICK_PERIOD_MS) != pdPASS ) {
    	ESP_LOGE(TAG, "Failed to post message, queue full!");
    	return;
    }

}



/*
 * StreamTitle='erwer';StreamUrl='dsfsf'
 */
void screen_on_meta(char* meta) {
	ESP_LOGI(TAG, "meta:%s", meta);

	icy_parser_t parser;
	icy_parser_init(meta, &parser);
	stream_title[0] = '\0';
	for(;;) {
		bool got = icy_parser_next(&parser);
		if(!got) {
			break;
		}
		int len = min(strlen("StreamTitle"), parser.key_len);
		if(strncmp("StreamTitle", &parser.data[parser.key_start], len)==0) {
			len = min(STREAM_TITLE_LEN-1, parser.value_len);
			strncpy(stream_title, &parser.data[parser.value_start], len);
			stream_title[len] = '\0';
			ESP_LOGI(TAG, "title:[%s]", stream_title);
			break;
		}
	}

	msg m;
	m.type = MSG_PLAYER;
	m.player.id = MSG_PLAYER_ICY_UPDATE;
	web_radio_post(&m);
}


