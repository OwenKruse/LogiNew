//
// Created by Owen Kruse on 4/17/24.
//

#ifndef LOGINEW_LOGITACKER_TX_PAYLOAD_PROVIDER_MOUSE_H
#define LOGINEW_LOGITACKER_TX_PAYLOAD_PROVIDER_MOUSE_H

#include "logitacker_tx_payload_provider.h"
#include "logitacker_devices.h"
#include "logitacker_mouse_map.h"


typedef struct {
        logitacker_mouse_map_t * p_mouse_map;
        uint32_t * count;
        bool append_release;
        bool key_combo_transmitted;
        bool release_transmitted;
        hid_mouse_report_t combo_hid_report;
        logitacker_devices_unifying_device_t * p_device;
        bool use_USB;
    } logitacker_tx_payload_provider_mouse_ctx_t;

bool provider_mouse_get_next(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);

bool provider_mouse_get_next_lightspeed(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);

void provider_mouse_reset(logitacker_tx_payload_provider_t * self);

bool provider_mouse_inject_get_next(logitacker_tx_payload_provider_mouse_ctx_t * self, nrf_esb_payload_t *p_next_payload);

bool provider_mouse_inject_get_next_lightspeed(logitacker_tx_payload_provider_mouse_ctx_t * self, nrf_esb_payload_t *p_next_payload);

void provider_mouse_inject_reset(logitacker_tx_payload_provider_mouse_ctx_t * self);

//(self->usb_inject, self->p_device, self->current_task);
logitacker_tx_payload_provider_t * new_payload_provider_mouse(bool use_USB, logitacker_devices_unifying_device_t * p_device_caps, logitacker_mouse_map_t * p_mouse_map, uint32_t * count, bool is_lightspeed);
#endif //LOGINEW_LOGITACKER_TX_PAYLOAD_PROVIDER_MOUSE_H
