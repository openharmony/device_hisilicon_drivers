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

#ifndef SDHCI_H
#define SDHCI_H

#include "asm/dma.h"
#include "asm/io.h"
#include "asm/platform.h"
#include "device_resource_if.h"
#include "linux/scatterlist.h"
#include "los_bitmap.h"
#include "los_event.h"
#include "los_vm_zone.h"
#include "mmc_corex.h"
#include "osal_io.h"
#include "osal_irq.h"
#include "osal_time.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define REG_CTRL_SD_CLK     IO_DEVICE_ADDR(0x100C0040)
#define REG_CTRL_SD_CMD     IO_DEVICE_ADDR(0x100C0044)
#define REG_CTRL_SD_DATA0   IO_DEVICE_ADDR(0x100C0048)
#define REG_CTRL_SD_DATA1   IO_DEVICE_ADDR(0x100C004C)
#define REG_CTRL_SD_DATA2   IO_DEVICE_ADDR(0x100C0050)
#define REG_CTRL_SD_DATA3   IO_DEVICE_ADDR(0x100C0054)

#define REG_CTRL_EMMC_CLK   IO_DEVICE_ADDR(0x100C0014)
#define REG_CTRL_EMMC_CMD   IO_DEVICE_ADDR(0x100C0018)
#define REG_CTRL_EMMC_DATA0 IO_DEVICE_ADDR(0x100C0020)
#define REG_CTRL_EMMC_DATA1 IO_DEVICE_ADDR(0x100C001c)
#define REG_CTRL_EMMC_DATA2 IO_DEVICE_ADDR(0x100C0028)
#define REG_CTRL_EMMC_DATA3 IO_DEVICE_ADDR(0x100C0024)
#define REG_CTRL_EMMC_DATA4 IO_DEVICE_ADDR(0x100C0030)
#define REG_CTRL_EMMC_DATA5 IO_DEVICE_ADDR(0x100C0034)
#define REG_CTRL_EMMC_DATA6 IO_DEVICE_ADDR(0x100C0038)
#define REG_CTRL_EMMC_DATA7 IO_DEVICE_ADDR(0x100C003c)
#define REG_CTRL_EMMC_DS    IO_DEVICE_ADDR(0x100C0058)
#define REG_CTRL_EMMC_RST   IO_DEVICE_ADDR(0x100C005C)

#define REG_CTRL_SDIO_CLK   IO_DEVICE_ADDR(0x112C0048)
#define REG_CTRL_SDIO_CMD   IO_DEVICE_ADDR(0x112C004C)
#define REG_CTRL_SDIO_DATA0 IO_DEVICE_ADDR(0x112C0064)
#define REG_CTRL_SDIO_DATA1 IO_DEVICE_ADDR(0x112C0060)
#define REG_CTRL_SDIO_DATA2 IO_DEVICE_ADDR(0x112C005C)
#define REG_CTRL_SDIO_DATA3 IO_DEVICE_ADDR(0x112C0058)

/* macro for io_mux. */
#define IO_CFG_SR               (1 << 10)
#define IO_CFG_PULL_DOWN        (1 << 9)
#define IO_CFG_PULL_UP          (1 << 8)
#define IO_CFG_DRV_STR_MASK     (0xfU << 4)
#define IO_DRV_MASK             0x7f0

#define IO_DRV_STR_SEL(str)     ((str) << 4)

#define IO_MUX_CLK_TYPE_EMMC    0x0
#define IO_MUX_CLK_TYPE_SD      0x1
#define IO_MUX_SHIFT(type)      ((type) << 0)
#define IO_MUX_MASK             (0xfU << 0)

#define IO_DRV_SDIO_CLK  0x3
#define IO_DRV_SDIO_CMD  0x6
#define IO_DRV_SDIO_DATA 0x6

#define IO_DRV_SD_SDHS_CLK  0x5
#define IO_DRV_SD_SDHS_CMD  0x7
#define IO_DRV_SD_SDHS_DATA 0x7

#define IO_DRV_SD_OTHER_CLK  0x7
#define IO_DRV_SD_OTHER_CMD   IO_DRV_SD_OTHER_CLK
#define IO_DRV_SD_OTHER_DATA  IO_DRV_SD_OTHER_CLK

#define IO_DRV_EMMC_HS400_CLK  0x3
#define IO_DRV_EMMC_HS400_CMD  0x4
#define IO_DRV_EMMC_HS400_DATA 0x4
#define IO_DRV_EMMC_HS400_DS   0x3
#define IO_DRV_EMMC_HS400_RST  0x3

#define IO_DRV_EMMC_HS200_CLK  0x2
#define IO_DRV_EMMC_HS200_CMD  0x4
#define IO_DRV_EMMC_HS200_DATA 0x4
#define IO_DRV_EMMC_HS200_RST  0x3

#define IO_DRV_EMMC_HS_CLK  0x4
#define IO_DRV_EMMC_HS_CMD  0x6
#define IO_DRV_EMMC_HS_DATA 0x6
#define IO_DRV_EMMC_HS_RST  0x3

