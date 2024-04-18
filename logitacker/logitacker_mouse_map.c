//
// Created by Owen Kruse on 4/17/24.
//

#include "logitacker_mouse_map.h"


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

    return *((uint32_t *)mouse_payload);

}

// Path: logitacker/logitacker_mouse_map.c