/******************************************************************************
 * Copyright 2013-2015 Espressif Systems
 *
 * FileName: user_main.c
 *
 * Description: Routines to use a SPI RAM chip as a big FIFO buffer. Multi-
 * thread-aware: the reading and writing can happen in different threads and
 * will block if the fifo is empty and full, respectively.
 *
 * Modification history:
 *     2015/06/02, v1.0 File created.
 *     2019/08/14, clean code for icy-meta-parser
 *******************************************************************************/
#include "esp_system.h"
#include "string.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "web_radio.h"
#include "playerconfig.h"

#include "mz_fifo.h"

#define SPIRAMSIZE 32000

static mz_fifo_handle_t fifo;
static void trigger_fifo_ui_refresh() {
	msg m;
	m.type = MSG_FIFO;
	m.fifo.id = MSG_FIFO_UPDATED;
	web_radio_post(&m);
}

//Initialize the FIFO
int spiRamFifoInit() {
	mz_fifo_init();
	fifo = mz_fifo_create_fifo(SPIRAMSIZE);
	return 1;
}

void spiRamFifoReset() {
	mz_fifo_reset(fifo);
}

//Read bytes from the FIFO
void spiRamFifoRead(char *buff, int len) {
	mz_fifo_read(fifo, buff, len);
	trigger_fifo_ui_refresh();
}

/*
                Data                    Meta                Data
    |--------------------------------|---|----|--------------------------------|
    '------ icy_meta_interval -------'        '------ icy_meta_interval -------'
                                     '-.-'--.-'
    metadata_len_byte / 16:------------'    ;
    metadata_content :---------------------'-----> StreamTitle='Song title';

*/


/*
 * Write bytes to the FIFO
 * @returns amount of bytes inside the buffer
 */
void spiRamFifoWrite(const char *buff, int buffLen) {
	mz_fifo_write(fifo, buff, buffLen);
	trigger_fifo_ui_refresh();
}


//Get amount of bytes in use
int spiRamFifoFill() {
	// MZ : here mux lock was taken... why ? Let return a dirty value
	return mz_fifo_get_filled_size(fifo);
}


int spiRamFifoLen() {
	return SPIRAMSIZE;
}

int spiRamFifoFillPercentage() {
	return (spiRamFifoFill() * 100 / spiRamFifoLen());
}