#define IO_DRV_EMMC_OTHER_CLK  0x5
#define IO_DRV_EMMC_OTHER_CMD  0x6
#define IO_DRV_EMMC_OTHER_DATA 0x6
#define IO_DRV_EMMC_OTHER_RST  0x3

#define PERI_CRG125 (CRG_REG_BASE + 0x01F4)
#define PERI_CRG126 (CRG_REG_BASE + 0x01F8)
#define PERI_CRG127 (CRG_REG_BASE + 0x01FC)
#define PERI_CRG135 (CRG_REG_BASE + 0x021C)
#define PERI_CRG136 (CRG_REG_BASE + 0x0220)
#define PERI_CRG139 (CRG_REG_BASE + 0x022C)
#define PERI_SD_DRV_DLL_CTRL (CRG_REG_BASE + 0x210)
#define PERI_SDIO_DRV_DLL_CTRL (CRG_REG_BASE + 0x228)
#define PERI_SD_SAMPL_DLL_STATUS (CRG_REG_BASE + 0x208)
#define PERI_SDIO_SAMPL_DLL_STATUS (CRG_REG_BASE + 0x224)

/*
 * PERI_CRG125/PERI_CRG139 details.
 * [28]Clock gating configuration. 0: disabled; 1: enabled.
 * [27]Soft reset request. 0: reset deasserted; 1: reset.
 * [26:24]Working clock selection. 000: 100KHz; 001: 400KHz; 010: 25MHz; 011: 50MKz;
 * 100: 90MKz(Supported only in eMMC mode); 101: 112.5MKz(Supported only in eMMC mode);
 * 110: 150MKz(Supported only in eMMC mode); others: reserved.
 */
#define SDHCI_MMC_FREQ_MASK 0x7
#define SDHCI_MMC_FREQ_SHIFT 24
#define SDHCI_CKEN (1U << 28)
#define SDHCI_CLK_SEL_100K   0
#define SDHCI_CLK_SEL_400K   1
#define SDHCI_CLK_SEL_25M    2
#define SDHCI_CLK_SEL_50M    3
#define SDHCI_CLK_SEL_90M    4
#define SDHCI_CLK_SEL_112P5M 5
#define SDHCI_CLK_SEL_150M   6
#define SDHCI_EMMC_CRG_REQ (1U << 27)
#define SDHCI_EMMC_CKEN (1U << 28)
#define SDHCI_EMMC_DLL_RST (1U << 29)

/*
 * PERI_CRG126/PERI_CRG135 details.
 * [4:0]Clock phase configuration for B clock debugging during edge detection. The default value is 90 degrees.
 * 0x00: 0; 0x01: 11.25; 0x02: 22.5; ... 0x1E: 337.5; 0x1F: 348.75; others: reserved.
 */
#define SDHCI_SAMPLB_DLL_CLK_MASK (0x1fU << 0)
#define SDHCI_SAMPLB_DLL_CLK 8
#define SDHCI_SAMPLB_SEL(phase) ((phase) << 0)

/*
 * PERI_CRG127/PERI_CRG136 details.
 * [28:24]Clock phase configuration. Default value: 180.
 * 0x00: 0; 0x01: 11.25; 0x02: 22.5; ... 0x1E: 337.5; 0x1F: 348.75; others: reserved.
 */
#define SDHCI_DRV_CLK_PHASE_SHFT 24
#define SDHCI_DRV_CLK_PHASE_MASK (0x1f << 24)
#define SDHCI_PHASE_112P5_DEGREE 10        /* 112.5 degree */
#define SDHCI_PHASE_258P75_DEGREE 23       /* 258.75 degree */
#define SDHCI_PHASE_225_DEGREE 20          /* 225 degree */
#define SDHCI_PHASE_180_DEGREE 16          /* 180 degree */

/*
 * PERI_SD_DRV_DLL_CTRL/PERI_SDIO_DRV_DLL_CTRL details.
 */
#define SDHCI_DRV_DLL_LOCK (1U << 15)

/*
 * PERI_SD_SAMPL_DLL_STATUS/PERI_SDIO_SAMPL_DLL_STATUS details.
 */
#define SDHCI_SAMPL_DLL_DEV_READY 1
#define SDHCI_SAMPL_DLL_DEV_EN (1U << 16)

#define SDHCI_MMC_FREQ_100K       100000
#define SDHCI_MMC_FREQ_400K       400000
#define SDHCI_MMC_FREQ_25M        25000000
#define SDHCI_MMC_FREQ_50M        50000000
/* only support for EMMC chip */
#define SDHCI_MMC_FREQ_90M        90000000
#define SDHCI_MMC_FREQ_112P5M     112500000
#define SDHCI_MMC_FREQ_150M       150000000

