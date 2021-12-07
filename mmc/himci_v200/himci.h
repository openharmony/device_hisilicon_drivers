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

#ifndef HIMCI_H
#define HIMCI_H

#include "asm/dma.h"
#include "asm/io.h"
#include "asm/platform.h"
#include "device_resource_if.h"
#include "linux/scatterlist.h"
#include "los_event.h"
#include "los_vm_iomap.h"
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

#define HIMCI_MAX_RETRY_COUNT 100
#define HIMCI_PAGE_SIZE 4096
#define HIMCI_DMA_MAX_BUFF_SIZE 0x1000

#define HIMCI_MMC_FREQ_150M 150000000
#define HIMCI_MMC_FREQ_100M 100000000
#define HIMCI_MMC_FREQ_50M  50000000
#define HIMCI_MMC_FREQ_25M  25000000

/* register mapping */
#define PERI_CRG49  (CRG_REG_BASE + 0xC4)
#define PERI_CRG50  (CRG_REG_BASE + 0xC8)
#define PERI_CRG82  (CRG_REG_BASE + 0x0148)
#define PERI_CRG83  (CRG_REG_BASE + 0x014C)
#define PERI_CRG84  (CRG_REG_BASE + 0x0150)
#define PERI_CRG85  (CRG_REG_BASE + 0x0154)
#define PERI_CRG86  (CRG_REG_BASE + 0x0158)
#define PERI_CRG87  (CRG_REG_BASE + 0x015C)
#define PERI_CRG88  (CRG_REG_BASE + 0x0160)
#define PERI_CRG89  (CRG_REG_BASE + 0x0164)
#define PERI_CRG90  (CRG_REG_BASE + 0x0168)

/*
 * PERI_CRG82/PERI_CRG88/PERI_CRG85 details.
 * [3:2]Working clock selection. 01: 100MHz; 10: 50MHz; 11: 25MHz.
 * [1]Clock gating. 0: disabled; 1: enabled.
 * [0]Soft reset request. 0: reset deasserted; 1: reset.
 */
#define HIMCI_CLK_SEL_MASK (3U << 2)
#define HIMCI_CLK_SEL_100M (1U << 2)
#define HIMCI_CLK_SEL_50M (2U << 2)
#define HIMCI_CLK_SEL_25M (3U << 2)
#define HIMCI_CKEN (1U << 1)
#define HIMCI_RESET (1U << 0)

/*
 * PERI_CRG83/PERI_CRG89/PERI_CRG86 details.
 * [19]SAP_DLL device delay line enable. 0: The device stops working. 1: The device starts to work.
 * [18]SAP_DLL host calculation clock cycle disable signal. 0: enabled; 1: Disable clock detection.
 * [17]SAP_DLL device LINE bypass. 0: normal mode; 1: device line bypass.
 * [16]SAP_DLL mode select. 0: normal mode; 1: The device line is controlled by the SAP_DLL_dllssel.
 * [15:8]SAP_DLL device LINE delay level select, valid when SAP_DLL_dllmode is high.
 * [7:4]SAP_DLL device tap calibration.
 * [1]SAP_DLL soft reset. 0: reset deasserted; 1: reset. [0]SAP_DLL clock gating. 0: disabled; 1: enabled.
 */
#define HIMCI_SAP_DLL_DEVICE_DELAY_ENABLE (1U << 19)
#define HIMCI_SAP_DLL_MODE_DLLSSEL        (1U << 16)
#define HIMCI_SAP_DLL_SOFT_RESET          (1U << 0)
#define HIMCI_SAP_DLL_ELEMENT_SHIFT       8

/* HI MCI CONFIGS */
#define HIMCI_REQUEST_TIMEOUT    (10 * LOSCFG_BASE_CORE_TICK_PER_SECOND) /* 10s */
#define HIMCI_TUNINT_REQ_TIMEOUT (LOSCFG_BASE_CORE_TICK_PER_SECOND / 5)  /* 0.2s */
#define HIMCI_CARD_COMPLETE_TIMEOUT (5 * LOSCFG_BASE_CORE_TICK_PER_SECOND) /* 5s */

#define HIMCI_READL(addr) OSAL_READL((uintptr_t)(addr))

#define HIMCI_WRITEL(v, addr) OSAL_WRITEL((v), (uintptr_t)(addr))

