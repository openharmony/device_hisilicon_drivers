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

#ifndef _HI_MCI_REG_H_
#define _HI_MCI_REG_H_


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define HI_MCI_IO_SIZE        0x1000

#define MCI_CTRL        0x00
#define MCI_PWREN        0x04
#define MCI_CLKDIV        0x08
#define MCI_CLKSRC        0x0C
#define MCI_CLKENA        0x10
#define MCI_TIMEOUT        0x14
#define MCI_CTYPE        0x18
#define MCI_BLKSIZ        0x1c
#define MCI_BYTCNT        0x20
#define MCI_INTMASK        0x24
#define MCI_CMDARG        0x28
#define MCI_CMD            0x2C
#define MCI_RESP0        0x30
#define MCI_RESP1        0x34
#define MCI_RESP2        0x38
#define MCI_RESP3        0x3C
#define MCI_MINTSTS        0x40
#define MCI_RINTSTS        0x44
#define MCI_STATUS        0x48
#define MCI_FIFOTH        0x4C
#define MCI_CDETECT        0x50
#define MCI_WRTPRT        0x54
#define MCI_GPIO        0x58
#define MCI_TCBCNT        0x5C
#define MCI_TBBCNT        0x60
#define MCI_DEBNCE        0x64
#define MCI_DEBNCE_MS     25

#define MCI_USRID        0x68
#define MCI_VERID        0x6C
#define MCI_HCON        0x70
#define MCI_UHS_REG        0x74
#define MCI_RESET_N        0x78
#define MCI_BMOD        0x80
#define MCI_DBADDR        0x88
#define MCI_IDSTS        0x8C
#define MCI_IDINTEN        0x90
#define MCI_DSCADDR        0x94
#define MCI_BUFADDR        0x98

#define ADMA_Q_ADDR     0xb4
#define ADMA_Q_DEEPTH       0xb8
#define ADMA_Q_RDPTR        0xbc
#define ADMA_Q_WRPTR        0xc0
#define ADMA_Q_TO       0xc4

#define MCI_CARDTHRCTL  0x100
#define MCI_BSYCLRINTEN (1U << 1)
#define MCI_UHS_EXT        0x108
#define MCI_EMMC_DDR_REG        0x10c
#define MCI_ENABLE_SHIFT                0x110
#define MCI_TUNING_CTRL     0x118

/* MCI_UHS_REG(0x74) details */
#define HI_SDXC_CTRL_VDD_180    (1U << 0)
#define HI_SDXC_CTRL_DDR_REG    (1U << 16)

/* MCI_BMOD(0x80) details */
#define BMOD_DMA_EN                (1U << 7)
#define BMOD_SWR                   (1U << 0)
#define BURST_8                    (1U << 8)
#define BURST_16                   (3U << 8)
#define BURST_INCR                 (1U << 1)

/* MCI_CTRL(0x00) details */
#define CTRL_RESET             (1U << 0)
#define FIFO_RESET             (1U << 1)
#define DMA_RESET              (1U << 2)
#define INTR_EN                (1U << 4)
#define DEV_INT_STATUS         (1U << 11)
#define USE_INTERNAL_DMA       (1U << 25)

/* MCI_CDETECT(0x50) details */
#define HIMCI_CARD0        (1)

/* MCI_TIMEOUT(0x14) details: */
/* bit 31-8: data read timeout param */
#define DATA_TIMEOUT        (0xffffffU << 8)

/* bit 7-0: response timeout param */
#define RESPONSE_TIMEOUT    0xff

/* bit 0: enable of card clk */
#define CCLK_ENABLE        (1U << 0)
#define CCLK_LOW_POWER     (1U << 16)

/* IDMAC DEST0 details */
#define DMA_DES_OWN        (1U << 31)
#define DMA_DES_NEXT_DES    (1U << 4)
#define DMA_DES_FIRST_DES    (1U << 3)
#define DMA_DES_LAST_DES    (1U << 2)