#define SDHCI_CMD_DATA_REQ_TIMEOUT  (LOSCFG_BASE_CORE_TICK_PER_SECOND * 10) /* 30 s */
#define SDHCI_CMD_REQ_TIMEOUT  (LOSCFG_BASE_CORE_TICK_PER_SECOND * 1) /* 1s */

/* define event lock */
typedef EVENT_CB_S SDHCI_EVENT;
#define SDHCI_EVENT_INIT(event) LOS_EventInit(event)
#define SDHCI_EVENT_SIGNAL(event, bit) LOS_EventWrite(event, bit)
#define SDHCI_EVENT_WAIT(event, bit, timeout) LOS_EventRead(event, bit, (LOS_WAITMODE_OR + LOS_WAITMODE_CLR), timeout)
#define SDHCI_EVENT_DELETE(event) LOS_EventDestroy(event)

/* define irq lock */
#define SDHCI_IRQ_LOCK(flags)    do { (*(flags)) = LOS_IntLock(); } while (0)
#define SDHCI_IRQ_UNLOCK(flags)  do { LOS_IntRestore(flags); } while (0)

#define SDHCI_SG_DMA_ADDRESS(sg) ((sg)->dma_address)
#ifdef CONFIG_NEED_SG_DMA_LENGTH
#define SDHCI_SG_DMA_LEN(sg)     ((sg)->dma_length)
#else
#define SDHCI_SG_DMA_LEN(sg)     ((sg)->length)
#endif

/*
 * Host SDMA buffer boundary. Valid values from 4K to 512K in powers of 2.
 */
#define SDHCI_DEFAULT_BOUNDARY_SIZE  (512 * 1024)
#define SDHCI_DEFAULT_BOUNDARY       19
#define SDHCI_DEFAULT_BOUNDARY_ARG   (SDHCI_DEFAULT_BOUNDARY - 12)