#define HIMCI_CLEARL(host, reg, v) OSAL_WRITEL(OSAL_READL((uintptr_t)(host)->base + (reg)) & (~(v)), \
    (uintptr_t)(host)->base + (reg));

#define HIMCI_SETL(host, reg, v) OSAL_WRITEL(OSAL_READL((uintptr_t)(host)->base + (reg)) | (v), \
    (uintptr_t)(host)->base + (reg));

/* define event lock */
typedef EVENT_CB_S HIMCI_EVENT;
#define HIMCI_EVENT_INIT(event) LOS_EventInit(event)
#define HIMCI_EVENT_SIGNAL(event, bit) LOS_EventWrite(event, bit)
#define HIMCI_EVENT_WAIT(event, bit, timeout) LOS_EventRead(event, bit, (LOS_WAITMODE_OR + LOS_WAITMODE_CLR), timeout)
#define HIMCI_EVENT_DELETE(event) LOS_EventDestroy(event)

/* define task/irq lock */
#define HIMCI_TASK_LOCK(lock)    do { LOS_TaskLock(); } while (0)
#define HIMCI_TASK_UNLOCK(lock)  do { LOS_TaskUnlock(); } while (0)
#define HIMCI_IRQ_LOCK(flags)    do { (*(flags)) = LOS_IntLock(); } while (0)
#define HIMCI_IRQ_UNLOCK(flags)  do { LOS_IntRestore(flags); } while (0)

#define HIMCI_SG_DMA_ADDRESS(sg) ((sg)->dma_address)
#ifdef CONFIG_NEED_SG_DMA_LENGTH
#define HIMCI_SG_DMA_LEN(sg)     ((sg)->dma_length)
#else
#define HIMCI_SG_DMA_LEN(sg)     ((sg)->length)
#endif

#define REG_CTRL_NUM 4
#define REG_CTRL_EMMC_START IO_DEVICE_ADDR(0x10ff0000 + 0x0) /* eMMC pad ctrl reg */
#define REG_CTRL_SD_START IO_DEVICE_ADDR(0x10ff0000 + 0x24)  /* sd pad ctrl reg */
#define REG_CTRL_SDIO_START IO_DEVICE_ADDR(0x112f0000 + 0x8) /* sdio pad ctrl reg */

enum HimciPowerStatus {
    HOST_POWER_OFF,
    HOST_POWER_ON,
};

enum HimciDmaDataDirection {
    DMA_BIDIRECTIONAL = 0,
    DMA_TO_DEVICE = 1,
    DMA_FROM_DEVICE = 2,
    DMA_NONE = 3,
};

enum HimciHostRegister {
    MMC_CTRL = 0x0000,
    MMC_PWREN = 0x0004,
    MMC_CLKDIV = 0x0008,
    MMC_CLKENA = 0x0010,
    MMC_TMOUT = 0x0014,
    MMC_CTYPE = 0x0018,
    MMC_BLKSIZ = 0x001c,
    MMC_BYTCNT = 0x0020,
    MMC_INTMASK = 0x0024,
    MMC_CMDARG = 0x0028,
    MMC_CMD = 0x002C,
    MMC_RESP0 = 0x0030,
    MMC_RESP1 = 0x0034,
    MMC_RESP2 = 0x0038,
    MMC_RESP3 = 0x003C,
    MMC_MINTSTS = 0x0040,
    MMC_RINTSTS = 0x0044,
    MMC_STATUS = 0x0048,
    MMC_FIFOTH = 0x004C,
    MMC_CDETECT = 0x0050,
    MMC_WRTPRT = 0x0054,
    MMC_GPIO = 0x0058,
    MMC_TCBCNT = 0x005C,
    MMC_TBBCNT = 0x0060,
    MMC_DEBNCE = 0x0064,
    MMC_UHS_REG = 0x0074,
    MMC_CARD_RSTN = 0x0078,
    MMC_BMOD = 0x0080,
    MMC_DBADDR = 0x0088,
    MMC_IDSTS = 0x008C,
    MMC_IDINTEN = 0x0090,
    MMC_DSCADDR = 0x0094,
    MMC_BUFADDR = 0x0098,
    MMC_CARDTHRCTL = 0x0100,
    MMC_UHS_REG_EXT = 0x0108,
    MMC_EMMC_DDR_REG = 0x010c,
    MMC_ENABLE_SHIFT = 0x0110,
    MMC_TUNING_CTRL = 0x0118,
    MMC_DATA = 0x0200
};

