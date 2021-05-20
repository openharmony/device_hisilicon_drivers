/*
 * Copyright (c) 2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

#ifndef DMAC_HI35XX_H
#define DMAC_HI35XX_H

#include "asm/platform.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define HIDMAC_CHANNEL_NUM             8
#define HIDMAC_ENABLE                  1

#define HIDMAC_MAX_PERIPHERALS         32
#define HIDMAC_PERI_ID_OFFSET          4
#define HIDMAC_SRC_WIDTH_OFFSET        16
#define HIDMAC_DST_WIDTH_OFFSET        12
#define HIDMAC_CH_ENABLE               1

enum HiDmacPeriphWidth {
    PERI_MODE_8BIT = 0,
    PERI_MODE_16BIT = 1,
    PERI_MODE_32BIT = 2,
    PERI_MODE_64BIT = 3,
};

/* UART ADDRESS INFO */
#define UART0_RX_ADDR                  (UART0_REG_BASE + 0x0)
#define UART0_TX_ADDR                  (UART0_REG_BASE + 0x0)
#define UART1_RX_ADDR                  (UART1_REG_BASE + 0x0)
#define UART1_TX_ADDR                  (UART1_REG_BASE + 0x0)
#define UART2_RX_ADDR                  (UART2_REG_BASE + 0x0)
#define UART2_TX_ADDR                  (UART2_REG_BASE + 0x0)

/* I2C ADDRESS INFO */
#define I2C0_TX_FIFO                   (I2C0_REG_BASE + 0x20) 
#define I2C0_RX_FIFO                   (I2C0_REG_BASE + 0x24) 
#define I2C1_TX_FIFO                   (I2C1_REG_BASE + 0x20) 
#define I2C1_RX_FIFO                   (I2C1_REG_BASE + 0x24) 
#define I2C2_TX_FIFO                   (I2C2_REG_BASE + 0x20) 
#define I2C2_RX_FIFO                   (I2C2_REG_BASE + 0x24) 

/* SPI ADDRESS INFO */
#define SPI0_RX_FIFO                   (0x120c0000 + 0x8)
#define SPI0_TX_FIFO                   (0x120c0000 + 0x8)
#define SPI1_RX_FIFO                   (0x120c1000 + 0x8)
#define SPI1_TX_FIFO                   (0x120c1000 + 0x8)
#define SPI2_RX_FIFO                   (0x120c2000 + 0x8)
#define SPI2_TX_FIFO                   (0x120c2000 + 0x8)

#define HIDMAC_PERI_CRG101_OFFSET      0x194
#define HIDMA0_AXI_OFFSET              2
#define HIDMA0_CLK_OFFSET              1
#define HIDMA0_RST_OFFSET              0
#define DDRAM_ADDR                     DDR_MEM_BASE
#define DDRAM_SIZE                     0x3FFFFFFF

#define HIDMAC_INT_STAT_OFFSET         0x00
#define HIDMAC_INT_TC1_OFFSET          0x04
#define HIDMAC_INT_TC2_OFFSET          0x08
#define HIDMAC_INT_ERR1_OFFSET         0x0C
#define HIDMAC_INT_ERR2_OFFSET         0x10
#define HIDMAC_INT_ERR3_OFFSET         0x14
#define HIDMAC_INT_TC1_MASK_OFFSET     0x18
#define HIDMAC_INT_TC2_MASK_OFFSET     0x1C
#define HIDMAC_INT_ERR1_MASK_OFFSET    0x20
#define HIDMAC_INT_ERR2_MASK_OFFSET    0x24
#define HIDMAC_INT_ERR3_MASK_OFFSET    0x28
#define HIDMAC_INT_TC1_RAW_OFFSET      0x600
#define HIDMAC_INT_TC2_RAW_OFFSET      0x608
#define HIDMAC_INT_ERR1_RAW_OFFSET     0x610
#define HIDMAC_INT_ERR2_RAW_OFFSET     0x618
#define HIDMAC_INT_ERR3_RAW_OFFSET     0x620
#define HIDMAC_CH_PRI_OFFSET           0x688
#define HIDMAC_CH_STAT_OFFSET          0x690
#define HIDMAC_CX_CUR_SRC_OFFSET(x)    (0x408 + (x) * 0x20)
#define HIDMAC_CX_CUR_DST_OFFSET(x)    (0x410 + (x) * 0x20)
#define HIDMAC_CX_LLI_OFFSET_L(x)      (0x800 + (x) * 0x40)
#define HIDMAC_CX_LLI_OFFSET_H(x)      (0x804 + (x) * 0x40)
#define HIDMAC_CX_CNT0_OFFSET(x)       (0x81C + (x) * 0x40)
#define HIDMAC_CX_SRC_OFFSET_L(x)      (0x820 + (x) * 0x40)
#define HIDMAC_CX_SRC_OFFSET_H(x)      (0x824 + (x) * 0x40)
#define HIDMAC_CX_DST_OFFSET_L(x)      (0x828 + (x) * 0x40)
#define HIDMAC_CX_DST_OFFSET_H(x)      (0x82C + (x) * 0x40)
#define HIDMAC_CX_CFG_OFFSET(x)        (0x830 + (x) * 0x40)

/* others */
#define HIDMAC_ALL_CHAN_CLR            0xFF
#define HIDMAC_INT_ENABLE_ALL_CHAN     0xFF
#define HIDMAC_CFG_SRC_INC             (1 << 31)
#define HIDMAC_CFG_DST_INC             (1 << 30)
#define HIDMAC_CFG_SRC_WIDTH_SHIFT     16
#define HIDMAC_CFG_DST_WIDTH_SHIFT     12
#define HIDMAC_WIDTH_8BIT              0x0
#define HIDMAC_WIDTH_16BIT             0x1
#define HIDMAC_WIDTH_32BIT             0x10
#define HIDMAC_WIDTH_64BIT             0x11
#define HIDMAC_BURST_WIDTH_MAX         16
#define HIDMAC_BURST_WIDTH_MIN         1
#define HIDMAC_CFG_SRC_BURST_SHIFT     24
#define HIDMAC_CFG_DST_BURST_SHIFT     20
#define HIDMAC_LLI_ALIGN               0x40
#define HIDMAC_LLI_DISABLE             0x0
#define HIDMAC_LLI_ENABLE              0x2
#define HIDMAC_CX_CFG_SIGNAL_SHIFT     0x4
#define HIDMAC_CX_CFG_MEM_TYPE         0x0
#define HIDMAC_CX_CFG_DEV_MEM_TYPE     0x1
#define HIDMAC_CX_CFG_TSF_TYPE_SHIFT   0x2
#define HIDMAC_CX_CFG_ITC_EN           0x1
#define HIDMAC_CX_CFG_ITC_EN_SHIFT     0x1
#define HIDMAC_CX_CFG_M2M              0xCFF00001
#define HIDMAC_CX_CFG_CHN_START        0x1
#define HIDMAC_CX_DISABLE              0x0
#define HIDMAC_M2M                     0x0
#define HIDMAC_NOM2M                   0x1
#define HIDMAC_TRQANS_MAX_SIZE         (64 * 1024 - 1)

struct HiDmacPeripheral {
    unsigned int periphId;             // peripheral ID
    uintptr_t periphAddr;              // peripheral data register address
    int hostSel;                       // config request
#define HIDMAC_HOST0                   0
#define HIDMAC_HOST1                   1
#define HIDMAC_NOT_USE                 (-1)
    unsigned long transCfg;            // default channel config word
    unsigned int transWidth;           // transfer data width
    unsigned int dynPeripNum;          // dynamic peripheral number
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* DMAC_HI35XX_H */
