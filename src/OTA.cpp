/**
  ******************************************************************************
  * @file           : ota_update.cpp
  * @brief          : Dual-Bank Internal Flash OTA Update Engine for STM32H7
  *                   Handles HTTP/UART Firmware Streaming, CRC32 Verification,
  *                   and Bank Swapping.
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"
#include <string.h>

/* --- FLASH BANK DEFINITIONS (STM32H743ZI Dual Bank) --- */
#define FLASH_BANK2_BASE_ADDR   0x08100000U // Base Address for OTA Flash Bank 2
#define OTA_HEADER_MAGIC_KEY    0xAA55BEEFU

/* --- OTA FIRMWARE HEADER STRUCT --- */
typedef struct {
    uint32_t magic_key;     // Validation Signature
    uint32_t payload_size;  // Binary size in bytes
    uint32_t crc32_checksum;// Firmware integrity payload CRC
    uint16_t version_major; // Major version
    uint16_t version_minor; // Minor version
} OTA_Header_t;

/* --- FUNCTION PROTOTYPES --- */
HAL_StatusTypeDef OTA_Erase_Bank2(uint32_t payload_size);
HAL_StatusTypeDef OTA_Write_Chunk(uint32_t address_offset, uint32_t *data, uint32_t length_words);
uint32_t OTA_Calculate_CRC32(const uint8_t *pData, uint32_t size);
void OTA_Boot_Swap_Bank(void);

/**
  * @brief Prepares Flash Bank 2 by erasing necessary sectors prior to writing
  */
HAL_StatusTypeDef OTA_Erase_Bank2(uint32_t payload_size) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    HAL_FLASH_Unlock();

    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Banks        = FLASH_BANK_2;
    EraseInitStruct.Sector       = FLASH_SECTOR_0; // Start of Bank 2
    EraseInitStruct.NbSectors    = (payload_size / (128 * 1024)) + 1; // 128KB Sector size
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return HAL_ERROR;
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

/**
  * @brief Writes incoming OTA firmware chunk to Flash Bank 2 using 256-bit Flash programming
  */
HAL_StatusTypeDef OTA_Write_Chunk(uint32_t address_offset, uint32_t *data, uint32_t length_words) {
    HAL_FLASH_Unlock();

    uint32_t current_addr = FLASH_BANK2_BASE_ADDR + address_offset;

    for (uint32_t i = 0; i < length_words; i += 8) { // Write 256 bits (32 bytes) at a time
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, current_addr + (i * 4), (uint32_t)((uint32_t)data + (i * 4))) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

/**
  * @brief Hardware CRC32 verification over written Flash memory
  */
uint32_t OTA_Calculate_CRC32(const uint8_t *pData, uint32_t size) {
    CRC_HandleTypeDef hcrc;
    hcrc.Instance = CRC;
    __HAL_RCC_CRC_CLK_ENABLE();
    
    return HAL_CRC_Calculate(&hcrc, (uint32_t *)pData, size);
}

/**
  * @brief Swaps Flash Banks and resets MCU to boot into new OTA firmware
  */
void OTA_Boot_Swap_Bank(void) {
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    HAL_FLASHEx_OBGetConfig(&OBInit);
    
    // Toggle SWAP_BANK Option Bit
    if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_ENABLE) {
        OBInit.USERConfig &= ~OB_SWAP_BANK_ENABLE;
    } else {
        OBInit.USERConfig |= OB_SWAP_BANK_ENABLE;
    }

    OBInit.OptionType = OPTIONBYTE_USER;
    HAL_FLASHEx_OBProgram(&OBInit);

    HAL_FLASH_OB_Launch(); // Forces System Reset into newly written Bank
}