/*
 * MMC_CTRL(0x0000) details.
 * [25]Whether to use the built-in DMA to transfer data.
 * 0: The CPU uses the device interface to transfer data. 1: The internal DMA is used to transfer data.
 * [4]Global interrupt enable. 0: disabled; 1: enabled.
 * The interrupt output is valid only when this bit is valid and an interrupt source is enabled.
 * [2]Soft reset control for the internal DMAC. 0: invalid; 1: Reset the internal DMA interface.
 * This bit is automatically reset after two AHB clock cycles.
 * [1]Soft reset control for the internal FIFO. 0: invalid; 1: Reset the FIFO pointer.
 * This bit is automatically reset after the reset operation is complete.
 * [0]Soft reset control for the controller. 0: invalid; 1: Reset the eMMC/SD/SDIO host module.
 */
#define CTRL_RESET       (1U << 0)
#define FIFO_RESET       (1U << 1)
#define DMA_RESET        (1U << 2)
#define INTR_EN          (1U << 4)
#define USE_INTERNAL_DMA (1U << 25)

/*
 * MMC_PWREN(0x0004) details.
 * [0]POWER control. 0: power off; 1: The power supply is turned on.
 */
#define POWER_ENABLE (1U << 0)

/*
 * MMC_CLKDIV(0x0008) details.
 * [7:0]Clock divider. The clock frequency division coefficient is 2 * n.
 * For example, 0 indicates no frequency division, 1 indicates frequency division by 2,
 * and ff indicates frequency division by 510.
 */
#define CLK_DIVIDER    (0xff * 2)
#define MAX_CLKDIV_VAL 0xff

/*
 * MMC_CLKENA(0x0010) details.
 * [16]Low-power control of the card, used to disable the card clock. 0: no low-power mode; 1: low-power mode.
 * When the card is in the idle state, the card clock is stopped. This function applies only to the SD card and eMMC.
 * For the SDIO, the clock cannot be stopped to detect interrupts.
 * [0]Card clock enable. 0: disabled; 1: enabled.
 */
#define CCLK_LOW_POWER (1U << 16)
#define CCLK_ENABLE    (1U << 0)

/*
 * MMC_TMOUT(0x14) details.
 * [31:8]data read timeout param.
 * [7:0]response timeout param.
 */
#define DATA_TIMEOUT     (0xffffffU << 8)
#define RESPONSE_TIMEOUT 0xff

/*
 * MCI_CTYPE(0x0018) details.
 * [16]Bus width of the card. 0: non-8-bit mode, depending on the configuration of bit[0];
 * 1: 8-bit mode, the value of bit[0] is ignored.
 * [0]Bus width of the card. 0: 1-bit mode; 1: 4-bit mode.
 */
#define CARD_WIDTH_1  (1U << 0)
#define CARD_WIDTH_0  (1U << 16)

/* MCI_INTMASK(0x24) details.
 * [16:0]mask MMC host controller each interrupt. 0: disable; 1: enabled.
 * [16]SDIO interrupt; [3]data transfer over(DTO).
 */
#define ALL_INT_MASK   0x1ffff
#define DTO_INT_MASK   (1 << 3)
#define SDIO_INT_MASK  (1 << 16)

/*
 * MCI_CMD(0x2c) details:
 * [31]cmd execute or load start param of interface clk bit.
 */
#define START_CMD (1U << 31)

/*
 * MCI_INTSTS(0x44) details.
 * [16]sdio interrupt status; [15]end-bit error (read)/write no CRC interrupt status;
 * [14]auto command done interrupt status; [13]start bit error interrupt status;
 * [12]hardware locked write error interrupt status; [11]FIFO underrun/overrun error interrupt status;
 * [10]data starvation-by-host timeout/volt_switch to 1.8v for sdxc interrupt status;
 * [9]data read timeout interrupt status; [8]response timeout interrupt status; [7]data CRC error interrupt status;
 * [6]response CRC error interrupt status; [5]receive FIFO data request interrupt status;
 * [4]transmit FIFO data request interrupt status; [3]data transfer Over interrupt status;
 * [2]command done interrupt status; [1]response error interrupt status; [0]card detect interrupt status.
 */
