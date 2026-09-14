/*
 * main.c - STM32F746NGH6 Custom In-Field Bootloader
 *
 * Resident in Sector 0 (0x08000000 - 0x08007FFF, 32 KB).
 * Completely independent, unbrickable, and communication-compatible
 * with MnpControl via USART1 (PA9/PA10 @ 115200 8N1).
 *
 * Author: Antigravity / Mark Angyal
 */

#include "stm32f746xx.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BOOTLOADER_VERSION      "1.0"
#define APP_START_ADDR          0x08008000U     /* Sector 1 Start */
#define APP_MAX_SIZE            (224U * 1024U)  /* Sectors 1-4 (224 KB) */
#define RAM_START               0x20000000U
#define RAM_END                 0x20050000U     /* 320 KB SRAM */
#define HSI_CLOCK_HZ            16000000U
#define UART_BAUDRATE           115200U
#define MAGIC_ENTER_BOOTLOADER  0x55AA55AAU

/* =========================================================================
 * UART Low-Level Functions (USART1 on PA9/PA10)
 * ========================================================================= */
static void uart_init(void) {
    /* 1. Enable GPIOA and USART1 clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* 2. Configure PA9 (TX) and PA10 (RX) as AF7 */
    /* Mode: Alternate (10b) */
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->MODER |=  ((2U << (9 * 2)) | (2U << (10 * 2)));

    /* Speed: Very High (11b) */
    GPIOA->OSPEEDR |= ((3U << (9 * 2)) | (3U << (10 * 2)));

    /* Pull-up for RX (01b) */
    GPIOA->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2)));
    GPIOA->PUPDR |=  (1U << (10 * 2));

    /* AF7 (USART1) on PA9 and PA10 */
    GPIOA->AFR[1] &= ~((0xFU << ((9 - 8) * 4)) | (0xFU << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((7U   << ((9 - 8) * 4)) | (7U   << ((10 - 8) * 4)));

    /* 3. Configure USART1: 115200 8N1 @ 16 MHz HSI */
    USART1->CR1 = 0; /* Disable */
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    /* BRR = 16000000 / 115200 = 138.88 -> 139 = 0x8B */
    USART1->BRR = (HSI_CLOCK_HZ + (UART_BAUDRATE / 2U)) / UART_BAUDRATE;

    /* Enable Transmitter and Receiver */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart_send_char(char c) {
    while (!(USART1->ISR & USART_ISR_TXE));
    USART1->TDR = (uint8_t)c;
}

static void uart_send_str(const char *str) {
    while (*str) {
        uart_send_char(*str++);
    }
}

static bool uart_has_char(void) {
    return (USART1->ISR & USART_ISR_RXNE) != 0;
}

static char uart_read_char(void) {
    while (!(USART1->ISR & USART_ISR_RXNE));
    return (char)(USART1->RDR & 0xFF);
}

/* =========================================================================
 * Delay using SysTick
 * ========================================================================= */
static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        /* ~16000 cycles for 1 ms at 16 MHz */
        for (volatile uint32_t j = 0; j < 1600; j++) {
            __NOP();
        }
    }
}

/* =========================================================================
 * Flash Operations (Unlock, Erase Sectors 1-4, Program Words, Lock)
 * ========================================================================= */
static void flash_unlock(void) {
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
}

static void flash_lock(void) {
    FLASH->CR |= FLASH_CR_LOCK;
}

static void flash_wait_busy(void) {
    while (FLASH->SR & FLASH_SR_BSY);
}

static bool flash_erase_app_sectors(uint32_t size_bytes) {
    flash_unlock();
    flash_wait_busy();

    /* Determine which sectors to erase (Sectors 1..4: S1=32K, S2=32K, S3=32K, S4=128K -> 224 KB) */
    uint32_t sectors_to_erase = 1; /* At least Sector 1 */
    if (size_bytes > 32U * 1024U) sectors_to_erase = 2;
    if (size_bytes > 64U * 1024U) sectors_to_erase = 3;
    if (size_bytes > 96U * 1024U) sectors_to_erase = 4;
    if (sectors_to_erase > 4) sectors_to_erase = 4;

    for (uint32_t sec = 1; sec <= sectors_to_erase; sec++) {
        FLASH->CR &= ~FLASH_CR_PSIZE;
        FLASH->CR |= FLASH_CR_PSIZE_1; /* 32-bit parallelism (2.7V - 3.6V) */

        FLASH->CR &= ~FLASH_CR_SNB;
        FLASH->CR |= ((sec & 0x1FU) << FLASH_CR_SNB_Pos) | FLASH_CR_SER;

        FLASH->CR |= FLASH_CR_STRT;
        flash_wait_busy();

        FLASH->CR &= ~FLASH_CR_SER;

        if (FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_ERSERR)) {
            FLASH->SR |= (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_ERSERR);
            flash_lock();
            return false;
        }
    }

    flash_lock();
    return true;
}

