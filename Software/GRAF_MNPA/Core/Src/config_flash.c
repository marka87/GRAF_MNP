/*
 * config_flash.c
 *
 * Schlittenspezifischer Parameterspeicher in Flash Sektor 7 (EEPROM-Emulation)
 * Rolling-Slot Architektur mit CRC32-Integritaetspruefung
 *
 * Created: 2026
 * Author:  Antigravity / Mark Angyal
 */

#include "config_flash.h"
#include "Z_PID_Control.h"
#include "test_run.h"
#include "stm32f7xx_hal.h"
#include <string.h>
#include <stdio.h>

/* Compile-Time Check: Struktur muss exakt 256 Bytes gross sein */
_Static_assert(sizeof(CarriageConfig_t) == CONFIG_FLASH_SLOT_SIZE, "CarriageConfig_t must be exactly 256 bytes");

/* Aktive Arbeitskonfiguration im RAM */
static CarriageConfig_t s_active_config;
static uint32_t         s_next_free_slot = 0;
static bool             s_is_initialized = false;

/* =========================================================================
 * CRC-32 (IEEE 802.3 Standard-Polynom 0xEDB88320)
 * ========================================================================= */
uint32_t ConfigFlash_CalculateCRC32(const uint8_t *data, size_t length)
{
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

/* =========================================================================
 * Standardwerte laden (Werkseinstellungen)
 * ========================================================================= */
void ConfigFlash_LoadDefaults(CarriageConfig_t *out)
{
    if (out == NULL) return;

    memset(out, 0, sizeof(CarriageConfig_t));
    out->magic       = CONFIG_FLASH_MAGIC;
    out->version     = CONFIG_FLASH_VERSION;
    out->struct_size = (uint16_t)sizeof(CarriageConfig_t);
    out->write_count = 0;
    strncpy(out->carriage_id, "DEFAULT", sizeof(out->carriage_id) - 1);

    /* PID-Parameter: P-Lage + PI-Geschwindigkeit (Mimot-Referenz) */
    out->pos_kp = 0.12f;
    out->pos_ki = 0.0f;
    out->pos_kd = 0.0f;
    out->vel_kp = 0.025f;
    out->vel_ki = 0.00025f;
    out->vel_kd = 0.0f;

    /* Kinematik & Landebahn */
    out->landing_zone_margin = 150u;  /* 150 Inc (1.5 mm) vor Kontakt Kriechfahrt */
    out->landing_speed_level = 3u;    /* Stufe 3 (4 Inc/ms) */
    out->braking_dist_factor = 80u;   /* Bremsweg = Stufe * 80 Inc */

    /* Sensorik */
    out->z_ax_no_pos         = 3204;
    out->ds_trigger_delta_mv = 1500u; /* TTL 1.5 V ueber Standby */
    out->d_mot_voltage       = 3.30f; /* 3.3 V Druckmotor */

    out->crc32 = ConfigFlash_CalculateCRC32((const uint8_t *)out, offsetof(CarriageConfig_t, crc32));
}

/* =========================================================================
 * Initialisierung & Suche des neuesten gueltigen Slots
 * ========================================================================= */
void ConfigFlash_Init(void)
{
    int32_t  best_slot_index = -1;
    uint32_t max_write_count = 0;
    s_next_free_slot         = CONFIG_FLASH_MAX_SLOTS;

    for (uint32_t i = 0; i < CONFIG_FLASH_MAX_SLOTS; i++) {
        const CarriageConfig_t *slot = (const CarriageConfig_t *)(CONFIG_FLASH_SECTOR_ADDR + (i * CONFIG_FLASH_SLOT_SIZE));

        /* Leerer Slot in geloeschtem Flash (alle Bits 1) */
        if (slot->magic == 0xFFFFFFFFU) {
            if (s_next_free_slot == CONFIG_FLASH_MAX_SLOTS) {
                s_next_free_slot = i;
            }
            continue;
        }

        /* Gueltige Magic & Version pruefen */
        if (slot->magic == CONFIG_FLASH_MAGIC &&
            slot->version == CONFIG_FLASH_VERSION &&
            slot->struct_size == sizeof(CarriageConfig_t)) {

            uint32_t calc_crc = ConfigFlash_CalculateCRC32((const uint8_t *)slot, offsetof(CarriageConfig_t, crc32));
            if (calc_crc == slot->crc32) {
                if (best_slot_index < 0 || slot->write_count > max_write_count) {
                    best_slot_index = (int32_t)i;
                    max_write_count = slot->write_count;
                }
            }
        }
    }

    if (best_slot_index >= 0) {
        /* Neuesten gueltigen Datensatz aus dem Flash ins RAM uebernehmen */
        memcpy(&s_active_config, (const void *)(CONFIG_FLASH_SECTOR_ADDR + (best_slot_index * CONFIG_FLASH_SLOT_SIZE)), sizeof(CarriageConfig_t));
    } else {
        /* Kein gueltiger Datensatz vorhanden -> Werkseinstellungen */
        ConfigFlash_LoadDefaults(&s_active_config);
        if (s_next_free_slot == CONFIG_FLASH_MAX_SLOTS) {
            s_next_free_slot = 0;
        }
    }

    s_is_initialized = true;
}

CarriageConfig_t* ConfigFlash_GetActive(void)
{
    if (!s_is_initialized) {
        ConfigFlash_Init();
    }
    return &s_active_config;
}

/* =========================================================================
 * Konfiguration in Flash speichern (Rolling Slot mit Sektor-Erase bei Voll)
 * ========================================================================= */
bool ConfigFlash_Save(const CarriageConfig_t *cfg)
{
    if (!s_is_initialized) {
        ConfigFlash_Init();
    }

    if (cfg != NULL && cfg != &s_active_config) {
        memcpy(&s_active_config, cfg, sizeof(CarriageConfig_t));
    }

    s_active_config.magic       = CONFIG_FLASH_MAGIC;
    s_active_config.version     = CONFIG_FLASH_VERSION;
    s_active_config.struct_size = (uint16_t)sizeof(CarriageConfig_t);
    s_active_config.write_count++;
    s_active_config.crc32       = ConfigFlash_CalculateCRC32((const uint8_t *)&s_active_config, offsetof(CarriageConfig_t, crc32));

    HAL_FLASH_Unlock();

    /* Wenn alle 1024 Slots voll sind: Sektor 7 loeschen (ca. 1.1s) */
    if (s_next_free_slot >= CONFIG_FLASH_MAX_SLOTS) {
        FLASH_EraseInitTypeDef erase_init;
        erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
        erase_init.Sector       = FLASH_SECTOR_7;
        erase_init.NbSectors    = 1;
        erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        uint32_t sector_error = 0;
        HAL_StatusTypeDef erase_status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
        if (erase_status != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
        s_next_free_slot = 0;
    }

    /* Ziel-Slot im Flash programmieren (64 Worte a 32 Bit) */
    uint32_t target_addr = CONFIG_FLASH_SECTOR_ADDR + (s_next_free_slot * CONFIG_FLASH_SLOT_SIZE);
    const uint32_t *src_words = (const uint32_t *)&s_active_config;

    for (uint32_t w = 0; w < (CONFIG_FLASH_SLOT_SIZE / 4U); w++) {
        HAL_StatusTypeDef prog_status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, target_addr + (w * 4U), (uint64_t)src_words[w]);
        if (prog_status != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();

    /* D-Cache fuer den beschriebenen Bereich invalidieren */
    SCB_InvalidateDCache_by_Addr((uint32_t *)target_addr, CONFIG_FLASH_SLOT_SIZE);

    s_next_free_slot++;

    /* Neue Werte direkt auf laufende Regler anwenden */
    ConfigFlash_ApplyActive();

    return true;
}

/* =========================================================================
 * Auf Regler und Kinematik anwenden
 * ========================================================================= */
void ConfigFlash_ApplyActive(void)
{
    if (!s_is_initialized) {
        ConfigFlash_Init();
    }

    CarriageConfig_t *cfg = &s_active_config;

    /* PID Z-Achse setzen */
    Z_PID_SetPositionParameters(cfg->pos_kp, cfg->pos_ki, cfg->pos_kd);
    Z_PID_SetVelocityParameters(cfg->vel_kp, cfg->vel_ki, cfg->vel_kd);

    /* Test B Parameter */
    TestRun_SetTriggerDeltaMv(cfg->ds_trigger_delta_mv);
    TestRun_SetDruckmotorVoltage(cfg->d_mot_voltage);
}

/* =========================================================================
 * Profil anhand der ID laden (Flash oder Preset)
 * ========================================================================= */
bool ConfigFlash_LoadById(const char *id)
{
    if (!s_is_initialized) {
        ConfigFlash_Init();
    }
    if (id == NULL || id[0] == '\0') {
        return false;
    }

    int32_t  best_slot_index = -1;
    uint32_t max_write_count = 0;

    /* 1. Neuesten passenden Slot in Flash Sektor 7 suchen */
    for (uint32_t i = 0; i < CONFIG_FLASH_MAX_SLOTS; i++) {
        const CarriageConfig_t *slot = (const CarriageConfig_t *)(CONFIG_FLASH_SECTOR_ADDR + (i * CONFIG_FLASH_SLOT_SIZE));
        if (slot->magic == CONFIG_FLASH_MAGIC &&
            slot->version == CONFIG_FLASH_VERSION &&
            slot->struct_size == sizeof(CarriageConfig_t)) {
            uint32_t calc_crc = ConfigFlash_CalculateCRC32((const uint8_t *)slot, offsetof(CarriageConfig_t, crc32));
            if (calc_crc == slot->crc32 && strncmp(slot->carriage_id, id, sizeof(slot->carriage_id)) == 0) {
                if (best_slot_index < 0 || slot->write_count > max_write_count) {
                    best_slot_index = (int32_t)i;
                    max_write_count = slot->write_count;
                }
            }
        }
    }

    if (best_slot_index >= 0) {
        memcpy(&s_active_config, (const void *)(CONFIG_FLASH_SECTOR_ADDR + (best_slot_index * CONFIG_FLASH_SLOT_SIZE)), sizeof(CarriageConfig_t));
        ConfigFlash_ApplyActive();
        return true;
    }

    /* 2. Nicht im Flash vorhanden -> Vordefinierte Standard-Profile */
    if (strcmp(id, "SCHLITTEN-A") == 0) {
        ConfigFlash_LoadDefaults(&s_active_config);
        strncpy(s_active_config.carriage_id, "SCHLITTEN-A", sizeof(s_active_config.carriage_id) - 1);
        s_active_config.landing_zone_margin = 100u;
        s_active_config.landing_speed_level = 4u;
        s_active_config.braking_dist_factor = 120u;
        ConfigFlash_ApplyActive();
        return true;
    }
    if (strcmp(id, "SCHLITTEN-B") == 0) {
        ConfigFlash_LoadDefaults(&s_active_config);
        strncpy(s_active_config.carriage_id, "SCHLITTEN-B", sizeof(s_active_config.carriage_id) - 1);
        s_active_config.landing_zone_margin = 150u;
        s_active_config.landing_speed_level = 3u;
        s_active_config.braking_dist_factor = 120u;
        ConfigFlash_ApplyActive();
        return true;
    }
    if (strcmp(id, "DEFAULT") == 0) {
        ConfigFlash_LoadDefaults(&s_active_config);
        ConfigFlash_ApplyActive();
        return true;
    }

    return false;
}

/* =========================================================================
 * Liste aller im Flash vorhandenen Profil-IDs ermitteln
 * ========================================================================= */
size_t ConfigFlash_GetProfileList(char list[][16], size_t max_count)
{
    if (list == NULL || max_count == 0) return 0;
    if (!s_is_initialized) ConfigFlash_Init();

    size_t count = 0;
    strncpy(list[count++], "DEFAULT", 15);
    if (count < max_count) strncpy(list[count++], "SCHLITTEN-A", 15);
    if (count < max_count) strncpy(list[count++], "SCHLITTEN-B", 15);

    for (uint32_t i = 0; i < CONFIG_FLASH_MAX_SLOTS && count < max_count; i++) {
        const CarriageConfig_t *slot = (const CarriageConfig_t *)(CONFIG_FLASH_SECTOR_ADDR + (i * CONFIG_FLASH_SLOT_SIZE));
        if (slot->magic == CONFIG_FLASH_MAGIC &&
            slot->version == CONFIG_FLASH_VERSION &&
            slot->struct_size == sizeof(CarriageConfig_t)) {
            uint32_t calc_crc = ConfigFlash_CalculateCRC32((const uint8_t *)slot, offsetof(CarriageConfig_t, crc32));
            if (calc_crc == slot->crc32 && slot->carriage_id[0] != '\0') {
                bool exists = false;
                for (size_t k = 0; k < count; k++) {
                    if (strncmp(list[k], slot->carriage_id, 15) == 0) {
                        exists = true;
                        break;
                    }
                }
                if (!exists && count < max_count) {
                    strncpy(list[count], slot->carriage_id, 15);
                    list[count][15] = '\0';
                    count++;
                }
            }
        }
    }
    return count;
}