#define SDIO_INT_STATUS        (1U << 16)
#define EBE_INT_STATUS         (1U << 15)
#define ACD_INT_STATUS         (1U << 14)
#define SBE_INT_STATUS         (1U << 13)
#define HLE_INT_STATUS         (1U << 12)
#define FRUN_INT_STATUS        (1U << 11)
#define HTO_INT_STATUS         (1U << 10)
#define VOLT_SWITCH_INT_STATUS (1U << 10)
#define DRTO_INT_STATUS        (1U << 9)
#define RTO_INT_STATUS         (1U << 8)
#define DCRC_INT_STATUS        (1U << 7)
#define RCRC_INT_STATUS        (1U << 6)
#define RXDR_INT_STATUS        (1U << 5)
#define TXDR_INT_STATUS        (1U << 4)
#define DTO_INT_STATUS         (1U << 3)
#define CD_INT_STATUS          (1U << 2)
#define RE_INT_STATUS          (1U << 1)
#define CARD_DETECT_INT_STATUS (1U << 0)
#define DATA_INT_MASK (DTO_INT_STATUS | DCRC_INT_STATUS | SBE_INT_STATUS | EBE_INT_STATUS)
#define CMD_INT_MASK  (RTO_INT_STATUS | RCRC_INT_STATUS | RE_INT_STATUS | CD_INT_STATUS | VOLT_SWITCH_INT_STATUS)
#define ALL_INT_CLR   0x1efff

/*
 * MMC_STATUS(0x48) details.
 * [9]Status of data_busy indicated by DAT[0]. 0: idle; 1: The card is busy.
 */
#define DATA_BUSY (1U << 9)

/* MMC_FIFOTH(0x4c) details.
 * [30:28]Indicates the transmission burst length.
 * 000: 1; 001: 4; 010: 8; 011: 16; 100: 32; 101: 64; 110: 128; 111:256.
 * [27:16]FIFO threshold watermarklevel when data is read.
 * When the FIFO count is greater than the value of this parameter, the DMA request is enabled.
 * To complete the remaining data after data transfer, a DMA request is generated.
 * [11:0]FIFO threshold watermark level when data is transmitted.
 * When the FIFO count is less than the value of this parameter, the DMA request is enabled.
 * To complete the remaining data after data transfer, a DMA request is generated.
 */
#define BURST_SIZE      (0x6 << 28)
#define RX_WMARK        (0x7f << 16)
#define TX_WMARK        0x80

/*
 * MMC_CDETECT(0x0050) details.
 * [0]Card detection signal. 0: The card is detected; 1: The card is not detected.
 */
#define CARD_UNPLUGED (1U << 0)

/*
 * MMC_WRTPRT(0x0054) details.
 * [0] 0: card read/write; 1: card readonly.
 */
#define CARD_READONLY (1U << 0)

/*
 * MMC_GPIO(0x0058) details.
 * [23] 0: dto fix bypass; 1: dto fix enable.
 */
#define DTO_FIX_ENABLE (1U << 23)

/*
 * MMC_DEBNCE(0x0064) details.
 * [23:0]Number of bus clock cycles used by the dejitter filter logic. The dejitter time is 5ms to 25ms.
 */
#define DEBNCE_MS 25
#define DEBOUNCE_E (DEBNCE_MS * 150000)
#define DEBOUNCE_H (DEBNCE_MS * 100000)
#define DEBOUNCE_M (DEBNCE_MS * 50000)
#define DEBOUNCE_L (DEBNCE_MS * 25000)

/*
 * MMC_UHS_REG(0x0074) details.
 * [16] DDR Mode control register, 0: non-DDR mode, 1: DDR mode.
 * [0] Voltage mode control register, 0: 3.3V, 1: 1.8V.
 */
#define HI_SDXC_CTRL_DDR_REG    (1U << 16)
#define HI_SDXC_CTRL_VDD_180    (1U << 0)

/*
 * MMC_CARD_RSTN(0x0078) details.
 * [16] eMMC reset controller. 0: reset; 1: reset deasserted.
 */
#define CARD_RESET (1U << 0)

/* MMC_BMOD(0x80) details.
 * [10:8]Indicates the length of the IDMAC burst transmission.
 * 000: 1; 001: 4; 010: 8; 011: 16; 100: 32; 101: 64; 110: 128; 111:256.
 * [7]IDMAC enable. 0: disabled; 1: enabled.
 * [1]Fixed burst length.
 * 0: SINGLE and INCR burst types are used; 1: SINGLE, INCR4, INCR8, and INCR16 burst types are used.
 * [0]Soft reset control for IDMAC internal registers. 0: not reset; 1: reset.
 * This bit is automatically cleared one clock cycle after this bit is set.
 *
 */
