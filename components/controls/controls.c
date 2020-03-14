/*
 * controls.c
 *
 *  Created on: 13.04.2017
 *      Author: michaelboeckling
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "controls.h"
#include "web_radio.h"

#define ESP_INTR_FLAG_DEFAULT 0

/* gpio event handler */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    msg m;
    m.type = MSG_CTRL;
    m.ctrl.gpio_num = (uint32_t) arg;

    web_radio_post_from_isr(&m);
}

void controls_init()
{
    gpio_config_t io_conf;

    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO0 here ("Boot" button)
    io_conf.pin_bit_mask = (1 << GPIO_NUM_0);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    // remove existing handler that may be present
    gpio_isr_handler_remove(GPIO_NUM_0);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_NUM_0, gpio_isr_handler, (void*) GPIO_NUM_0);
}

void controls_destroy()
{
    gpio_isr_handler_remove(GPIO_NUM_0);
    // TODO: free gpio_handler_param_t params
}
