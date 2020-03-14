/*
 * audio_decoder.h
 *
 *  Created on: Jan 16, 2020
 *      Author: mzmarzlo
 */

#ifndef COMPONENTS_AUDIO_PLAYER_INCLUDE_AUDIO_DECODER_H_
#define COMPONENTS_AUDIO_PLAYER_INCLUDE_AUDIO_DECODER_H_

#include "audio_player.h"

typedef struct audio_decoder {
	/**
	 * Blocking call that starts the player, allocates all needed resources and start reading from fifo and rendering
	 *
	 */
	void (*start)(player_t *player);

	/**
	 * As a result of that call the start method shall return and release allocated resources on start
	 */
	void (*stop)();
} audio_decoder_t;

#endif /* COMPONENTS_AUDIO_PLAYER_INCLUDE_AUDIO_DECODER_H_ */