enum SdhciDmaDataDirection {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

enum SdhciHostRegister {
    SDMASA_R = 0x0000,
    BLOCKSIZE_R = 0x0004,
    BLOCKCOUNT_R = 0x0006,
    ARGUMENT_R = 0x0008,
    XFER_MODE_R = 0x000c,
    CMD_R = 0x000e,
    RESP01_R = 0x0010,
    RESP23_R = 0x0014,
    RESP45_R = 0x0018,
    RESP67_R = 0x001c,
    BUF_DATA_R = 0x0020,
    PSTATE_R = 0x0024,
    HOST_CTRL1_R = 0x0028,
    PWR_CTRL_R = 0x0029,
    BLOCK_GAP_CTRL_R = 0x002a,
    WUP_CTRL_R = 0x002b,
    CLK_CTRL_R = 0x002c,
    TOUT_CTRL_R = 0x002e,
    SW_RST_R = 0x002f,
    NORMAL_INT_STAT_R = 0x0030,
    ERROR_INT_STAT_R = 0x0032,
    NORMAL_INT_STAT_EN_R = 0x0034,
    ERROR_INT_STAT_EN_R = 0x0036,
    NORMAL_INT_SIGNAL_EN_R = 0x0038,
    ERROR_INT_SIGNAL_EN_R = 0x003a,
    AUTO_CMD_STAT_R = 0x003c,
    HOST_CTRL2_R = 0x003e,
    CAPABILITIES1_R = 0x0040,
    CAPABILITIES2_R = 0x0044,
    CURR_CAPBILITIES1_R = 0x0048,
    ADMA_ERR_STAT_R = 0x0054,
    ADMA_SA_LOW_R = 0x0058,
    ADMA_SA_HIGH_R = 0x005c,
    ADMA_ID_LOW_R = 0x0078,
    ADMA_ID_HIGH_R = 0x007c,
    SLOT_INT_STATUS_R = 0x00FC,
    HOST_VERSION_R = 0x00FE,
    MSHC_VER_ID_R = 0x0500,
    MSHC_VER_TYPE_R = 0x0504,
    MSHC_CTRL_R = 0x0508,
    MBIU_CTRL_R = 0x0510,
    EMMC_CTRL_R = 0x052c,
    BOOT_CTRL_R = 0x052e,
    EMMC_HW_RESET_R = 0x0534,
    AT_CTRL_R = 0x0540,
    AT_STAT_R = 0x0544,
    MULTI_CYCLE_R = 0x054c
};

/*
 * SDMASA_R(0x0000) details.
 * [31:0]Whether to use the built-in DMA to transfer data.
 * If Host Version Enable is set to 0, it indicates the SDMA system address.
 * If Host Version Enable is set to 1, it indicates the block count. 1 indicates a block.
 */
#define SDHCI_DMA_ADDRESS 0x00
#define SDHCI_ARGUMENT2 SDHCI_DMA_ADDRESS

/*
 * BLOCKSIZE_R(0x0004) details.
 * [14:12]Boundary value of an SDMA data block. 000: 4K bytes; 001: 8K bytes; ... 111: 512K bytes.
 * [11:0]Transfer block zise. 0x800: 2048bytes.
 */
#define SDHCI_MAKE_BLKSZ(dma, blksz) (((dma & 0x7) << 12) | (blksz & 0xFFF))

/*
 * XFER_MODE_R(0x000c) details.
 * [15:9]reserved.
 * [8]Disable the response interrupt. 0: enable; 1: disable.
 * [7]Response error check enable. 0: disable; 1: enable.
 * [6]Response R1/R5 Type. 0: R1; 1: R5.
 * [5]Multiple/single block select. 0: single block; 1: multiple block.
 * [4]Data transfer direction. 0: Controller-to-card; 1: card-to-Controller.
 * [3:2]Auto Command Enable. 00: close; 01: Auto CMD12 Enable; 10: Auto CMD23 Enable; 11: Auto CMD Auto Select.(sdio 00)
 * [1]Block count Enable. 0: disable; 1: enable.
 * [0]DMA Enable. 0: No data is transmitted or non-DMA data is transmitted; 1: DMA data transfer.
 */
#define SDHCI_TRNS_DMA (1 << 0)
#define SDHCI_TRNS_BLK_CNT_EN (1 << 1)
#define SDHCI_TRNS_AUTO_CMD12 (1 << 2)
#define SDHCI_TRNS_AUTO_CMD23 (1 << 3)
#define SDHCI_TRNS_READ (1 << 4)
#define SDHCI_TRNS_MULTI (1 << 5)

/*
 * CMD_R(0x000e) details.
 * [15:14]reserved.
 * [13:8]cmd index.
 * [7:6]cmd type. 00: Normal cmd; 01: Suspend cmd; 10: Resume cmd; 11: Abort cmd.
 * [5]Whether data is being transmitted. 0: No data is transmitted. 1: Data is being transmitted.
 * [4]Command ID check enable. 0: disable; 1: enable.
 * [3]Command CRC check enable. 0: disable; 1: enable.
 * [2]Subcommand ID. 0: Main; 1: Sub Command.
 * [1:0]Response type. 00: no response; 01: response Length 136; 10: response length 48; 11: response length 48 check.
 */
#define SDHCI_CMD_CRC_CHECK_ENABLE 0x08
#define SDHCI_CMD_INDEX_CHECK_ENABLE 0x10
#define SDHCI_CMD_DATA_TX 0x20

#define SDHCI_CMD_NONE_RESP 0x00
#define SDHCI_CMD_LONG_RESP 0x01
#define SDHCI_CMD_SHORT_RESP 0x02
#define SDHCI_CMD_SHORT_RESP_BUSY 0x03

#define SDHCI_GEN_CMD(c, f) (((c & 0xff) << 8) | (f & 0xff))
#define SDHCI_PARSE_CMD(c) ((c >> 8) & 0x3f)

/*
 * PSTATE_R(0x0024) details.
 * [31:28]reserved.
 * [27]Command sending error status. 0: No error occurs in the sent command. 1: The command cannot be sent.
 * [26:25]reserved.
 * [24]CMD pin status. 0: The cmd pin is at low level. 1: The cmd pin is at high level.
 * [23:20]Data[3:0] pin status, meaning of each bit. 0: low level; 1: high level.
 * [18]Card_detect_n pin status. 0: low level; 1: high level.
 * [11]Buffer read enable. 0 : disable; 1: enable.
 * [10]Buffer write enable. 0 : disable; 1: enable.
 * [9]Read transfer valid. 0: idle; 1: Data is being read.
 * [8]Write transfer valid. 0: idle; 1: Data is being written.
 * [7:4]Data[7:4] pin status, meaning of each bit. 0: low level; 1: high level.
 * [3]reserved.
 * [2]The data line is valid. 0: idle; 1: Data is being transmitted.
 * [1]The command with data is valid. 0: idle state, Commands with data can be sent.
 * 1: The data line is being transferred or the read operation is valid.
 * [0]The command line is valid. 0: idle state. The controller can send commands.
 * 1: busy. The controller cannot send commands.
 */
#define SDHCI_CMD_INVALID 0x00000001
#define SDHCI_DATA_INVALID 0x00000002
#define SDHCI_CARD_PRESENT 0x00010000
#define SDHCI_WRITE_PROTECT 0x00080000
#define SDHCI_DATA_0_LEVEL_MASK 0x00100000

/*
 * HOST_CTRL1_R(0x0028) details.
 * [7]Card detection signal select. 0: card detection signal card_detect_n (common use); 1: card detection test level.
 * [6]Card detection test level. 0: no card; 1: a card is inserted.
 * [5]reserved.
 * [4:3]DMA select. When Host Version 4 Enable in HOST_CTRL2_R is 1: 00: SDMA; 01: ADMA1; 10: ADMA2;
 * 11: ADMA2 or ADMA3. When Host Version 4 Enable in HOST_CTRL2_R is 0: 00: SDMA; 01: reserved;
 * 10: 32-bit address ADMA2; 11: 64-bit address ADMA2.
 * [2]High-speed enable. 0: Normal Speed; 1: High Speed.
 * [1]Bit width of the data transfer. 0: 1bit; 1: 4bit.
 * [0]reserved.
 */
#define SDHCI_CTRL_4_BIT_BUS 0x02
#define SDHCI_CTRL_HIGH_SPEED 0x04
#define SDHCI_CTRL_DMA_ENABLE_MASK 0x18
#define SDHCI_CTRL_SDMA_ENABLE 0x00
#define SDHCI_CTRL_ADMA1_ENABLE 0x08
#define SDHCI_CTRL_ADMA32_ENABLE 0x10
#define SDHCI_CTRL_ADMA64_ENABLE 0x18
#define SDHCI_CTRL_8_BIT_BUS 0x20

/*
 * PWR_CTRL_R(0x0029) details.
 * [0]VDD2 Power enable. 0: power off; 1: power on.
 */
#define SDHCI_POWER_ON 0x01
#define SDHCI_POWER_180 0x0A
#define SDHCI_POWER_300 0x0C
#define SDHCI_POWER_330 0x0E

/*
 * CLK_CTRL_R(0x002c) details.
 * [15:4]reserved.
 * [3]PLL enable. 0: The PLL is in low power mode. 1: enabled.
 * [2]SD/eMMC clock enable. 0: disabled; 1: enabled.
 * [1]Internal clock status. 0: unstable; 1: stable.
 * [0]Internal clock enable. 0: disabled; 1: enabled.
 */
#define SDHCI_CLK_CTRL_PLL_EN (1 << 3)
#define SDHCI_CLK_CTRL_CLK_EN (1 << 2)
#define SDHCI_CLK_CTRL_INT_STABLE (1 << 1)
#define SDHCI_CLK_CTRL_INT_CLK_EN (1 << 0)

/*
 * TOUT_CTRL_R(0x002e) details.
 * [3:0]Data timeout count. 0x0: TMCLK x 2^13; 0xe: TMCLK x 2^27; others: reserved.
 */
#define SDHCI_DEFINE_TIMEOUT 0xE

/*
 * SW_RST_R(0x002f) details.
 * [7:3]reserved.
 * [2]Data line soft reset request. 0: not reset; 1: reset.
 * [1]Command line soft reset request. 0: not reset; 1: reset.
 * [0]Soft reset request of the controller. 0: not reset; 1: reset.
 */
#define SDHCI_RESET_ALL 0x01
#define SDHCI_RESET_CMD 0x02
#define SDHCI_RESET_DATA 0x04

/*
 * NORMAL_INT_STAT_R(0x0030) details.
 * [15]Summary error interrupt status. [14]reserved. [13]TX event interrupt status.
 * [12]Retuning event interrupt status. [11]INT_C interrupt status. [10]INT_B interrupt status.
 * [9]INT_A interrupt status. [8]Card interrupt status. [7]Card removal interrupt status.
 * [6]Card insertion interrupt status. [5]Buffer read ready interrupt status. [4]Buffer write ready interrupt status.
 * [3]DMA interrupt status. [2]Block gap event interrupt status due to a stop request.
 * [1]Read/write transfer completion interrupt status. [0]Command completion interrupt status.
 */
#define SDHCI_INTERRUPT_RESPONSE 0x00000001
#define SDHCI_INTERRUPT_DATA_END 0x00000002
#define SDHCI_INTERRUPT_BLK_GAP 0x00000004
#define SDHCI_INTERRUPT_DMA_END 0x00000008
#define SDHCI_INTERRUPT_SPACE_AVAIL 0x00000010
#define SDHCI_INTERRUPT_DATA_AVAIL 0x00000020
#define SDHCI_INTERRUPT_CARD_INSERT 0x00000040
#define SDHCI_INTERRUPT_CARD_REMOVE 0x00000080
#define SDHCI_INTERRUPT_CARD_INT 0x00000100
#define SDHCI_INTERRUPT_ERROR 0x00008000

/*
 * NORMAL_INT_SIGNAL_EN_R(0x0038) details.
 */
#define SDHCI_INTERRUPT_TIMEOUT 0x00010000
#define SDHCI_INTERRUPT_CRC 0x00020000
#define SDHCI_INTERRUPT_END_BIT 0x00040000
#define SDHCI_INTERRUPT_INDEX 0x00080000
#define SDHCI_INTERRUPT_DATA_TIMEOUT 0x00100000
#define SDHCI_INTERRUPT_DATA_CRC 0x00200000
#define SDHCI_INTERRUPT_DATA_END_BIT 0x00400000
#define SDHCI_INTERRUPT_BUS_POWER 0x00800000
#define SDHCI_INTERRUPT_AUTO_CMD_ERR 0x01000000
#define SDHCI_INTERRUPT_ADMA_ERROR 0x02000000

#define SDHCI_INT_CMD_MASK (SDHCI_INTERRUPT_RESPONSE | SDHCI_INTERRUPT_TIMEOUT | SDHCI_INTERRUPT_CRC | \
    SDHCI_INTERRUPT_END_BIT | SDHCI_INTERRUPT_INDEX | SDHCI_INTERRUPT_AUTO_CMD_ERR)