#define BMOD_SWR    (1U << 0)
#define BURST_INCR  (1U << 1)
#define BMOD_DMA_EN (1U << 7)
#define BURST_8     (1U << 8)
#define BURST_16    (3U << 8)

/* MMC_CARDTHRCTL(0x0100) details.
 * [27:16]Read threshold. The maximum value is 512.
 * [1]Busy clear interrupt enable. 0: disabled; 1: enabled.
 * [0]Read threshold enable. 0: disabled; 1: enabled.
 */
#define READ_THRESHOLD_SIZE    0x2000005
#define BUSY_CLEAR_INT_ENABLE  (1U << 1)

/* MMC_UHS_REG_EXT(0x0108) details.
 * [25:23]Clock phase of clk_in_drv, in degrees.
 * [18:16]Clock phase of clk_in_sample, in degrees.
 * 000: 0; 001: 45; 010: 90; 011: 135; 100: 180; 101: 225; 110: 270; 111: 315.
 */
#define CLK_SMPL_PHS_OFFSET   16
#define CLK_SMPL_PHS_MASK     (0x7 << CLK_SMPL_PHS_OFFSET)
#define CLK_DRV_PHS_OFFSET    23
#define CLK_DRV_PHS_MASK      (0x7 << CLK_DRV_PHS_OFFSET)
#define DRV_PHASE_180         (0x4 << 23)
#define DRV_PHASE_135         (0x3 << 23)
#define DRV_PHASE_90          (0x2 << 23)
#define SMP_PHASE_45          (0x1 << 16)
#define SMP_PHASE_0           (0x0 << 16)
#define DRV_PHASE_SHIFT       0x4
#define SMPL_PHASE_SHIFT      0x1

#define TUNING_START_PHASE    0
#define TUNING_END_PHASE      7
#define HIMCI_PHASE_SCALE     8
#define DRV_PHASE_DFLT        DRV_PHASE_180
#define SMPL_PHASE_DFLT       SMP_PHASE_0

/*
 * MMC_TUNING_CTRL(0x118) details.
 */
#define HW_TUNING_EN    (1U << 0)
#define EDGE_CTRL       (1U << 1)
#define FOUND_EDGE      (1U << 5)

/* IDMAC DEST0 details */
#define DMA_DES_OWN         (1U << 31)
#define DMA_DES_NEXT_DES    (1U << 4)
#define DMA_DES_FIRST_DES   (1U << 3)
#define DMA_DES_LAST_DES    (1U << 2)

