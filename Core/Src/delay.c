//
// Created by JIANG on 2021/9/19.
//

#include "delay.h"
#include "main.h"

void HAL_delay_us(__IO uint32_t nTime) {
    int old_val, new_val, val;

    if (nTime > 900) {
        for (old_val = 0; old_val < nTime / 900; old_val++) {
            HAL_delay_us(900);
        }
        nTime = nTime % 900;
    }

    old_val = SysTick->VAL;
    new_val = old_val - HAL_RCC_GetHCLKFreq() / 1000000 * nTime;
    if (new_val >= 0) {
        do {
            val = SysTick->VAL;
        } while ((val < old_val) && (val >= new_val));
    } else {
        new_val += HAL_RCC_GetHCLKFreq() / 1000000 * 1000;
        do {
            val = SysTick->VAL;
        } while ((val <= old_val) || (val > new_val));
    }
}