#define SDHCI_INT_DATA_MASK (SDHCI_INTERRUPT_DATA_END | SDHCI_INTERRUPT_DMA_END | SDHCI_INTERRUPT_DATA_AVAIL | \
    SDHCI_INTERRUPT_SPACE_AVAIL | SDHCI_INTERRUPT_DATA_TIMEOUT | SDHCI_INTERRUPT_DATA_CRC | \
    SDHCI_INTERRUPT_DATA_END_BIT | SDHCI_INTERRUPT_ADMA_ERROR | SDHCI_INTERRUPT_BLK_GAP)

/*
 * HOST_CTRL2_R(0x003e) details.
 * [9:8]reserved; [3]reserved.
 * [15]Automatic selection enable of the preset value. 0: disabled, 1: enabled.
 * [14]Async interrupt enable. 0: disabled; 1: enabled.
 * [13]Bus 64-bit address enable. 0: disabled; 1: enabled.
 * [12]Controller version 4 enable. 0: 3.0; 1: 4.0.
 * [11]CMD23 enable. 0: disabled; 1: enabled.
 * [10]ADMA2 length. 0:16-bit data length; 1:26-bit data length.
 * [7]Sample clock select. 0: Select the fixed clock to collect data. 1: tuned clock.
 * [6]Run the tuning command. The value is automatically cleared after the tuning operation is complete.
 * 0: Tuning is not performed or tuning is complete. 1: tuning.
 * [5:4]Drive capability select. 00: typeB; 01: typeA; 10: typeC; 11: typeD.
 * [2:0]emmc mode. 000: Legacy; 001: High Speed SDR; 010: HS200; others: reserved.
 */
