#ifndef DISPLAY_SYSTEM_H
#define DISPLAY_SYSTEM_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

void DISP_Init(SPI_HandleTypeDef *hspi, GPIO_TypeDef *latchPort, uint16_t latchPin);
void DISP_SetDigits(uint8_t d0, uint8_t d1, uint8_t d2, uint8_t d3);
void DISP_SetTemperature(float temperature);
void DISP_RefreshOnce(void);
void DISP_SetBrightness(uint8_t percent);
void DISP_TestPattern(void);  // New test function

/* Character definitions for easy access */
#define SEG_0      0
#define SEG_1      1
#define SEG_2      2
#define SEG_3      3
#define SEG_4      4
#define SEG_5      5
#define SEG_6      6
#define SEG_7      7
#define SEG_8      8
#define SEG_9      9
#define SEG_A      10
#define SEG_B      11
#define SEG_C      12
#define SEG_D      13
#define SEG_E      14
#define SEG_F      15
#define SEG_BLANK  16
#define SEG_MINUS  17
#define SEG_G      18
#define SEG_H      19
#define SEG_I      20
#define SEG_J      21
#define SEG_L      22
#define SEG_N      23
#define SEG_O      24
#define SEG_P      25
#define SEG_Q      26
#define SEG_R      27
#define SEG_S      28
#define SEG_T      29
#define SEG_U      30
#define SEG_Y      31
#define SEG_Z      32
#define SEG_DEGREE 33  /* ° symbol */

#endif /* DISPLAY_SYSTEM_H */
