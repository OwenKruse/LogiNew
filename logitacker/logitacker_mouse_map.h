//
// Created by Owen Kruse on 4/17/24.
//

#ifndef LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H
#define LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H

#endif //LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H
// Strut
typedef struct {
    uint8_t buttons;
    uint8_t x;
    uint8_t y;
    uint8_t wheel;
} hid_mouse_report_t;


uint32_t logitacker_mouse_map_to_hid_report(uint16_t x_move, uint16_t y_move, uint8_t scroll_v, uint8_t scroll_h, bool leftClick, bool rightClick, hid_mouse_report_t *p_report);