#define SDHCI_UHS_MASK 0x0007
#define SDHCI_UHS_SDR12 0x0000
#define SDHCI_UHS_SDR25 0x0001
#define SDHCI_UHS_SDR50 0x0002
#define SDHCI_UHS_SDR104 0x0003
#define SDHCI_UHS_DDR50 0x0004
#define SDHCI_HS_SDR200 0x0005 /* reserved value in SDIO spec */
#define SDHCI_HS400 0x0007
#define SDHCI_VDD_180 0x0008
#define SDHCI_DRV_TYPE_MASK 0x0030
#define SDHCI_DRV_TYPE_B 0x0000
#define SDHCI_DRV_TYPE_A 0x0010
#define SDHCI_DRV_TYPE_C 0x0020
#define SDHCI_DRV_TYPE_D 0x0030
#define SDHCI_EXEC_TUNING 0x0040
#define SDHCI_TUNED_CLK 0x0080
#define SDHCI_ASYNC_INT_ENABLE 0x4000
#define SDHCI_PRESET_VAL_ENABLE 0x8000

/*
 * CAPABILITIES1_R(0x0040) details.
 * [31:30]Slot type. 00: Removable card slot; 01: Embedded Slot;
 * 10: Shared bus slot (used in SD mode); 11:UHS2 (not supported currently).
 * [29]Whether the sync interrupt is supported. 0: not supported; 1: supported.
 * [28]The 64-bit system address is used for V3. 0: not supported; 1: supported.
 * [27]The 64-bit system address is used for V4. 0: not supported; 1: supported.
 * [26]The voltage is 1.8 V. 0: not supported; 1: supported.
 * [25]The voltage is 3.0 V. 0: not supported; 1: supported.
 * [24]The voltage is 3.3 V. 0: not supported; 1: supported.
 * [23]Suspending and Resuming Support. 0: not supported; 1: supported.
 * [22]SDMA Support. 0: not supported; 1: supported.
 * [21]Indicates whether to support high speed. 0: not supported; 1: supported.
 * [19]Indicates whether to support ADMA2. 0: not supported; 1: supported.
 * [18]Whether the 8-bit embedded component is supported. 0: not supported; 1: supported.
 * [17:16]Maximum block length. 0x0: 512Byte; 0x1: 1024Byte; 0x2: 2048Byte; 0x3: reserved.
 * [15:8]Basic frequency of the clock. 0x0: 1MHz; 0x3F: 63MHz; 0x40~0xFF: not supported.
 * [7]Timeout clock unit. 0: KHz; 1: MHz.
 * [5:0]Timeout interval. 0x1: 1KHz/1MHz; 0x2: 2KHz/2MHz; ... 0x3F: 63KHz/63MHz
 */
