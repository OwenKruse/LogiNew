//
// Created by Owen Kruse on 4/17/24.
//

#ifndef LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H
#define LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H

#include <stdint.h>
#include <stdbool.h>


typedef struct {
    uint16_t x_move;
    uint16_t y_move;
    uint8_t scroll_v;
    uint8_t scroll_h;
    bool leftClick;
    bool rightClick;
} hid_mouse_report_t;


uint32_t logitacker_mouse_map_to_hid_report(uint16_t x_move, uint16_t y_move, uint8_t scroll_v, uint8_t scroll_h, bool leftClick, bool rightClick);


typedef struct {
    uint16_t x_move;
    uint16_t y_move;
    uint8_t scroll_v;
    uint8_t scroll_h;
    bool leftClick;
    bool rightClick;
} logitacker_mouse_map_t;

logitacker_mouse_map_t * logitacker_mouse_map_get();
logitacker_mouse_map_t * logitacker_mouse_map_get_from_data(uint8_t data[10]);
logitacker_mouse_map_t * logitacker_mouse_map_get_from_data_lightspeed(uint8_t data[11]);
logitacker_mouse_map_t * logitacker_mouse_map_build(int16_t x_move, int16_t y_move, int8_t scroll_v, int8_t scroll_h, bool leftClick, bool rightClick);


#endif //LOGINEW_LOGITACKER_PROCESSOR_MOUSE_MAP_H




