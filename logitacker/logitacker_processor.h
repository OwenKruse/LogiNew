//
// Created by root on 5/1/19.
//

#ifndef LOGITACKER_PROCESSOR_H
#define LOGITACKER_PROCESSOR_H

#include "app_usbd_types.h"
#include "app_usbd_hid.h"
#include "logitacker_radio.h"
#include "nrf_esb_illegalmod.h"
#include "bsp.h"


typedef struct logitacker_processor_struct logitacker_processor_t;


typedef void (*p_logitacker_processor_reset_func)(logitacker_processor_t *p_processor);
typedef void (*p_logitacker_processor_init_func)(logitacker_processor_t *p_processor); //does nothing but taking and assigning the context
typedef void (*p_logitacker_processor_deinit_func)(logitacker_processor_t *p_processor); //does nothing but taking and assigning the context
typedef void (*p_logitacker_processor_timer_handler)(logitacker_processor_t *p_processor, void *p_timer_ctx);
typedef void (*p_logitacker_processor_esb_handler)(logitacker_processor_t *p_processor, nrf_esb_evt_t *p_esb_event);
typedef void (*p_logitacker_processor_radio_handler)(logitacker_processor_t *p_processor, radio_evt_t const *p_event);
typedef void (*p_logitacker_processor_bsp_handler)(logitacker_processor_t *p_processor, bsp_event_t event);
typedef void (*p_logitacker_processor_hid_keyboard_event_handler)(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);
typedef void (*p_logitacker_processor_hid_mouse_event_handler)(logitacker_processor_t *p_processor, app_usbd_class_inst_t const *p_inst, app_usbd_hid_user_event_t event);


typedef struct logitacker_processor_struct{
    void *p_ctx;
    p_logitacker_processor_init_func p_init_func;
    p_logitacker_processor_deinit_func p_deinit_func;
    p_logitacker_processor_reset_func p_reset_func;
    p_logitacker_processor_timer_handler p_timer_handler;
    p_logitacker_processor_esb_handler p_esb_handler;
    p_logitacker_processor_radio_handler p_radio_handler;
    p_logitacker_processor_bsp_handler p_bsp_handler;
    p_logitacker_processor_hid_keyboard_event_handler p_usb_hid_keyboard_event_handler;
    p_logitacker_processor_hid_mouse_event_handler p_usb_hid_mouse_event_handler;
} logitacker_processor_t;

#endif //LOGITACKER_PROCESSOR_H
