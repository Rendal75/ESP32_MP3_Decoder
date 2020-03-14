/*
 * web_radio.h
 *
 *  Created on: 13.03.2017
 *      Author: michaelboeckling
 */

#ifndef INCLUDE_WEB_RADIO_H_
#define INCLUDE_WEB_RADIO_H_

#include "audio_player.h"
#include "playlist.h"

typedef struct {
    player_t *player_config;
    playlist_t *playlist;
} web_radio_t;


typedef enum { MSG_HTTP, MSG_CTRL, MSG_TIMER, MSG_PLAYER, MSG_FIFO } msg_type;
typedef enum { MSG_HTTP_CONNECTING, MSG_HTTP_CONNECTED, MSG_HTTP_CONNECTION_FINISHED, MSG_HTTP_RADIO_UPDATE } msg_type_http;
typedef enum { MSG_CTRL_GPIO } msg_type_ctrl;
typedef enum { MSG_TIMER_1S, MSG_TIMER_10S } msg_type_timer;
typedef enum { MSG_PLAYER_ICY_UPDATE } msg_type_player;
typedef enum { MSG_FIFO_UPDATED } msg_type_fifo;

typedef struct msg_http {
	msg_type_http id;
} msg_http;

typedef struct msg_fifo {
	msg_type_fifo id;
} msg_fifo;

typedef struct msg_timer {
	msg_type_timer id;
} msg_timer;

typedef struct msg_player {
	msg_type_player id;
} msg_player;

typedef struct msg_ctrl {
	uint32_t gpio_num;
} msg_ctrl;

typedef struct msg {
	msg_type type;
	union  {
		msg_http http;
		msg_ctrl ctrl;
		msg_timer timer;
		msg_player player;
		msg_fifo fifo;
	};
} msg;

void web_radio_init(web_radio_t *config);
void web_radio_start();
void web_radio_post(msg* m);
void web_radio_post_from_isr(msg* m);

void screen_on_meta(char* meta);
void screen_on_entry_changed(playlist_entry_t* entry);

#define WEBRADIO_TASK_STATS 0

#endif /* INCLUDE_WEB_RADIO_H_ */
