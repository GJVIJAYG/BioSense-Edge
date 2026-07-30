/**
  ******************************************************************************
  * @file           : lora_protocol.cpp
  * @brief          : SX1276 LoRa SPI Transceiver Driver for STM32H7
  *                   Handles Failover Radio Telemetry Transmission
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"
#include <string.h>

/* --- SX1276 REGISTER MAP --- */
#define REG_FIFO                    0x00
#define REG_OP_MODE                 0x01
#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08
#define REG_PA_CONFIG               0x09
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_ADDR       0x0E
#define REG_IRQ_FLAGS               0x12
#define REG_MODEM_CONFIG_1          0x1D
#define REG_MODEM_CONFIG_2          0x1E
#define REG_PAYLOAD_LENGTH          0x22

/* --- LORA MODES --- */
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_SLEEP                  0x00
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03

/* --- HARDWARE PIN DEFINITIONS --- */
#define LORA_NSS_PORT               GPIOA
#define LORA_NSS_PIN                GPIO_PIN_4
#define LORA_RESET_PORT             GPIOC
#define LORA_RESET_PIN              GPIO_PIN_7

extern SPI_HandleTypeDef hspi1; // Hardware SPI handle initialized in CubeMX

/* --- LOW-LEVEL SPI FUNCTIONS --- */
static void LoRa_Select(void) {
    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_RESET);
}

static void LoRa_Deselect(void) {
    HAL_GPIO_WritePin(LORA_NSS_PORT, LORA_NSS_PIN, GPIO_PIN_SET);
}

static uint8_t LoRa_ReadRegister(uint8_t address) {
    uint8_t tx_buf[2] = { (uint8_t)(address & 0x7F), 0x00 };
    uint8_t rx_buf[2] = { 0 };

    LoRa_Select();
    HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, 2, 50);
    LoRa_Deselect();

    return rx_buf[1];
}

static void LoRa_WriteRegister(uint8_t address, uint8_t value) {
    uint8_t tx_buf[2] = { (uint8_t)(address | 0x80), value };

    LoRa_Select();
    HAL_SPI_Transmit(&hspi1, tx_buf, 2, 50);
    LoRa_Deselect();
}

/* --- DRIVER API IMPLEMENTATION --- */

/**
  * @brief Reset and Initialize SX1276 Module to 868MHz LoRa Mode
  */
uint8_t LoRa_Init(void) {
    // 1. Hardware Reset Pulse
    HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LORA_RESET_PORT, LORA_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    // 2. Check Device Version (0x12 is default for SX1276)
    uint8_t version = LoRa_ReadRegister(0x42);
    if (version != 0x12) {
        return 0; // Initialization Failed
    }

    // 3. Put in Sleep Mode to enable LoRa Mode change
    LoRa_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    HAL_Delay(10);

    // 4. Set Carrier Frequency to 868 MHz (FRF = (868MHz * 2^19) / 32MHz)
    LoRa_WriteRegister(REG_FRF_MSB, 0xD9);
    LoRa_WriteRegister(REG_FRF_MID, 0x00);
    LoRa_WriteRegister(REG_FRF_LSB, 0x00);

    // 5. Set Power Output +17dBm
    LoRa_WriteRegister(REG_PA_CONFIG, 0x8F);

    // 6. Set Modem Config (BW 125kHz, CR 4/5, Explicit Header)
    LoRa_WriteRegister(REG_MODEM_CONFIG_1, 0x72);
    LoRa_WriteRegister(REG_MODEM_CONFIG_2, 0x74); // SF7

    // 7. Set Standby Mode
    LoRa_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    return 1; // Success
}

/**
  * @brief Transmit Packet over LoRa Radio
  */
void LoRa_SendPacket(uint8_t* buffer, uint8_t size) {
    // Put module in Standby
    LoRa_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    // Reset FIFO Pointer
    LoRa_WriteRegister(REG_FIFO_ADDR_PTR, 0x00);
    LoRa_WriteRegister(REG_FIFO_TX_BASE_ADDR, 0x00);

    // Write Payload to FIFO
    for (uint8_t i = 0; i < size; i++) {
        LoRa_WriteRegister(REG_FIFO, buffer[i]);
    }

    // Set Payload Length
    LoRa_WriteRegister(REG_PAYLOAD_LENGTH, size);

    // Trigger Transmission
    LoRa_WriteRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    // Wait until TxDone flag is set in IRQ Flags
    while ((LoRa_ReadRegister(REG_IRQ_FLAGS) & 0x08) == 0) {
        HAL_Delay(1);
    }

    // Clear IRQ Flags
    LoRa_WriteRegister(REG_IRQ_FLAGS, 0xFF);
}
