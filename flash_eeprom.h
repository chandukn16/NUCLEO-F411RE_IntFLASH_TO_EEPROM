#ifndef FLASH_EEPROM_H
#define FLASH_EEPROM_H

#include "stm32f4xx_hal.h"

// For STM32F411 - Use Sector 5 (128KB at 0x08020000)
#define FLASH_USER_START_ADDR   0x08020000UL  // Sector 5
#define FLASH_USER_SECTOR       FLASH_SECTOR_5

// We'll use first 4KB of this sector for our variables (can be expanded)
#define FLASH_USER_SIZE         4096

// Return codes
typedef enum {
    EE_OK = 0,
    EE_ERROR,
    EE_NOT_FOUND,
    EE_FULL
} EE_Status;

// Variable addresses for your application
typedef enum {
    EE_ADDR_SETPOINT1 = 0,      // 4 bytes (float)
    EE_ADDR_SETPOINT2 = 1,      // 4 bytes
    EE_ADDR_HYSTERESIS1 = 2,    // 4 bytes
    EE_ADDR_HYSTERESIS2 = 3,    // 4 bytes
    EE_ADDR_LAST_TEMP = 4,      // 4 bytes (optional: last measured temp)
    EE_ADDR_BOOT_COUNT = 5,     // 4 bytes (for debugging)
    EE_ADDR_RESERVED = 6        // Start of any additional data
} EE_VarAddr;

// Simple API
void EE_Init(void);
EE_Status EE_WriteFloat(EE_VarAddr addr, float value);
EE_Status EE_ReadFloat(EE_VarAddr addr, float* value);
EE_Status EE_WriteU32(EE_VarAddr addr, uint32_t value);
EE_Status EE_ReadU32(EE_VarAddr addr, uint32_t* value);
void EE_Format(void);  // Erase everything (use carefully!)

#endif
