/*
 * controls.h
 *
 *  Created on: 13.04.2017
 *      Author: michaelboeckling
 */

#ifndef _CONTROLS_H_
#define _CONTROLS_H_

typedef struct {
    xQueueHandle gpio_evt_queue;
    void *user_data;
} gpio_handler_param_t;


void controls_init();
void controls_destroy();

#endif