static bool flash_write_words(uint32_t addr, const uint32_t *data, uint32_t num_words) {
    flash_unlock();
    flash_wait_busy();

    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1; /* 32-bit */

    for (uint32_t i = 0; i < num_words; i++) {
        FLASH->CR |= FLASH_CR_PG;
        *(volatile uint32_t*)(addr + (i * 4U)) = data[i];
        flash_wait_busy();
        FLASH->CR &= ~FLASH_CR_PG;

        if (FLASH->SR & (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR)) {
            FLASH->SR |= (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR);
            flash_lock();
            return false;
        }

        /* Read-back verify */
        if (*(volatile uint32_t*)(addr + (i * 4U)) != data[i]) {
            flash_lock();
            return false;
        }
    }

    flash_lock();
    return true;
}

/* =========================================================================
 * CRC32 Calculation (IEEE 802.3 Standard-Polynom 0xEDB88320)
 * ========================================================================= */
static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

static uint16_t calculate_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ 0xA001U;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* =========================================================================
 * Application Validity & Jump
 * ========================================================================= */
static bool is_app_valid(void) {
    uint32_t sp = *(volatile uint32_t*)APP_START_ADDR;
    uint32_t reset_vec = *(volatile uint32_t*)(APP_START_ADDR + 4U);

    /* Stack pointer must point to internal SRAM */
    if (sp < RAM_START || sp > RAM_END) {
        return false;
    }

    /* Reset vector must point to application flash (Thumb mode: odd address) */
    if (reset_vec < (APP_START_ADDR + 1U) || reset_vec > (APP_START_ADDR + APP_MAX_SIZE) || !(reset_vec & 1U)) {
        return false;
    }

    return true;
}

static void jump_to_application(void) {
    uint32_t app_sp = *(volatile uint32_t*)APP_START_ADDR;
    uint32_t app_reset = *(volatile uint32_t*)(APP_START_ADDR + 4U);

    /* Disable interrupts and reset peripherals */
    __disable_irq();

    /* Disable USART1 */
    USART1->CR1 = 0;
    RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;

    /* Set VTOR to application vector table */
    SCB->VTOR = APP_START_ADDR;

    /* Set main stack pointer */
    __set_MSP(app_sp);

    /* Jump to application reset handler */
    void (*app_entry)(void) = (void (*)(void))app_reset;
    app_entry();
}

/* =========================================================================
 * Protocol Parser & Line Processing
 * ========================================================================= */
static uint8_t hex_to_byte(char hi, char lo) {
    uint8_t h = (hi >= '0' && hi <= '9') ? (hi - '0') : ((hi >= 'A' && hi <= 'F') ? (hi - 'A' + 10) : (hi - 'a' + 10));
    uint8_t l = (lo >= '0' && lo <= '9') ? (lo - '0') : ((lo >= 'A' && lo <= 'F') ? (lo - 'A' + 10) : (lo - 'a' + 10));
    return (uint8_t)((h << 4) | l);
}

