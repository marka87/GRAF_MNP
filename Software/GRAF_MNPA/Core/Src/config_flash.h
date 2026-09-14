#ifndef CONFIG_FLASH_H
#define CONFIG_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CONFIG_FLASH_MAGIC          0x47524146U /* "GRAF" */
#define CONFIG_FLASH_VERSION        1U
#define CONFIG_FLASH_SLOT_SIZE      256U
#define CONFIG_FLASH_SECTOR_NUM     7U          /* FLASH_SECTOR_7 */
#define CONFIG_FLASH_SECTOR_ADDR    0x080C0000U /* Start von Sektor 7 (256 KB) */
#define CONFIG_FLASH_SECTOR_SIZE    (256U * 1024U)
#define CONFIG_FLASH_MAX_SLOTS      (CONFIG_FLASH_SECTOR_SIZE / CONFIG_FLASH_SLOT_SIZE) /* 1024 Slots */

typedef struct __attribute__((packed)) {
    uint32_t magic;                 /* 0x47524146 ("GRAF") */
    uint16_t version;               /* CONFIG_FLASH_VERSION (1) */
    uint16_t struct_size;           /* sizeof(CarriageConfig_t) = 256 */
    uint32_t write_count;           /* Monoton steigender Zähler */
    char     carriage_id[16];       /* z. B. "SCHLITTEN-A", "SCHLITTEN-B", "DEFAULT" */

    /* Regler-Parameter Z-Achse */
    float    pos_kp;                /* Position Kp (Default: 0.12) */
    float    pos_ki;                /* Position Ki (Default: 0.0) */
    float    pos_kd;                /* Position Kd (Default: 0.0) */
    float    vel_kp;                /* Velocity Kp (Default: 0.025) */
    float    vel_ki;                /* Velocity Ki (Default: 0.00025) */
    float    vel_kd;                /* Velocity Kd (Default: 0.0) */

    /* Kinematik & Landebahn */
    uint16_t landing_zone_margin;   /* Inkremente vor Kontakt (Default: 150, für A z. B. 60) */
    uint8_t  landing_speed_level;   /* Speed-Level auf Landebahn (Default: 3, für A z. B. 4) */
    uint8_t  braking_dist_factor;   /* Faktor für Bremsweg-Berechnung (Default: 80) */

    /* Sensorik & Offsets */
    int32_t  z_ax_no_pos;           /* Gelernte NO-Sensor Position (Default: 3204) */
    uint16_t ds_trigger_delta_mv;   /* Trigger-Delta Drucksensor in mV (Default: 1500) */
    float    d_mot_voltage;         /* Druckmotor-Spannung in V (Default: 3.30) */

    /* Auffüllung auf 252 Bytes (vor 4 Bytes CRC) */
    uint8_t  reserved[252 - 66];

    uint32_t crc32;                 /* CRC-32 Prüfsumme über Bytes 0 bis 251 */
} CarriageConfig_t;

/* Initialisiert das Flash-Konfigurationsmodul und lädt den neuesten Stand */
void              ConfigFlash_Init(void);

/* Gibt Zeiger auf die aktive Arbeitskonfiguration (im RAM) zurück */
CarriageConfig_t* ConfigFlash_GetActive(void);

/* Befüllt die Zielstruktur mit den werkseitigen Standardwerten */
void              ConfigFlash_LoadDefaults(CarriageConfig_t *out);

/* Schreibt die Konfiguration in den nächsten freien Slot in Sektor 7 */
bool              ConfigFlash_Save(const CarriageConfig_t *cfg);

/* Wendet die aktuelle Konfiguration auf PID-Regler und Kinematik an */
void              ConfigFlash_ApplyActive(void);

/* Berechnet IEEE 802.3 CRC-32 */
uint32_t          ConfigFlash_CalculateCRC32(const uint8_t *data, size_t length);

#endif /* CONFIG_FLASH_H */
