#include "flash_eeprom.h"
#include <string.h>

// Each variable entry: [address(32-bit) | data(32-bit)]
typedef struct {
    uint32_t addr;
    uint32_t data;
} EE_Entry;

// Local state
static uint32_t next_write_pos = 0;
static uint8_t initialized = 0;

// Calculate actual flash address for a slot
static uint32_t get_slot_address(uint32_t slot) {
    return FLASH_USER_START_ADDR + (slot * sizeof(EE_Entry));
}

// Find the latest value for a given address
static EE_Status find_latest_value(EE_VarAddr addr, uint32_t* data) {
    EE_Entry entry;

    // Search from newest to oldest
    for (int32_t slot = (int32_t)next_write_pos - 1; slot >= 0; slot--) {
        uint32_t slot_addr = get_slot_address(slot);
        memcpy(&entry, (void*)slot_addr, sizeof(EE_Entry));

        // Check if this entry is valid (not 0xFFFFFFFF)
        if (entry.addr != 0xFFFFFFFF && entry.data != 0xFFFFFFFF) {
            if (entry.addr == (uint32_t)addr) {
                *data = entry.data;
                return EE_OK;
            }
        }
    }

    return EE_NOT_FOUND;
}

// Initialize EEPROM emulation
void EE_Init(void) {
    if (initialized) return;

    // Find next empty slot
    EE_Entry entry;
    for (next_write_pos = 0; next_write_pos < (FLASH_USER_SIZE / sizeof(EE_Entry)); next_write_pos++) {
        uint32_t slot_addr = get_slot_address(next_write_pos);
        memcpy(&entry, (void*)slot_addr, sizeof(EE_Entry));

        // Check if slot is empty (all 0xFFFFFFFF)
        if (entry.addr == 0xFFFFFFFF && entry.data == 0xFFFFFFFF) {
            break;
        }
    }

    initialized = 1;
}

// Write a 32-bit value
EE_Status EE_WriteU32(EE_VarAddr addr, uint32_t value) {
    if (!initialized) return EE_ERROR;

    // Check if we have space
    if (next_write_pos >= (FLASH_USER_SIZE / sizeof(EE_Entry))) {
        // No space - erase sector and start over
        EE_Format();
        next_write_pos = 0;
    }

    // Prepare entry
    EE_Entry entry;
    entry.addr = (uint32_t)addr;
    entry.data = value;

    // Calculate write address
    uint32_t write_addr = get_slot_address(next_write_pos);

    // Unlock flash
    HAL_FLASH_Unlock();

    // Write entry (two 32-bit words)
    HAL_StatusTypeDef status1 = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                                   write_addr,
                                                   entry.addr);

    HAL_StatusTypeDef status2 = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                                   write_addr + 4,
                                                   entry.data);

    // Lock flash
    HAL_FLASH_Lock();

    if (status1 == HAL_OK && status2 == HAL_OK) {
        next_write_pos++;
        return EE_OK;
    }

    return EE_ERROR;
}

// Read a 32-bit value
EE_Status EE_ReadU32(EE_VarAddr addr, uint32_t* value) {
    if (!initialized) return EE_ERROR;

    return find_latest_value(addr, value);
}

// Write a float value
EE_Status EE_WriteFloat(EE_VarAddr addr, float value) {
    // Convert float to 32-bit representation
    uint32_t int_value = *(uint32_t*)&value;
    return EE_WriteU32(addr, int_value);
}

// Read a float value
EE_Status EE_ReadFloat(EE_VarAddr addr, float* value) {
    uint32_t int_value;
    EE_Status status = EE_ReadU32(addr, &int_value);

    if (status == EE_OK) {
        *value = *(float*)&int_value;
    }

    return status;
}

// Erase entire EEPROM area
void EE_Format(void) {
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_USER_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sectorError = 0;
    HAL_FLASHEx_Erase(&erase, &sectorError);

    HAL_FLASH_Lock();

    next_write_pos = 0;
    initialized = 1;
}