static void process_command(char *cmd) {
    /* 1. PING */
    if (strcmp(cmd, "CMD:BOOT_PING") == 0) {
        uart_send_str("BOOTLOADER:PONG\r\n");
        return;
    }

    /* 2. ERASE: size=<bytes> */
    if (strncmp(cmd, "CMD:BOOT_ERASE:size=", 20) == 0) {
        uint32_t size = 0;
        sscanf(cmd + 20, "%lu", &size);
        if (size == 0 || size > APP_MAX_SIZE) {
            uart_send_str("BOOTLOADER:ERASE_ERR:INVALID_SIZE\r\n");
            return;
        }

        if (flash_erase_app_sectors(size)) {
            uart_send_str("BOOTLOADER:ERASE_OK\r\n");
        } else {
            uart_send_str("BOOTLOADER:ERASE_ERR:FLASH_FAULT\r\n");
        }
        return;
    }

    /* 3. CHUNK: <seq>:<offset>:<len>:<hex_data>:<crc16> */
    if (strncmp(cmd, "CMD:BOOT_CHUNK:", 15) == 0) {
        char *p = cmd + 15;
        uint32_t seq = 0, offset = 0, len = 0;
        uint32_t crc16_in = 0;

        char *token = strtok(p, ":");
        if (token) seq = (uint32_t)strtoul(token, NULL, 10);

        token = strtok(NULL, ":");
        if (token) offset = (uint32_t)strtoul(token, NULL, 10);

        token = strtok(NULL, ":");
        if (token) len = (uint32_t)strtoul(token, NULL, 10);

        char *hex_data = strtok(NULL, ":");
        char *crc_str = strtok(NULL, ":");
        if (crc_str) sscanf(crc_str, "%lx", &crc16_in);

        if (!hex_data || len == 0 || len > 1024 || (offset + len) > APP_MAX_SIZE || (offset % 4U) != 0) {
            char resp[48];
            snprintf(resp, sizeof(resp), "BOOTLOADER:NACK:%lu:PARAM_ERR\r\n", seq);
            uart_send_str(resp);
            return;
        }

        /* Decode hex data */
        static uint8_t s_chunk_bytes[1024];
        for (uint32_t i = 0; i < len; i++) {
            s_chunk_bytes[i] = hex_to_byte(hex_data[i * 2], hex_data[i * 2 + 1]);
        }

        /* Check CRC16 */
        uint16_t calc_crc = calculate_crc16(s_chunk_bytes, len);
        if (calc_crc != (uint16_t)crc16_in) {
            char resp[48];
            snprintf(resp, sizeof(resp), "BOOTLOADER:NACK:%lu:CRC_ERR\r\n", seq);
            uart_send_str(resp);
            return;
        }

        /* Pad to 32-bit word if needed */
        uint32_t num_words = (len + 3U) / 4U;
        uint32_t target_addr = APP_START_ADDR + offset;

        if (flash_write_words(target_addr, (const uint32_t*)s_chunk_bytes, num_words)) {
            char resp[48];
            snprintf(resp, sizeof(resp), "BOOTLOADER:ACK:%lu\r\n", seq);
            uart_send_str(resp);
        } else {
            char resp[48];
            snprintf(resp, sizeof(resp), "BOOTLOADER:NACK:%lu:WRITE_ERR\r\n", seq);
            uart_send_str(resp);
        }
        return;
    }

    /* 4. VERIFY: size=<bytes>:crc32=<hex> */
    if (strncmp(cmd, "CMD:BOOT_VERIFY:", 16) == 0) {
        char *p = cmd + 16;
        uint32_t size = 0;
        uint32_t expected_crc = 0;

        char *token = strtok(p, ":");
        if (token && strncmp(token, "size=", 5) == 0) size = (uint32_t)strtoul(token + 5, NULL, 10);

        token = strtok(NULL, ":");
        if (token && strncmp(token, "crc32=", 6) == 0) expected_crc = (uint32_t)strtoul(token + 6, NULL, 16);

        if (size == 0 || size > APP_MAX_SIZE) {
            uart_send_str("BOOTLOADER:VERIFY_ERR:INVALID_SIZE\r\n");
            return;
        }

        uint32_t actual_crc = calculate_crc32((const uint8_t*)APP_START_ADDR, size);
        if (actual_crc == expected_crc) {
            uart_send_str("BOOTLOADER:VERIFY_OK\r\n");
        } else {
            char resp[64];
            snprintf(resp, sizeof(resp), "BOOTLOADER:VERIFY_ERR:CALC=%08lX\r\n", actual_crc);
            uart_send_str(resp);
        }
        return;
    }

    /* 5. RUN: Jump to Application */
    if (strcmp(cmd, "CMD:BOOT_RUN") == 0) {
        if (!is_app_valid()) {
            uart_send_str("BOOTLOADER:RUN_ERR:APP_INVALID\r\n");
            return;
        }
        uart_send_str("BOOTLOADER:RUNNING\r\n");
        delay_ms(20);
        jump_to_application();
        return;
    }

    /* Unknown */
    uart_send_str("BOOTLOADER:UNKNOWN_CMD\r\n");
}

/* =========================================================================
 * Main Entry Point
 * ========================================================================= */
int main(void) {
    uart_init();

    /* Enable Power interface clock and check RTC Backup Register */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR1 |= PWR_CR1_DBP;

    bool force_bootloader = (RTC->BKP0R == MAGIC_ENTER_BOOTLOADER);
    if (force_bootloader) {
        RTC->BKP0R = 0; /* Clear flag */
    }

    bool app_valid = is_app_valid();

    /* If no update was requested and application is valid:
     * Wait 150 ms for an interactive connection, otherwise jump straight to app */
    if (!force_bootloader && app_valid) {
        for (int i = 0; i < 15; i++) {
            delay_ms(10);
            if (uart_has_char()) {
                char c = uart_read_char();
                if (c == '?' || c == 'C') {
                    force_bootloader = true;
                    break;
                }
            }
        }

        if (!force_bootloader) {
            jump_to_application();
        }
    }

    /* Bootloader stays active: announce readiness */
    uart_send_str("BOOTLOADER:READY:v" BOOTLOADER_VERSION "\r\n");

    /* Line receiving buffer */
    static char s_line_buf[2200];
    uint32_t buf_idx = 0;

    while (1) {
        if (uart_has_char()) {
            char c = uart_read_char();
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                s_line_buf[buf_idx] = '\0';
                if (buf_idx > 0) {
                    process_command(s_line_buf);
                }
                buf_idx = 0;
            } else {
                if (buf_idx < (sizeof(s_line_buf) - 1U)) {
                    s_line_buf[buf_idx++] = c;
                } else {
                    buf_idx = 0; /* Overflow reset */
                }
            }
        }
    }

    return 0;
}