#define SDHCI_TIMEOUT_CLK_UNIT 0x00000080
#define SDHCI_CLK_BASE_MASK 0x00003F00
#define SDHCI_BASIC_FREQ_OF_CLK_MASK 0x0000FF00
#define SDHCI_BASIC_FREQ_OF_CLK_SHIFT 8
#define SDHCI_MAX_BLOCK_SIZE_MASK 0x00030000
#define SDHCI_MAX_BLOCK_SIZE_SHIFT 16
#define SDHCI_SUPPORT_8BIT 0x00040000
#define SDHCI_SUPPORT_ADMA2 0x00080000
#define SDHCI_SUPPORT_HISPD 0x00200000
#define SDHCI_SUPPORT_SDMA 0x00400000
#define SDHCI_SUPPORT_VDD_330 0x01000000
#define SDHCI_SUPPORT_VDD_300 0x02000000
#define SDHCI_SUPPORT_VDD_180 0x04000000
#define SDHCI_SUPPORT_64BIT 0x10000000
#define SDHCI_SUPPORT_ASYNC_INT 0x20000000

/*
 * CAPABILITIES2_R(0x0044) details.
 * [28]Whether the 1.8V VDDR is supported. 0: not supported; 1: supported.
 * [27]Whether the ADMA3 is supported. 0: not supported; 1: supported.
 * [15:14]Retuning mode. 00: MODE1, Timer; 01: MODE2, Timer and ReTuning request;
 * 10: MODE3, Auto retuning Timer and ReTuning request; 11: reserved.
 * [13]SDR50 uses Tuning. 0: not use; 1: use.
 * [11:8]Retuning count. 0x1: 1s; 0x3: 4s; others: reserved.
 * [6]Support TYPED. 0: not supported; 1: supported.
 * [5]Support TYPEC. 0: not supported; 1: supported.
 * [4]Support TYPEA. 0: not supported; 1: supported.
 * [3]Support UHS2. 0: not supported; 1: supported.
 * [2]Support DDR50. 0: not supported; 1: supported.
 * [1]Support SDR104. 0: not supported; 1: supported.
 * [0]Support SDR50. 0: not supported; 1: supported.
 */
#define SDHCI_SUPPORT_SDR50 0x00000001
#define SDHCI_SUPPORT_SDR104 0x00000002
#define SDHCI_SUPPORT_DDR50 0x00000004
#define SDHCI_SUPPORT_DRIVER_TYPE_A 0x00000010
#define SDHCI_SUPPORT_DRIVER_TYPE_C 0x00000020
#define SDHCI_SUPPORT_DRIVER_TYPE_D 0x00000040
#define SDHCI_RETUNING_TIMER_COUNT_MASK 0x00000F00
#define SDHCI_RETUNING_TIMER_COUNT_SHIFT 8
#define SDHCI_USE_SDR50_TUNING 0x00002000
#define SDHCI_RETUNING_MODE_MASK 0x0000C000
#define SDHCI_RETUNING_MODE_SHIFT 14
#define SDHCI_CLK_MUL_MASK 0x00FF0000
#define SDHCI_CLK_MUL_SHIFT 16
#define SDHCI_SUPPORT_ADMA3 0x8000000

/*
 * HOST_VERSION_R(0x00FE) details.
 */
#define SDHCI_HOST_SPEC_VER_MASK 0x00FF
#define SDHCI_HOST_SPEC_100 0
#define SDHCI_HOST_SPEC_200 1
#define SDHCI_HOST_SPEC_300 2
#define SDHCI_HOST_SPEC_400 3
#define SDHCI_HOST_SPEC_410 4
#define SDHCI_HOST_SPEC_420 5

/*
 * EMMC_CTRL_R(0x0508) details.
 */
#define SDHC_CMD_CONFLIT_CHECK     0x01

/*
 * MBIU_CTRL_R(0x0510) details.
 * [3]16 burst enable. 0: disable 16 burst; 1: enable 16 burst.
 * [2]8 burst enable. 0: disable 8 burst; 1: enable 8 burst.
 * [1]4 burst enable. 0: disable 4 burst; 1: enable 4 burst.
 * [0]Burst configuration validation enable.
 * 0: Generate fixed bursts by configuring gm_enburst4, gm_enburst8, gm_enburst16;
 * 1: Generate bursts based on the actual data length.
 */
#define SDHCI_GM_WR_OSRC_LMT_MASK (0x7 << 24)
#define SDHCI_GM_RD_OSRC_LMT_MASK (0x7 << 16)
#define SDHCI_GM_WR_OSRC_LMT_VAL (7 << 24)
#define SDHCI_GM_RD_OSRC_LMT_VAL (7 << 16)
#define SDHCI_UNDEFL_INCR_EN 0x1

/*
 * EMMC_CTRL_R(0x052c) details.
 * [15:10]reserved; [8:2]reserved.
 * [9]Algorithm for sorting tasks. 0: Tasks with a higher priority are executed first.
 * Tasks with the same priority are executed first in first. 1: first come first execute.
 * [1]Disable the CRC check. 0: Data CRC check is enabled. 1: The CRC check is disabled.
 * [0]Type of the connected card. 0: non-eMMC card; 1: eMMC card.
 */
#define SDHCI_EMMC_CTRL_EMMC (1 << 0)
#define SDHCI_EMMC_CTRL_ENH_STROBE_EN (1 << 8)

