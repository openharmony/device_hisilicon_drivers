/*
 * Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _MMC_CAPS_H
#define _MMC_CAPS_H

#include <sys/types.h>

enum signal_volt {
    SIGNAL_VOLT_3V3 = 0,
    SIGNAL_VOLT_1V8,
    SIGNAL_VOLT_1V2
};

enum mmc_bus_timing {
    TIMING_MMC_DS = 0,
    TIMING_MMC_HS,
    TIMING_SD_HS,
    TIMING_UHS_SDR12,
    TIMING_UHS_SDR25,
    TIMING_UHS_SDR50,
    TIMING_UHS_SDR104,
    TIMING_UHS_DDR50,
    TIMING_UHS_DDR52,
    TIMING_MMC_HS200,  /* for emmc */
    TIMING_MMC_HS400,  /* for emmc */
};

union mmc_caps {
    uint32_t caps_data;
    struct caps_bits_data {
        uint32_t cap_4_bit : 1;             /* bit:0 support 4 bit transfer */
        uint32_t cap_8_bit : 1;             /* bit:1 support 8 bit transfer */
        uint32_t cap_mmc_highspeed : 1;     /* bit:2 support mmc high-speed timing */
        uint32_t cap_sd_highspeed : 1;      /* bit:3 support SD high-speed timing */
        uint32_t cap_sdio_irq : 1;          /* bit:4 signal pending SDIO irqs */
        uint32_t cap_only_spi : 1;          /* bit:5 only support spi protocols */
        uint32_t cap_need_poll : 1;         /* bit:6 need polling for card-detection */
        uint32_t cap_nonremovable : 1;      /* bit:7 Nonremoveable eg. eMMC */
        uint32_t cap_wait_while_busy : 1;   /* bit:8 waits while card is busy */
        uint32_t cap_erase : 1;             /* bit:9 allow erase */
        uint32_t cap_1v8_ddr : 1;           /* bit:10 support ddr mode at 1.8V */
        uint32_t cap_1v2_ddr : 1;           /* bit:11 support ddr mode at 1.2V */
        uint32_t cap_power_off_card : 1;    /* bit:12 support power off after boot */
        uint32_t cap_bus_width_test : 1;    /* bit:13 CMD14/CMD19 bus width ok */
        uint32_t cap_UHS_SDR12 : 1;         /* bit:14 support UHS SDR12 mode */
        uint32_t cap_UHS_SDR25 : 1;         /* bit:15 support UHS SDR25 mode */
        uint32_t cap_UHS_SDR50 : 1;         /* bit:16 support UHS SDR50 mode */
        uint32_t cap_UHS_SDR104 : 1;        /* bit:17 support UHS SDR104 mode */
        uint32_t cap_UHS_DDR50 : 1;         /* bit:18 support UHS DDR50 mode */
        uint32_t cap_XPC_330 : 1;           /* bit:19 support >150mA current at 3.3V */
        uint32_t cap_XPC_300 : 1;           /* bit:20 support >150mA current at 3.0V */
        uint32_t cap_XPC_180 : 1;           /* bit:21 support >150mA current at 1.8V */
        uint32_t cap_driver_type_A : 1;     /* bit:22 support driver type A */
        uint32_t cap_driver_type_C : 1;     /* bit:23 support driver type C */
        uint32_t cap_driver_type_D : 1;     /* bit:24 support driver type D */
        uint32_t cap_max_current_200 : 1;   /* bit:25 max current limit 200mA */
        uint32_t cap_max_current_400 : 1;   /* bit:26 max current limit 400mA */
        uint32_t cap_max_current_600 : 1;   /* bit:27 max current limit 600mA */
        uint32_t cap_max_current_800 : 1;   /* bit:28 max current limit 800mA */
        uint32_t cap_CMD23 : 1;             /* bit:29 support CMD23 */
        uint32_t cap_hw_reset : 1;          /* bit:30 support hardware reset */
        uint32_t reserved : 1;               /* bit:31 reserverd */
    } bits;
};