/* MMC_CMD(0x002C) register bits define. */
union HimciCmdRegArg {
    uint32_t arg;
    struct CmdBits {
        uint32_t cmdIndex : 6;   /* [5:0]Command sequence number. */
        uint32_t rspExpect : 1;  /*
                                  * Indicates whether a response exists.
                                  * 0: No response is output from the card.
                                  * 1: A response is output from the card.
                                  */
        uint32_t rspLen : 1;     /*
                                  * Response length. 0: The short response is output from the card.
                                  * 1: The long response is output from the card.
                                  * The long response is 128 bits, and the short response is 32 bits.
                                  */
        uint32_t checkRspCrc : 1; /*
                                   * Indicates whether the CRC check is performed.
                                   * 0: The CRC response is not checked. 1: Check the CRC response.
                                   */
        uint32_t dataTransferExpected : 1; /*
                                            * Data transfer indicator.
                                            * 0: No data is output from the card. 1: Data is output from the card.
                                            */
        uint32_t readWrite : 1;  /*
                                  * Read/write control. 0: Read data from the card. 1: Write data to the card.
                                  * This bit is ignored in non-data transmission.
                                  */
        uint32_t transferMode : 1; /*
                                    * 0: block transfer command; 1: stream transmission command.
                                    * This bit is ignored in non-data transmission.
                                    */
        uint32_t sendAutoStop : 1; /*
                                    * Indicates whether to send the stop command.
                                    * 0: The stop command is not sent after the data transfer is complete.
                                    * 1: The stop command is sent after data transfer is complete.
                                    * This bit is ignored in non-data transmission.
                                    */
        uint32_t waitDataComplete : 1; /*
                                        * Indicates whether to send an instruction immediately.
                                        * 0: Send the command immediately;
                                        * 1: Send the command after the previous data transfer is complete.
                                        * 0 is a typical value, which is used to read the status or interrupt the
                                        * transfer during data transfer.
                                        */
        uint32_t stopAbortCmd : 1; /*
                                    * When the data transfer operation is in progress, the values are as follows:
                                    * 0: The stop/abort command is not sent.
                                    * 1: The stop/abort command is sent to stop the ongoing data transfer.
                                    */
        uint32_t sendInitialization : 1; /*
                                          * Indicates whether to send the initial sequence.
                                          * 0: The initial sequence is not sent before the Send_initialization is sent.
                                          * 1: The initial sequence is sent before the Send_initialization is sent.
                                          * When the card is powered on, the initial sequence must be sent for
                                          * initialization before any command is sent. That is, this bit is set to 1.
                                          */
        uint32_t cardNumber : 5;  /* Sequence number of the card in use. */
        uint32_t updateClkRegOnly : 1; /*
                                        * Indicates whether to automatically update.
                                        * 0: normal command sequence; 1: No command is sent. Only the clock register
                                        * value of the card clock domain is updated.
                                        * Set this bit to 1 each time the card clock is changed. In this case,
                                        * no command is transmitted to the card,
                                        * and no command-done interrupt is generated.
                                        */
        uint32_t reserved1 : 2;
        uint32_t enableBoot : 1; /*
                                  * Enable the boot function. This bit can be used only in forcible boot mode.
                                  * When software enables this bit and Start_cmd at the same time,
                                  * the controller pulls down the CMD signal to start the boot process.
                                  * Enable_boot and Disable_boot cannot be enabled at the same time.
                                  */
        uint32_t expectBootAck : 1; /*
                                     * Enables the boot response. When the software enables this bit and Enable_boot at
                                     * the same time, the controller detects the boot response signal,
                                     * that is, the 0-1-0 sequence.
                                     */
        uint32_t disableBoot : 1; /*
                                   * Disable the boot. When the software enables this bit and Start_cmd at the same
                                   * time, the controller stops the boot operation.
                                   * Enable_boot and Disable_boot cannot be enabled at the same time.
                                   */
        uint32_t bootMode : 1;    /* Boot mode. 0: forcible boot mode; 1: alternate boot mode. */
        uint32_t voltSwitch : 1;  /* Voltage switching control. 0: The voltage switching is disabled. 1: enabled. */
        uint32_t useHoldReg : 1;  /*
                                   * 0: The CMD and DATA signals sent to the card do not pass through the HOLD register.
                                   * 1: The CMD and DATA signals sent to the card pass through the HOLD register.
                                   */
        uint32_t reserved2 : 1;
        uint32_t startCmd : 1;    /*
                                   * Start control. 0: not enabled; 1: start command.
                                   * This bit is cleared when the command has been sent to the CIU.
                                   * The CPU cannot modify this register.
                                   * If the value is changed, a hardware lock error interrupt is generated.
                                   * After sending a command, the CPU needs to query this bit.
                                   * After the bit becomes 0, the CPU sends the next command.
                                   */
    } bits;
};

struct HimciDes {
    unsigned long dmaDesCtrl;
    unsigned long dmaDesBufSize;
    unsigned long dmaDesBufAddr;
    unsigned long dmaDesNextAddr;
};

#define HIMCI_PEND_DTO_M     (1U << 0)
#define HIMCI_PEND_ACCIDENT  (1U << 1)
#define HIMCI_HOST_INIT_DONE (1U << 2)
struct HimciHost {
    struct MmcCntlr *mmc;
    struct MmcCmd *cmd;
    void *base;
    enum HimciPowerStatus powerStatus;
    uint8_t *alignedBuff;
    uint32_t buffLen;
    struct scatterlist dmaSg;
    struct scatterlist *sg;
    uint32_t dmaSgNum;
    DMA_ADDR_T dmaPaddr;
    uint32_t *dmaVaddr;
    uint32_t irqNum;
    bool isTuning;
    uint32_t id;
    struct OsalMutex mutex;
    bool waitForEvent;
    HIMCI_EVENT himciEvent;
};

struct HimciTuneParam {
    uint32_t cmdCode;
    uint32_t edgeP2f;
    uint32_t edgeF2p;
    uint32_t startp;
    uint32_t endp;
    uint32_t endpInit;
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* HIMCI_H */
