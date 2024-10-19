//
// Created by Owen Kruse on 4/17/24.
//

#include "logitacker_tx_payload_provider_mouse.h"
#include <utf.h>
#include <string.h>
#include "logitacker_mouse_map.h"

#define NRF_LOG_MODULE_NAME logitacker_tx_payload_provider_mouse
#include "nrf_log.h"
#include "logitacker_devices.h"


NRF_LOG_MODULE_REGISTER();



bool provider_mouse_get_next_lightspeed(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);
bool provider_mouse_get_next(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload);
void provider_mouse_reset(logitacker_tx_payload_provider_t * self);
bool provider_mouse_inject_get_next(logitacker_tx_payload_provider_mouse_ctx_t * self, nrf_esb_payload_t *p_next_payload);
void provider_mouse_inject_reset(logitacker_tx_payload_provider_mouse_ctx_t * self);

static logitacker_tx_payload_provider_t m_local_provider;
static logitacker_tx_payload_provider_mouse_ctx_t m_local_ctx;

/**
 * @brief Function to get the next payload for a mouse injection
 *
 * This function retrieves the next payload for a mouse injection. It checks if the count of payloads is zero,
 * and if so, returns false. If the count is not zero, it sets up the payload based on whether USB is being used.
 * The payload length is set to 5, the pipe is set to the first byte of the device's RF address, and the data is
 * set to all zeros. The count of payloads is then decremented, and the function returns true.
 *
 * @param self A pointer to the context for the payload provider
 * @param p_next_payload A pointer to the next payload
 *
 * @return A boolean value indicating whether the next payload was successfully retrieved
 */
bool provider_mouse_inject_get_next(logitacker_tx_payload_provider_mouse_ctx_t * self, nrf_esb_payload_t *p_next_payload) {
    if (self->count == 0) {
        return false;
    }

    NRF_LOG_INFO("Injecting mouse payload");
    // Print x move
    NRF_LOG_INFO("X move: %d", self->p_mouse_map->x_move);

    uint8_t mouse_payload[10] = {0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //    //    tmp_data[2] = leftClick ? 1 : 0;
    //    //    tmp_data[2] |= rightClick ? 1 << 1 : 0;
    //    //    tmp_data[4] = (y_move & 0xFFF) >> 8;
    //    //    tmp_data[5] = y_move & 0xFF;
    //    //    tmp_data[6] = (x_move & 0xFFF) >> 8;
    //    //    tmp_data[7] = x_move & 0xFF;
    //    //    tmp_data[8] = scroll_v;

    //Payload at [2] is for the first 8 clicks where 1 is down and 0 is up
    // If just left click, set to 1
    // If just right click, set to 2
    // If both, set to 3
    // If neither, set to 0
    mouse_payload[2] = 0x00;
    if (self->p_mouse_map->leftClick) {
        mouse_payload[2] = 0x01;
    }
    if (self->p_mouse_map->rightClick) {
        mouse_payload[2] = 0x02;
    }
    if (self->p_mouse_map->leftClick && self->p_mouse_map->rightClick) {
        mouse_payload[2] = 0x03;
    }

    // .... 0000 0000 0000 is the x axis
    // 0000 0000 0000 .... is the y axis
    // 4 and 5 are the X axis
    // 5 and 6 are the Y axis
    // 7 is the vertical scroll

    // .... 0000 0000 0000 x = 0
    // 0000 0000 0000 .... y = 0

    // .... 1111 1111 1111 x = -1
    // 1111 1111 1111 .... y = -1

    // .... 0000 0000 0001 x = 1
    // 0000 0000 0001 .... y = 1
    uint32_t cursor_velocity;
    int16_t x_move = self->p_mouse_map->x_move;
    // if x_move is greater than 1000, set it to negative 0 - x_move
    if (x_move > 1000) {
        x_move = 1000 - x_move;
    }
    int16_t y_move = self->p_mouse_map->y_move;
    // if y_move is greater than 1000, set it to negative 0 - y_move
    if (y_move > 1000) {
        y_move = 1000 - y_move;
    }

    cursor_velocity = ((uint32_t)y_move & 0xFFF) << 12 | (x_move & 0xFFF);

    // Set the cursor velocity
    memcpy(mouse_payload + 4, &cursor_velocity, 3);


    memcpy(p_next_payload->data, mouse_payload, 10); // Copy to payload data
    p_next_payload->length = 10; // Set payload length correctly
    p_next_payload->pipe = 0x00; // Set pipe to first byte of RF address

    self->count--;

    return true;
}

bool provider_mouse_inject_get_next_lightspeed(logitacker_tx_payload_provider_mouse_ctx_t * self, nrf_esb_payload_t *p_next_payload) {
    if (self->count == 0) {
        return false;
    }

    NRF_LOG_INFO("Injecting mouse payload");
    // Print x move
    NRF_LOG_INFO("X move: %d", self->p_mouse_map->x_move);

    uint8_t mouse_payload[9] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    //    //    tmp_data[2] = leftClick ? 1 : 0;
    //    //    tmp_data[2] |= rightClick ? 1 << 1 : 0;
    //    //    tmp_data[4] = (y_move & 0xFFF) >> 8;
    //    //    tmp_data[5] = y_move & 0xFF;
    //    //    tmp_data[6] = (x_move & 0xFFF) >> 8;
    //    //    tmp_data[7] = x_move & 0xFF;
    //    //    tmp_data[8] = scroll_v;

    //Payload at [2] is for the first 8 clicks where 1 is down and 0 is up
    // If just left click, set to 1
    // If just right click, set to 2
    // If both, set to 3
    // If neither, set to 0
    mouse_payload[1] = 0x00;
    if (self->p_mouse_map->leftClick) {
        mouse_payload[1] = 0x01;
    }
    if (self->p_mouse_map->rightClick) {
        mouse_payload[1] = 0x02;
    }
    if (self->p_mouse_map->leftClick && self->p_mouse_map->rightClick) {
        mouse_payload[1] = 0x03;
    }

    // 0000 0000 0000 0000 is the x axis
    // 0000 0000 0000 0000 is the y axis
    // 4 and 5 are the X axis
    // 5 and 6 are the Y axis
    // 7 is the vertical scroll

    // 0000 0000 0000 0000 x = 0
    // 0000 0000 0000 0000 y = 0

    // 1111 1111 1111 1111 x = -1
    // 1111 1111 1111 1111 y = -1

    // 0000 0000 0000 0001 x = 1
    // 0000 0000 0000 0001 y = 1
    int16_t x_move = self->p_mouse_map->x_move;
    // if x_move is greater than 16383, set it to negative 0 - x_move
    if (x_move > 16383) {
        x_move = 16383 - x_move;
    }
    int16_t y_move = self->p_mouse_map->y_move;
    // if y_move is greater than 16383, set it to negative 0 - y_move
    if (y_move > 16383) {
        y_move = 16383 - y_move;
    }

    // 3 and 4 are for the x axis

    mouse_payload[3] = (x_move & 0xFF00) >> 8;
    mouse_payload[4] = x_move & 0xFF;

    // 5 and 6 are for the y axis

    mouse_payload[5] = (y_move & 0xFF00) >> 8;
    mouse_payload[6] = y_move & 0xFF;


    // 7 is for the vertical scroll
    mouse_payload[7] = self->p_mouse_map->scroll_v;

    memcpy(p_next_payload->data, mouse_payload, 9); // Copy to payload data
    p_next_payload->length = 9; // Set payload length correctly

    p_next_payload->pipe = 0x00; // Set pipe to 0x00

    self->count--;

    return true;
}

void provider_mouse_inject_reset(logitacker_tx_payload_provider_mouse_ctx_t * self) {
    self->count = 0;
}


/**
 * @brief Function to get the next payload for a mouse
 *
 * This function retrieves the next payload for a mouse. It checks if the key combo has been transmitted, and if so,
 * checks if the release should be appended. If the release should be appended and has not been transmitted, the
 * function injects the next payload and returns true. If the key combo has not been transmitted, the function injects
 * the next payload and returns true. If neither of these conditions are met, the function returns false.
 *
 * @param self A pointer to the context for the payload provider
 * @param p_next_payload A pointer to the next payload
 *
 * @return A boolean value indicating whether the next payload was successfully retrieved
 */

bool provider_mouse_get_next(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload) {
    logitacker_tx_payload_provider_mouse_ctx_t * p_ctx = (logitacker_tx_payload_provider_mouse_ctx_t *)self->p_ctx;

    if(p_ctx->key_combo_transmitted) {
        if(p_ctx->append_release && !

                p_ctx->release_transmitted) {
            p_ctx->release_transmitted = true;
            return provider_mouse_inject_get_next(p_ctx, p_next_payload);
        } else {
            return false;
        }
    } else {
        p_ctx->key_combo_transmitted = true;
        return provider_mouse_inject_get_next(p_ctx, p_next_payload);
    }

}

bool provider_mouse_get_next_lightspeed(logitacker_tx_payload_provider_t * self, nrf_esb_payload_t *p_next_payload) {
    logitacker_tx_payload_provider_mouse_ctx_t * p_ctx = (logitacker_tx_payload_provider_mouse_ctx_t *)self->p_ctx;

    if(p_ctx->key_combo_transmitted) {
        if(p_ctx->append_release && ! p_ctx->release_transmitted) {
            p_ctx->release_transmitted = true;
            return provider_mouse_inject_get_next_lightspeed(p_ctx, p_next_payload);
        } else {
            return false;
        }
    } else {
        p_ctx->key_combo_transmitted = true;
        return provider_mouse_inject_get_next_lightspeed(p_ctx, p_next_payload);
    }

}


void provider_mouse_reset(logitacker_tx_payload_provider_t * self) {
    logitacker_tx_payload_provider_mouse_ctx_t * p_ctx = (logitacker_tx_payload_provider_mouse_ctx_t *)self->p_ctx;
    provider_mouse_inject_reset(p_ctx);
    return;
}

/**
 * @brief Function to create a new payload provider for a mouse
 *
 * This function initializes a new payload provider for a mouse. It sets up the context for the payload provider,
 * including the mouse map, button, direction, count, and whether to use USB. It also sets up the function pointers
 * for getting the next payload and resetting the provider.
 *
 * @param use_USB A boolean value indicating whether to use USB
 * @param p_device_caps A pointer to the device capabilities
 * @param p_mouse_map A pointer to the mouse map
 * @param count The count of payloads
 *
 * @return A pointer to the newly created payload provider
 */

logitacker_tx_payload_provider_t * new_payload_provider_mouse(bool use_USB, logitacker_devices_unifying_device_t * p_device_caps, logitacker_mouse_map_t * p_mouse_map, uint32_t * count, bool is_lightspeed) {
    logitacker_tx_payload_provider_mouse_ctx_t * p_ctx = &m_local_ctx;

    p_ctx->p_mouse_map = p_mouse_map;
    p_ctx->count = count;
    p_ctx->append_release = true;
    p_ctx->key_combo_transmitted = false;
    p_ctx->release_transmitted = false;
    p_ctx->p_device = p_device_caps;
    p_ctx->use_USB = use_USB;

    m_local_provider.p_ctx = p_ctx;
    m_local_provider.p_get_next = is_lightspeed ? provider_mouse_get_next_lightspeed : provider_mouse_get_next;
    m_local_provider.p_reset = provider_mouse_reset;

    return &m_local_provider;
}




