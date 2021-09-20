//
// Created by JIANG on 2021/9/19.
//

#ifndef MY_LIL9341_TOUCH_H
#define MY_LIL9341_TOUCH_H

#include "main.h"


struct position {
    uint16_t x;
    uint16_t y;
};

uint8_t TP_Init(void);

uint8_t TP_Scan(uint8_t tp);

void TP_Drow_Touch_Point(uint16_t x, uint16_t y, uint16_t color);

extern struct position pos;


#endif //MY_LIL9341_TOUCH_H
