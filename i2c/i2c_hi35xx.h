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

#ifndef I2C_HI35XX_H
#define I2C_HI35XX_H


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*
 * I2C Registers offsets
 */
#define HI35XX_I2C_GLB       0x0
#define HI35XX_I2C_SCL_H     0x4
#define HI35XX_I2C_SCL_L     0x8
#define HI35XX_I2C_DATA1     0x10
#define HI35XX_I2C_TXF       0x20
#define HI35XX_I2C_RXF       0x24
#define HI35XX_I2C_CMD_BASE  0x30
#define HI35XX_I2C_LOOP1     0xb0
#define HI35XX_I2C_DST1      0xb4
#define HI35XX_I2C_TX_WATER  0xc8
#define HI35XX_I2C_RX_WATER  0xcc
#define HI35XX_I2C_CTRL1     0xd0
#define HI35XX_I2C_CTRL2     0xd4
#define HI35XX_I2C_STAT      0xd8
#define HI35XX_I2C_INTR_RAW  0xe0
#define HI35XX_I2C_INTR_EN   0xe4
#define HI35XX_I2C_INTR_STAT 0xe8

#ifndef BIT
#define BIT(n) (1U << (n))
#endif
/*
 * I2C Global Config Register -- HI35XX_I2C_GLB
 * */
#define GLB_EN_MASK         BIT(0)
#define GLB_SDA_HOLD_MASK   0xffff00
#define GLB_SDA_HOLD_SHIFT  8

/*
 * I2C Timing CMD Register -- HI35XX_I2C_CMD_BASE + n * 4 (n = 0, 1, 2, ... 31)
 */
#define CMD_EXIT    0x0
#define CMD_TX_S    0x1
#define CMD_TX_D1_2 0x4
#define CMD_TX_D1_1 0x5
#define CMD_TX_FIFO 0x9
#define CMD_RX_FIFO 0x12
#define CMD_RX_ACK  0x13
#define CMD_IGN_ACK 0x15
#define CMD_TX_ACK  0x16
#define CMD_TX_NACK 0x17
#define CMD_JMP1    0x18
#define CMD_UP_TXF  0x1d
#define CMD_TX_RS   0x1e
#define CMD_TX_P    0x1f

/*
 * I2C Control Register 1 -- HI35XX_I2C_CTRL1
 */
#define CTRL1_CMD_START_MASK    BIT(0)
#define CTRL1_DMA_MASK          (BIT(9) | BIT(8))
#define CTRL1_DMA_R         (BIT(9) | BIT(8))
#define CTRL1_DMA_W         (BIT(9))

/*
 * I2C Status Register -- HI35XX_I2C_STAT
 */
#define STAT_RXF_NOE_MASK   BIT(16) /* RX FIFO not empty flag */
#define STAT_TXF_NOF_MASK   BIT(19) /* TX FIFO not full flag */

/*
 * I2C Interrupt status and mask Register --
 * HI35XX_I2C_INTR_RAW, HI35XX_I2C_STAT, HI35XX_I2C_INTR_STAT
 */
#define INTR_ABORT_MASK     (BIT(0) | BIT(11))
#define INTR_RX_MASK        BIT(2)
#define INTR_TX_MASK        BIT(4)
#define INTR_CMD_DONE_MASK  BIT(12)
#define INTR_USE_MASK       (INTR_ABORT_MASK \
        |INTR_RX_MASK \
        | INTR_TX_MASK \
        | INTR_CMD_DONE_MASK)
#define INTR_ALL_MASK       0xffffffff

#define I2C_DEFAULT_FREQUENCY   100000
#define I2C_TXF_DEPTH       64
#define I2C_RXF_DEPTH       64
#define I2C_TXF_WATER       32
#define I2C_RXF_WATER       32
#define I2C_WAIT_TIMEOUT    0x800
#define I2C_TIMEOUT_COUNT    0x10000
#define I2C_IRQ_TIMEOUT     (msecs_to_jiffies(1000))
/* for i2c rescue */
#define CHECK_SDA_IN_SHIFT     16
#define GPIO_MODE_SHIFT        8
#define FORCE_SCL_OEN_SHIFT    4
#define FORCE_SDA_OEN_SHIFT    0

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* I2C_HI35XX_H */
