/*
 * Copyright (c) 2020-2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

#ifndef SPI_HI35XX_H
#define SPI_HI35XX_H
#include "los_vm_zone.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* ********** spi reg offset define *************** */
#define REG_SPI_CR0              0x00
#define SPI_CR0_SCR_SHIFT        8
#define SPI_CR0_SPH_SHIFT        7
#define SPI_CR0_SPO_SHIFT        6
#define SPI_CR0_FRF_SHIFT        4
#define SPI_CR0_DSS_SHIFT        0
#define SPI_CR0_SCR              (0xff << 8) /* clkout=clk/(cpsdvsr*(scr+1)) */
#define SPI_CR0_SPH              (0x1 << 7)  /* spi phase */
#define SPI_CR0_SPO              (0x1 << 6)  /* spi clk polarity */
#define SPI_CR0_FRF              (0x3 << 4)  /* frame format set */
#define SPI_CR0_DSS              (0xf << 0)  /* data bits width */

#define REG_SPI_CR1              0x04
#define SPI_CR1_WAIT_EN_SHIFT    15
#define SPI_CR1_WAIT_VAL_SHIFT   8
#define SPI_CR1_ALT_SHIFT        6
#define SPI_CR1_BIG_END_SHIFT    4
#define SPI_CR1_MS_SHIFT         2
#define SPI_CR1_SSE_SHIFT        1
#define SPI_CR1_LBN_SHIFT        0
#define SPI_CR1_WAIT_EN          (0x1 << 15)
#define SPI_CR1_WAIT_VAL         (0x7f << 8)

/* alt mode:spi enable csn is select; spi disable csn is cancel */
#define SPI_CR1_ALT              (0x1 << 6)
#define SPI_CR1_BIG_END          (0x1 << 4) /* big end or little */
#define SPI_CR1_MS               (0x1 << 2) /* cntlr-device mode */
#define SPI_CR1_SSE              (0x1 << 1) /* spi enable set */
#define SPI_CR1_LBN              (0x1 << 0) /* loopback mode */

#define REG_SPI_DR               0x08

#define REG_SPI_SR               0x0c
#define SPI_SR_BSY_SHIFT         4
#define SPI_SR_RFF_SHIFT         3
#define SPI_SR_RNE_SHIFT         2
#define SPI_SR_TNF_SHIFT         1
#define SPI_SR_TFE_SHIFT         0
#define SPI_SR_BSY               (0x1 << 4) /* spi busy flag */
#define SPI_SR_RFF               (0x1 << 3) /* Whether to send fifo is full */
#define SPI_SR_RNE               (0x1 << 2) /* Whether to send fifo is no empty */
#define SPI_SR_TNF               (0x1 << 1) /* Whether to send fifo is no full */
#define SPI_SR_TFE               (0x1 << 0) /* Whether to send fifo is empty */

#define REG_SPI_CPSR             0x10
#define SPI_CPSR_CPSDVSR_SHIFT   0
#define SPI_CPSR_CPSDVSR         (0xff << 0)  /* even 2~254 */

#define REG_SPI_IMSC             0x14
#define SPI_ALL_IRQ_DISABLE      0x0
#define SPI_ALL_IRQ_ENABLE       0x5
#define REG_SPI_RIS              0x18
#define REG_SPI_MIS              0x1c
#define SPI_RX_INTR_MASK         (0x1 << 2)

#define REG_SPI_ICR              0x20
#define SPI_ALL_IRQ_CLEAR        0x3

#define MAX_WAIT                 5000
#define DEFAULT_SPEED            2000000

#define SCR_MAX                  255
#define SCR_MIN                  0
#define CPSDVSR_MAX              254
#define CPSDVSR_MIN              2

#define SPI_CS_ACTIVE            0
#define SPI_CS_INACTIVE          1
#define TWO_BYTES                2
#define BITS_PER_WORD_MIN        4
#define BITS_PER_WORD_EIGHT      8
#define BITS_PER_WORD_MAX        16
#define HDF_IO_DEVICE_ADDR       IO_DEVICE_ADDR

#define SPI_DMA_CR               0x24
#define TX_DMA_EN_SHIFT          1
#define RX_DMA_EN_SHIFT          0            

#define SPI_TX_FIFO_CR           0x28
#define TX_INT_SIZE_SHIFT        3
#define TX_DMA_BR_SIZE_SHIFT     0
#define TX_DMA_BR_SIZE_MASK      0x7

#define SPI_RX_FIFO_CR           0x2C
#define RX_INT_SIZE_SHIFT        3
#define RX_INT_SIZE_MASK         0x7
#define RX_DMA_BR_SIZE_SHIFT     0
#define RX_DMA_BR_SIZE_MASK      0x7

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* SPI_HI35XX_H */