/*
 * AT_CTRL_R(0x0540) details.
 * [4]Software configuration tuning enable. 0: disable; 1: enable.
 */
#define SDHCI_SW_TUNING_EN 0x00000010

/*
 * AT_STAT_R(0x0544) details.
 * [7:0]Phase value configured by software.
 */
#define SDHCI_CENTER_PH_CODE_MASK   0x000000ff
#define SDHCI_SAMPLE_PHASE 4
#define SDHCI_PHASE_SCALE 32
#define SDHCI_PHASE_SCALE_TIMES 4

/*
 * MULTI_CYCLE_R(0x054C) details.
 */
#define SDHCI_FOUND_EDGE (0x1 << 11)
#define SDHCI_EDGE_DETECT_EN (0x1 << 8)
#define SDHCI_DOUT_EN_F_EDGE (0x1 << 6)
#define SDHCI_DATA_DLY_EN (0x1 << 3)
#define SDHCI_CMD_DLY_EN (0x1 << 2)

/*
 * End of controller registers.
 */
#define ADMA2_END 0x2

#define SDHCI_USE_SDMA  (1 << 0)
#define SDHCI_USE_ADMA  (1 << 1)
#define SDHCI_REQ_USE_DMA (1 << 2)
#define SDHCI_DEVICE_DEAD (1 << 3)
#define SDHCI_SDR50_NEEDS_TUNING (1 << 4)
#define SDHCI_NEEDS_RETUNING (1 << 5)
#define SDHCI_AUTO_CMD12 (1 << 6)
#define SDHCI_AUTO_CMD23 (1 << 7)
#define SDHCI_PV_ENABLED (1 << 8)
#define SDHCI_SDIO_IRQ_ENABLED (1 << 9)
#define SDHCI_SDR104_NEEDS_TUNING (1 << 10)
#define SDHCI_USING_RETUNING_TIMER (1 << 11)
#define SDHCI_USE_64BIT_ADMA  (1 << 12)
#define SDHCI_HOST_IRQ_STATUS  (1 << 13)

#define SDHCI_ADMA_MAX_DESC 128
#define SDHCI_ADMA_DEF_SIZE ((SDHCI_ADMA_MAX_DESC * 2 + 1) * 4)
#define SDHCI_ADMA_LINE_SIZE 8
#define SDHCI_ADMA_64BIT_LINE_SIZE 12
#define SDHCI_MAX_DIV_SPEC_200 256
#define SDHCI_MAX_DIV_SPEC_300 2046

union SdhciHostQuirks {
    uint32_t quirksData;
    struct QuirksBitData {
        uint32_t brokenCardDetection : 1;
        uint32_t forceSWDetect : 1; /* custom requirement: use the SD protocol to detect rather then the interrupt. */
        uint32_t invertedWriteProtect : 1;
        uint32_t noEndattrInNopdesc : 1;
        uint32_t reserved : 28;
    }bits;
};

#define SDHCI_PEND_REQUEST_DONE (1 << 0)
#define SDHCI_PEND_ACCIDENT (1 << 1)
struct SdhciHost {
    struct MmcCntlr *mmc;
    struct MmcCmd *cmd;
    void *base;
    uint32_t irqNum;
    uint32_t irqEnable;
    uint32_t hostId;
    union SdhciHostQuirks quirks;
    uint32_t flags;
    uint16_t version;
    uint32_t maxClk;
    uint32_t clkMul;
    uint32_t clock;
    uint8_t pwr;
    bool presetEnabled;
    struct OsalMutex mutex;
    SDHCI_EVENT sdhciEvent;
    bool waitForEvent;
    uint8_t *alignedBuff;
    uint32_t buffLen;
    struct scatterlist dmaSg;
    struct scatterlist *sg;
    uint32_t dmaSgCount;
    char *admaDesc;
    uint32_t admaDescSize;
    uint32_t admaDescLineSize;
    uint32_t admaMaxDesc;
    uint32_t tuningPhase;
};

static inline void SdhciWritel(struct SdhciHost *host, uint32_t val, int reg)
{
    OSAL_WRITEL(val, (uintptr_t)host->base + reg);
}

static inline void SdhciWritew(struct SdhciHost *host, uint16_t val, int reg)
{
    OSAL_WRITEW(val, (uintptr_t)host->base + reg);
}

static inline void SdhciWriteb(struct SdhciHost *host, uint8_t val, int reg)
{
    OSAL_WRITEB(val, (uintptr_t)host->base + reg);
}

static inline uint32_t SdhciReadl(struct SdhciHost *host, int reg)
{
    return OSAL_READL((uintptr_t)host->base + reg);
}

static inline uint16_t SdhciReadw(struct SdhciHost *host, int reg)
{
    return OSAL_READW((uintptr_t)host->base + reg);
}

static inline uint8_t SdhciReadb(struct SdhciHost *host, int reg)
{
    return OSAL_READB((uintptr_t)host->base + reg);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* SDHCI_H */
