/*
 * audio_player.c
 *
 *  Created on: 12.03.2017
 *      Author: michaelboeckling
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "audio_player.h"
#include "spiram_fifo.h"

#include "esp_system.h"
#include "esp_log.h"

#include "fdk_aac_decoder.h"
#include "libfaad_decoder.h"
#include "controls.h"
#include "screen.h"
#include "web_radio.h"
#include "audio_decoder.h"
#include "icy_filter_stream.h"

#define TAG "audio_player"
#define PRIO_MAD configMAX_PRIORITIES - 2


extern audio_decoder_t audio_decoder_mp3;

void on_meta(char* meta) {
	//ESP_LOGW(TAG, "On Meta:%s", meta);
	screen_on_meta(meta);
}
static icy_filter_stream filter_stream = {
	.out = spiRamFifoWrite,
	.on_meta = on_meta
};

#define MSG_QUEUE_SIZE 5
static QueueHandle_t msg_queue;

typedef struct audio_player_msg {
	player_command_t command;
} audio_player_msg_t;

static player_t *player_instance = NULL;
static component_status_t player_status = UNINITIALIZED;
static audio_decoder_t* decoder_instance = NULL;

static int start_decoder(player_t *player)
{
	audio_decoder_t* decoder = NULL;

    ESP_LOGI(TAG, "RAM left %d", esp_get_free_heap_size());


    switch (player->media_stream->content_type)
    {
        case AUDIO_MPEG:
        	decoder = &audio_decoder_mp3;
            break;

        	/*
        case AUDIO_MP4:
            task_func = libfaac_decoder_task;
            task_name = "libfaac_decoder_task";
            stack_depth = 55000;
            break;

        case AUDIO_AAC:
        case OCTET_STREAM: // probably .aac
            task_func = fdkaac_decoder_task;
            task_name = "fdkaac_decoder_task";
            stack_depth = 6144;
            break;
*/
        default:
            ESP_LOGE(TAG, "unknown mime type: %d", player->media_stream->content_type);
            return -1;
    }

    decoder_instance = decoder;
    audio_player_msg_t m;
    if(xQueueSend(msg_queue, &m, 20 / portTICK_PERIOD_MS) != pdPASS ) {
		ESP_LOGE(TAG, "Failed to post message, queue full!");
		return -1;
	}

    return 0;
}


/* Writes bytes into the FIFO queue, starts decoder task if necessary. */
int audio_stream_consumer(const char *recv_buf, ssize_t bytes_read,
        void *user_data)
{
	assert(bytes_read);
    player_t *player = user_data;

    if(player->decoder_status==STOPPED) {
    	ESP_LOGW(TAG, "Throwing data since player STOPPED");
    	return -1;
    }

    icy_filter_stream_write(&filter_stream, recv_buf, bytes_read);

    int bytes_in_buf = spiRamFifoFill();
    uint8_t fill_level = (bytes_in_buf * 100) / spiRamFifoLen();

    // seems 4k is enough to prevent initial buffer underflow
    uint8_t min_fill_lvl = player->buffer_pref == BUF_PREF_FAST ? 20 : 90;
    bool enough_buffer = fill_level > min_fill_lvl;

    bool early_start = (bytes_in_buf > 1028 && player->media_stream->eof);
    if ((enough_buffer || early_start)) {
    	if(player->decoder_status != RUNNING ) {
    		player->decoder_status = RUNNING;
			// buffer is filled, start decoder
			if (start_decoder(player) != 0) {
				ESP_LOGE(TAG, "failed to start decoder task");
				return -1;
			}
    	}
    }

    return 0;
}

static void player_task(void* param) {
	for(;;) {
		audio_player_msg_t m;
		if(xQueueReceive(msg_queue, &m, 1000/portTICK_PERIOD_MS)) {
			if(decoder_instance==NULL) {
				ESP_LOGE(TAG, "Decoder is NULL !");
				continue;
			}
			decoder_instance->start(player_instance);
			decoder_instance = NULL;
			player_instance->decoder_status=STOPPED;
		}
	}
}

void audio_player_init(player_t *player)
{
	msg_queue = xQueueCreate(MSG_QUEUE_SIZE, sizeof(msg));

    player_instance = player;
    player_status = INITIALIZED;
    if (xTaskCreatePinnedToCore(player_task, "player-task", 9000, NULL,
        PRIO_MAD, NULL, 1) != pdPASS) {
            ESP_LOGE(TAG, "ERROR creating decoder task! Out of memory?");
   } else {
		player->decoder_status = RUNNING;
	}

}

void audio_player_destroy()
{
    renderer_destroy();
    player_status = UNINITIALIZED;
}

void audio_player_start(int icymeta_interval)
{
	ESP_LOGI(TAG, "Starting player");
    renderer_start();
    player_instance->media_stream->eof = false;
    player_status = RUNNING;
    player_instance->decoder_status=INITIALIZED;
    player_instance->decoder_command = CMD_NONE;

    filter_stream.icy_interval = icymeta_interval;
    icy_filter_stream_init(&filter_stream);
}

void audio_player_stop()
{
	ESP_LOGI(TAG, "Stopping player");
    //renderer_stop();
    player_instance->decoder_command = CMD_STOP;
    // player_status = STOPPED;
}

component_status_t get_player_status()
{
    return player_status;
}