/* MCI_CTYPE(0x18) details */
#define CARD_WIDTH_1                (1U << 0)
#define CARD_WIDTH_0                (1U << 16)

#define EMMC_CARD_WIDTH_1                (2U << 0)
#define EMMC_CARD_WIDTH_0                (2U << 16)

#define CLK_SMPL_PHS_OFFSET                     (16)
#define CLK_SMPL_PHS_MASK                       (0x7 << 16)
#define CLK_DRV_PHS_OFFSET                      (23)
#define CLK_DRV_PHS_MASK                        (0x7 << 23)

#define MMC_RST_N       (1U << 0)

/* MCI_INTMASK(0x24) details:
   bit 16-1: mask MMC host controller each interrupt
*/
#define ALL_INT_MASK                0x1ffff
#define DTO_INT_MASK                (1 << 3)
#define SDIO_INT_MASK                           (1 << 16)

/* MCI_CMD(0x2c) details:
   bit 31: cmd execute or load start param of interface clk bit
*/
#define START_CMD        (1U << 31)


/* MCI_INTSTS(0x44) details */
/***************************************************************/
/* bit 16: sdio interrupt status */
#define SDIO_INT_STATUS        (1U << 16)

/* bit 15: end-bit error (read)/write no CRC interrupt status */
#define EBE_INT_STATUS        (1U << 15)

/* bit 14: auto command done interrupt status */
#define ACD_INT_STATUS        (1U << 14)

/* bit 13: start bit error interrupt status */
#define SBE_INT_STATUS        (1U << 13)

/* bit 12: hardware locked write error interrupt status */
#define HLE_INT_STATUS        (1U << 12)

/* bit 11: FIFO underrun/overrun error interrupt status */
#define FRUN_INT_STATUS        (1U << 11)

/* bit 10: data starvation-by-host timeout interrupt status */
#define HTO_INT_STATUS        (1U << 10)

/* bit 10: volt_switch to 1.8v for sdxc */
#define VOLT_SWITCH_INT_STATUS        (1U << 10)

/* bit 9: data read timeout interrupt status */
#define DRTO_INT_STATUS        (1U << 9)

/* bit 8: response timeout interrupt status */
#define RTO_INT_STATUS        (1U << 8)

/* bit 7: data CRC error interrupt status */
#define DCRC_INT_STATUS        (1U << 7)

/* bit 6: response CRC error interrupt status */
#define RCRC_INT_STATUS        (1U << 6)

/* bit 5: receive FIFO data request interrupt status */
#define RXDR_INT_STATUS        (1U << 5)

/* bit 4: transmit FIFO data request interrupt status */
#define TXDR_INT_STATUS        (1U << 4)

/* bit 3: data transfer Over interrupt status */
#define DTO_INT_STATUS        (1U << 3)

/* bit 2: command done interrupt status */
#define CD_INT_STATUS        (1U << 2)

/* bit 1: response error interrupt status */
#define RE_INT_STATUS        (1U << 1)

/* bit 0: response error interrupt status */
#define CARD_DETECT_INT_STATUS        (1)

#define DATA_INT_MASK (DTO_INT_STATUS | DCRC_INT_STATUS | SBE_INT_STATUS | EBE_INT_STATUS)
#define CMD_INT_MASK  (RTO_INT_STATUS | RCRC_INT_STATUS | RE_INT_STATUS | CD_INT_STATUS | VOLT_SWITCH_INT_STATUS)
/***************************************************************/

/* MCI_RINTSTS(0x44) details:bit 16-1: clear
   MMC host controller each interrupt but
   hardware locked write error interrupt
*/
#define ALL_INT_CLR       0x1efff

/* MCI_STATUS(0x48) details */
#define DATA_BUSY        (1U << 9)

/* MCI_TUNING_CTRL(0x118) details */
#define HW_TUNING_EN    (1U << 0)
#define EDGE_CTRL       (1U << 1)
#define FOUND_EDGE      (1U << 5)

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif
