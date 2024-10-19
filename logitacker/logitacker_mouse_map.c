//
// Created by Owen Kruse on 4/17/24.
//

#include "logitacker_mouse_map.h"
#include <string.h>
#include <stdint.h>


#include "nrf_log.h"


// This file provides the mapping of the mouse buttons and movement to the USB HID report format.



uint32_t logitacker_mouse_map_to_hid_report(uint16_t x_move, uint16_t y_move, uint8_t scroll_v, uint8_t scroll_h, bool leftClick, bool rightClick) {
    uint8_t mouse_payload[] = {0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint32_t cursor_velocity;

    cursor_velocity = ((uint32_t)y_move & 0xFFF) << 12 | (x_move & 0xFFF);

    memcpy(mouse_payload + 4, &cursor_velocity, 3);

    if(leftClick)
        mouse_payload[2] = 1;

    if(rightClick)
        mouse_payload[2] |= 1 << 1;

    mouse_payload[7] = scroll_v;
    mouse_payload[8] = scroll_h;

    return *((uint32_t*)mouse_payload);

}

logitacker_mouse_map_t * logitacker_mouse_map_get_from_data(uint8_t data[10]) {
    static logitacker_mouse_map_t mouse_map = {
            .x_move = 0,
            .y_move = 0,
            .scroll_v = 0,
            .scroll_h = 0,
            .leftClick = false,
            .rightClick = false
    };

    // Use a loop to print out each byte of the data using NRF_LOG_INFO
    //    if (leftClick) {
    //        tmp_data[2] = 1;
    //    }
    //    if (rightClick) {
    //        tmp_data[2] = 2;
    //    }
    //    if (leftClick && rightClick) {
    //        tmp_data[2] = 3;
    //    }
    //   Payload at [4] is for the y movement of the mouse
    //    mouse_payload[4] = (y_move & 0xFFF) >> 8;
    //    mouse_payload[5] = y_move & 0xFF;
    //
    //    //Payload at [6] is for the x movement of the mouse
    //    mouse_payload[6] = (x_move & 0xFFF) >> 8;
    //    mouse_payload[7] = x_move & 0xFF;
    //
    //    //Payload at [8] is for the vertical scroll of the mouse
    //    mouse_payload[8] = scroll_v;

    NRF_LOG_INFO("Mouse data: ");
    for (int i = 0; i < 10; i++) {
        NRF_LOG_INFO("%d: %d", i, data[i]);
    }

    if (data[2] == 1) {
        mouse_map.leftClick = true;
        mouse_map.rightClick = false;
    }
    else if (data[2] == 2) {
        mouse_map.leftClick = false;
        mouse_map.rightClick = true;
    }
    else if (data[2] == 3) {
        mouse_map.leftClick = true;
        mouse_map.rightClick = true;
    }
    else {
        mouse_map.leftClick = false;
        mouse_map.rightClick = false;
    }

    int16_t x_move = (data[6] << 8) | data[7];
    int16_t y_move = (data[4] << 8) | data[5];

    int8_t scroll_v = data[8];
    int8_t scroll_h = 0;

    mouse_map.x_move = x_move;
    mouse_map.y_move = y_move;
    mouse_map.scroll_v = scroll_v;
    mouse_map.scroll_h = scroll_h;

    return &mouse_map;



}
logitacker_mouse_map_t * logitacker_mouse_map_get_from_data_lightspeed(uint8_t data[11]) {
    static logitacker_mouse_map_t mouse_map = {
            .x_move = 0,
            .y_move = 0,
            .scroll_v = 0,
            .scroll_h = 0,
            .leftClick = false,
            .rightClick = false
    };

    // Use a loop to print out each byte of the data using NRF_LOG_INFO
    //    if (leftClick) {
    //        tmp_data[2] = 1;
    //    }
    //    if (rightClick) {
    //        tmp_data[2] = 2;
    //    }
    //    if (leftClick && rightClick) {
    //        tmp_data[2] = 3;
    //    }
    //   Payload at [4] is for the y movement of the mouse
    //    mouse_payload[4] = (y_move & 0xFFF) >> 8;
    //    mouse_payload[5] = y_move & 0xFF;
    //
    //    //Payload at [6] is for the x movement of the mouse
    //    mouse_payload[6] = (x_move & 0xFFF) >> 8;
    //    mouse_payload[7] = x_move & 0xFF;
    //
    //    //Payload at [8] is for the vertical scroll of the mouse
    //    mouse_payload[8] = scroll_v;

    NRF_LOG_INFO("Mouse data: ");
    for (int i = 0; i < 10; i++) {
        NRF_LOG_INFO("%d: %d", i, data[i]);
    }

    if (data[2] == 1) {
        mouse_map.leftClick = true;
        mouse_map.rightClick = false;
    }
    else if (data[2] == 2) {
        mouse_map.leftClick = false;
        mouse_map.rightClick = true;
    }
    else if (data[2] == 3) {
        mouse_map.leftClick = true;
        mouse_map.rightClick = true;
    }
    else {
        mouse_map.leftClick = false;
        mouse_map.rightClick = false;
    }

    // The lightspeed mouse data uses bytes 4-5 for the x-axis and 6-7 for the y-axis
    int16_t x_move = (data[4] << 8) | data[5];
    int16_t y_move = (data[6] << 8) | data[7];

    int8_t scroll_v = data[8];
    int8_t scroll_h = 0;

    mouse_map.x_move = x_move;
    mouse_map.y_move = y_move;
    mouse_map.scroll_v = scroll_v;
    mouse_map.scroll_h = scroll_h;

    return &mouse_map;



}



logitacker_mouse_map_t * logitacker_mouse_map_get() {
    static logitacker_mouse_map_t mouse_map = {
            .x_move = 0,
            .y_move = 0,
            .scroll_v = 0,
            .scroll_h = 0,
            .leftClick = false,
            .rightClick = false
    };

    return &mouse_map;
}

logitacker_mouse_map_t * logitacker_mouse_map_build(int16_t x_move, int16_t y_move, int8_t scroll_v, int8_t scroll_h, bool leftClick, bool rightClick) {
    static logitacker_mouse_map_t mouse_map = {
            .x_move = 0,
            .y_move = 0,
            .scroll_v = 0,
            .scroll_h = 0,
            .leftClick = false,
            .rightClick = false
    };

    mouse_map.x_move = x_move;
    mouse_map.y_move = y_move;
    mouse_map.scroll_v = scroll_v;
    mouse_map.scroll_h = scroll_h;
    mouse_map.leftClick = leftClick;
    mouse_map.rightClick = rightClick;

    return &mouse_map;
}

// Path: logitacker/logitacker_mouse_map.c