union mmc_caps2 {
    uint32_t caps2_data;
    struct caps2_bits_data {
        uint32_t caps2_bootpart_noacc : 1;    /* bit:0 boot partition no access */
        uint32_t caps2_cache_ctrl : 1;        /* bit:1 allow cache control */
        uint32_t caps2_poweroff_notify : 1;   /* bit:2 support notify power off */
        uint32_t caps2_no_multi_read : 1;     /* bit:3 not support multiblock read */
        uint32_t caps2_no_sleep_cmd : 1;      /* bit:4 not support sleep command */
        uint32_t caps2_HS200_1v8_SDR : 1;     /* bit:5 support */
        uint32_t caps2_HS200_1v2_SDR : 1;     /* bit:6 support */
        uint32_t caps2_broken_voltage : 1;    /* bit:7 use broken voltage */
        uint32_t caps2_detect_no_err : 1;     /* bit:8 I/O err check card removal */
        uint32_t caps2_HC_erase_size : 1;     /* bit:9 High-capacity erase size */
        uint32_t caps2_speedup_enable : 1;
        uint32_t caps2_HS400_1v8 : 1;
        uint32_t caps2_HS400_1v2 : 1;
        uint32_t caps2_HS400_enhanced_strobe : 1;
        uint32_t reserved : 18;               /* bits:13~31, reserved */
    } bits;
};

/* ocr register describe */
union mmc_ocr {
    uint32_t    ocr_data;
    struct ocr_bits_data {
        uint32_t reserved0 : 4;
        uint32_t reserved1 : 1;
        uint32_t reserved2 : 1;
        uint32_t reserved3 : 1;
        uint32_t vdd_1v65_1v95 : 1;     /* bit:7  voltage 1.65 ~ 1.95 */
        uint32_t vdd_2v0_2v1 : 1;       /* bit:8  voltage 2.0 ~ 2.1 */
        uint32_t vdd_2v1_2v2 : 1;       /* bit:9  voltage 2.1 ~ 2.2 */
        uint32_t vdd_2v2_2v3 : 1;       /* bit:10  voltage 2.2 ~ 2.3 */
        uint32_t vdd_2v3_2v4 : 1;       /* bit:11  voltage 2.3 ~ 2.4 */
        uint32_t vdd_2v4_2v5 : 1;       /* bit:12  voltage 2.4 ~ 2.5 */
        uint32_t vdd_2v5_2v6 : 1;       /* bit:13  voltage 2.5 ~ 2.6 */
        uint32_t vdd_2v6_2v7 : 1;       /* bit:14  voltage 2.6 ~ 2.7 */
        uint32_t vdd_2v7_2v8 : 1;       /* bit:15  voltage 2.7 ~ 2.8 */
        uint32_t vdd_2v8_2v9 : 1;       /* bit:16  voltage 2.8 ~ 2.9 */
        uint32_t vdd_2v9_3v0 : 1;       /* bit:17  voltage 2.9 ~ 3.0 */
        uint32_t vdd_3v0_3v1 : 1;       /* bit:18  voltage 3.0 ~ 3.1 */
        uint32_t vdd_3v1_3v2 : 1;       /* bit:19  voltage 3.1 ~ 3.2 */
        uint32_t vdd_3v2_3v3 : 1;       /* bit:20  voltage 3.2 ~ 3.3 */
        uint32_t vdd_3v3_3v4 : 1;       /* bit:21  voltage 3.3 ~ 3.4 */
        uint32_t vdd_3v4_3v5 : 1;       /* bit:22  voltage 3.4 ~ 3.5 */
        uint32_t vdd_3v5_3v6 : 1;       /* bit:23  voltage 3.5 ~ 3.6 */
        uint32_t S18 : 1;               /* bit:24  switch to 1.8v accepted */
        uint32_t reserved4 : 2;
        uint32_t SDIO_MEM_PRE : 1;      /* bit:27 sdio memory present */
        uint32_t SD_XPC : 1;            /* bit:28 XPC for ACMD41 */
        uint32_t SD_UHSII : 1;          /* bit:29 UHSII for resp of ACMD41 */
        uint32_t HCS : 1;               /* bit:30 support high capacity  */
        uint32_t busy : 1;              /* bit:31 This bit is set to LOW if the card
                                           has not finished the power up routine */
    } bits;
};

union host_quirks {
    unsigned int quirks_data;
    struct quirks_bit_data {
        unsigned int quirk_clock_before_reset : 1;
        unsigned int quirk_force_dma : 1;
        unsigned int quirk_no_card_no_reset : 1;
        unsigned int quirk_single_power_write : 1;
        unsigned int quirk_reset_cmd_data_on_ios : 1;
        unsigned int quirk_broken_dma : 1;
        unsigned int quirk_broken_adma : 1;
        unsigned int quirk_32bit_dma_addr : 1;
        unsigned int quirk_32bit_dma_size : 1;
        unsigned int quirk_32bit_adma_size : 1;
        unsigned int quirk_reset_after_request : 1;
        unsigned int quirk_no_simult_vdd_and_power : 1;
        unsigned int quirk_broken_timeout_val : 1;
        unsigned int quirk_broken_small_pio : 1;
        unsigned int quirk_no_busy_irq : 1;
        unsigned int quirk_broken_card_detection : 1;
        unsigned int quirk_inverted_write_protect : 1;
        unsigned int quirk_pio_needs_delay : 1;
        unsigned int quirk_force_blk_sz_2048 : 1;
        unsigned int quirk_no_multiblock : 1;
        unsigned int quirk_force_1_bit_data : 1;
        unsigned int quirk_delay_after_power : 1;
        unsigned int quirk_data_timeout_use_sdclk : 1;
        unsigned int quirk_cap_clock_base_broken : 1;
        unsigned int quirk_no_endattr_in_nopdesc : 1;
        unsigned int quirk_missing_caps : 1;
        unsigned int quirk_multiblock_read_acmd12 : 1;
        unsigned int quirk_no_hispd_bit : 1;
        unsigned int quirk_broken_adma_zerolen_desc : 1;
        unsigned int quirk_unstable_ro_detect : 1;
    } bits;
};

union host_quirks2 {
    unsigned int quirks2_data;
    struct quirks2_bit_data {
        unsigned int quirk2_host_off_card_on : 1;
        unsigned int quirk2_host_no_cmd23 : 1;
        unsigned int quirk2_no_1_8_v : 1;
        unsigned int quirk2_preset_value_broken : 1;
        unsigned int quirk2_cad_no_needs_bus_on : 1;
        unsigned int quirk2_broken_host_control : 1;
        unsigned int quirk2_broken_hs200 : 1;
        unsigned int quirk2_broken_ddr50 : 1;
        unsigned int quirk2_stop_with_tc : 1;
        unsigned int quirk2_rdwr_tx_active_eot : 1;
        unsigned int quirk2_slow_int_clr : 1;
        unsigned int quirk2_always_use_base_clock : 1;
        unsigned int quirk2_ignore_datatout_for_r1bcmd : 1;
        unsigned int quirk2_broken_perset_value : 1;
        unsigned int quirk2_use_reserved_max_timeout : 1;
        unsigned int quirk2_divide_tout_by_4 : 1;
        unsigned int quirk2_ign_data_end_bit_error : 1;
        unsigned int quirk2_adma_skip_data_alignment : 1;
        unsigned int quirk2_nonstandard_clock : 1;
        unsigned int quirk2_caps_bit63_for_hs400 : 1;
        unsigned int quirk2_use_reset_workaround : 1;
        unsigned int quirk2_broken_led_control : 1;
        unsigned int quirk2_non_standard_tuning : 1;
        unsigned int quirk2_use_pio_for_emmc_tuning : 1;
    } bits;
};

// sdhci flags
#define SDHC_USE_SDMA  (1<<0)
#define SDHC_USE_ADMA  (1<<1)
#define SDHC_REQ_USE_DMA (1<<2)
#define SDHC_DEVICE_DEAD (1<<3)
#define SDHC_SDR50_NEEDS_TUNING (1<<4)
#define SDHC_NEEDS_RETUNING (1<<5)
#define SDHC_AUTO_CMD12 (1<<6)
#define SDHC_AUTO_CMD23 (1<<7)
#define SDHC_PV_ENABLED (1<<8)
#define SDHC_SDIO_IRQ_ENABLED (1<<9)
#define SDHC_SDR104_NEEDS_TUNING (1<<10)
#define SDHC_USING_RETUNING_TIMER (1<<11)
#define SDHC_USE_ADMA_64BIT  (1<<12)
#define SDHC_HOST_IRQ_STATUS  (1<<13)

#endif /* _MMC_CAPS_H */
