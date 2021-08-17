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

#ifndef I2S_AIAO_HI35XX_H
#define I2S_AIAO_HI35XX_H

#include "i2s_if.h"
#include "i2s_hi35xx.h"
#include "los_vm_zone.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define I2S_AIAO_MAX_REG_SIZE   (64 * 1024)
#define I2S_DDR_BUFF_ALIGN_SIZE (128 * 8)
#define I2S_RX_BUFF_SIZE 0x8000
#define I2X_RX_TRANS_SIZE  0x140
#define I2S_AIAO_BUFF_SIZE  0x8000
#define I2S_TX_DATA_MIN 128
#define I2S_TX_BUFF_SIZE 0x8000
#define I2S_TX_TRANS_SIZE  0x400
#define I2S_BUFF_DATA_HEAD_1  1
#define I2S_BUFF_DATA_HEAD_2  2
#define I2S_BUFF_DATA_TAIL_3  3
#define I2S_BUFF_DATA_TAIL_2  2
#define I2S_BUFF_DATA_TAIL_1  1


#define I2S_AIAO_SAMPLE_RATE_8     8
#define I2S_AIAO_SAMPLE_RATE_16    16
#define I2S_AIAO_SAMPLE_RATE_32    32
#define I2S_AIAO_SAMPLE_RATE_48    48
#define I2S_AIAO_SAMPLE_RATE_96    96
#define I2S_AIAO_SAMPLE_RATE_192   192


#define I2S_CRG_CFG0_00         0x0100
#define AIAO_MCLK_DIV           0x152EF0

#define I2S_AIAO_INT_ENA               0x0
#define I2S_AIAO_INT_ENA_RX_CH0        (0x1 << 0)
#define I2S_AIAO_INT_ENA_RX_CH0_SHIFT  0
#define I2S_AIAO_INT_ENA_TX_CH0        (0x1 << 16)
#define I2S_AIAO_INT_ENA_TX_CH0_SHIFT  16
#define I2S_AIAO_INT_ENA_TX_CH1        (0x1 << 17)
#define I2S_AIAO_INT_ENA_TX_CH1_SHIFT  17

#define I2S_AIAO_INT_STARUS        0x0004

#define I2S_AIAO_SWITCH_RX_BCLK          0x28
#define INNER_BCLK_WS_SEL_RX_00          (0xf << 0)
#define INNER_BCLK_WS_SEL_RX_00_SHIFT     0

#define I2S_AIAO_PLL_FEQ    1188  /* 1188M */

/* I2S_CRG_CFG1_00 */
#define I2S_CRG_CFG1_00         0x0104
#define I2S_CRG_CFG1_00_VAL     0x0000c133
#define I2S_AIAO_SRST_REQ       (0x1 << 9)
#define I2S_AIAO_SRST_REQ_SHIFT 9
#define I2S_AIAO_CKEN           (0x1 << 8)
#define I2S_AIAO_CKEN_SHIFT     8
#define I2S_AIAO_FSCLK_DIV       (0x7 << 4)
#define I2S_AIAO_FSCLK_DIV_SHIFT 4
#define I2S_AIAO_BCLK_DIV        (0xF << 0)
#define I2S_AIAO_BCLK_DIV_SHIFT  0

#define I2S_AIAO_SRST_REQ_NO_RESET 0
#define I2S_AIAO_CKEN_OPEN         1

#define I2S_CRG_CFG0_08         0x0140
#define I2S_CRG_CFG1_08         0x0144


/* RX_IF_ATTR1 */
#define RX_IF_ATTR1              0x1000
#define RX_IF_ATTR1_VAL          0xE4800014
#define RX_SD_SOURCE_SEL         (0xf << 20)
#define RX_SD_SOURCE_SEL_SHIFT   20
#define RX_SD_SOURCE_SEL_NORMAL  0x8
#define RX_TRACKMODE             (0x7 << 16)
#define RX_TRACKMODE_SHIFT       16
#define RX_SD_OFFSET             (0x255 << 8)
#define RX_SD_OFFSET_SHIFT       8
#define RX_CH_NUM                (0x7 << 4)
#define RX_CH_NUM_SHIFT          4
#define RX_I2S_PRECISION         (0x3 << 2)
#define RX_I2S_PRECISION_SHIFT   2
#define RX_MODE                  (0x3 << 0)
#define RX_MODE_SHIFT            0

#define AIAO_RX_SD_OFFSET_LSB    0x0
#define AIAO_RX_SD_OFFSET_STD     0x1

#define AIAO_RX_CH_NUM_1    0x0
#define AIAO_RX_CH_NUM_2    0x1

#define AIAO_RX_I2S_PRECISION_I2S_16    0x1
#define AIAO_RX_I2S_PRECISION_I2S_24    0x2
#define AIAO_RX_I2S_PRECISION_PCM_16    0x1

#define AIAO_RX_MODE_I2S    0x0
#define AIAO_RX_MODE_PCM    0x1


/* TX_IF_ATTR1 */
#define TX_IF_ATTR1              0x2000
#define TX_CH_NUM                (0x3 << 4)
#define TX_CH_NUM_SHIFT          4
#define TX_I2S_PRECISION         (0x3 << 2)
#define TX_I2S_PRECISION_SHIFT   2
#define TX_MODE                  (0x3 << 0)
#define TX_MODE_SHIFT            0

#define RX_DSP_CTRL             0x1004
#define RX_DSP_CTRL_VAL         0x10000000
#define RX_DISABLE_DONE         (0x1 << 29)
#define RX_DISABLE_DONE_SHIFT   29
#define RX_ENABLE               (0x1 << 28)
#define RX_ENABLE_SHIFT         28
#define RX_DISABLE              (0x1 << 28)
#define RX_DISABLE_SHIFT        28

#define RX_BUFF_ASDDR           0x1080
#define RX_BUFF_ASDDR_VAL       0x00000100

#define RX_BUFF_SIZE            0x1084
#define RX_BUFF_SIZE_VAL        0x0000F000

#define RX_BUFF_WPTR            0x1088
#define RX_BUFF_WPTR_VAL        0x0

#define RX_BUFF_RPTR            0x108C
#define RX_BUFF_RPTR_VAL        0x0

#define RX_TRANS_SIZE           0x1094
#define RX_TRANS_SIZE_VAL       0x00000F00

#define RX_INT_ENA              0x10A0
#define RX_INT_ENA_VAL          0x00000001
#define RX_TRANS_INT_ENA        (0x1 << 0)
#define RX_TRANS_INT_ENA_SHIFT  0
#define RX_STOP_INT_ENA         (0x1 << 5) 
#define RX_STOP_INT_ENA_SHIFT   5

#define RX_INT_STATUS              0x10A8
#define RX_STOP_INT_STATUS         (0x1 << 5)
#define RX_STOP_INT_STATUS_SHIFT   5
#define RX_TRANS_INT_STATUS        (0x1 << 0)
#define RX_TRANS_INT_STATUS_SHIFT  0

#define RX_INT_CLR                  0x10AC
#define RX_INT_CLR_CLEAR            0x000000FF

#define TX_DSP_CTRL                 0x2004
#define TX_DISABLE_DONE             (0x1 << 29)
#define TX_DISABLE_DONE_SHIFT       29
#define TX_ENABLE                   (0x1 << 28)
#define TX_ENABLE_SHIFT             28
#define TX_DISABLE                   (0x1 << 28)
#define TX_DISABLE_SHIFT             28
#define AIAO_STOP_RX_TX_MSLEEP       10

#define TX_BUFF_SADDR               0x2080
#define TX_BUFF_SIZE                0x2084
#define TX_BUFF_WPTR                0x2088
#define TX_BUFF_RPTR                0x208C
#define TX_TRANS_SIZE               0x2094

#define TX_INT_ENA                  0x20A0
#define TX_TRANS_INT_ENA            (0x1 << 0)
#define TX_TRANS_INT_ENA_SHIFT      0
#define TX_STOP_INT_ENA             (0x1 << 5)
#define TX_STOP_INT_ENA_SHIFT      5

#define TX_INT_STATUS               0x20A8
#define TX_STOP_INT_STATUS          (0x1 << 5)
#define TX_STOP_INT_STATUS_SHIFT    5
#define TX_TRANS_INT_STATUS         (0x1 << 0)
#define TX_TRANS_INT_STATUS_SHIFT   0

#define TX_INT_CLR                  0x20AC
#define TX_INT_CLR_CLEAR            0x000000FF

#define AIAO_MCLK_48K_256FS_1188M    0x00152EF0 /* 48k * 256 */
#define AIAO_MCLK_441K_256FS_1188M   0x00137653 /* 44.1k * 256 */
#define AIAO_MCLK_32K_256FS_1188M    0x000E1F4B /* 32k * 256 */

#define AIAO_MCLK_48K_320FS_1188M    0x001A7AAC /* 48k * 320 */
#define AIAO_MCLK_441K_320FS_1188M   0x00185FA0 /* 44.1k * 320 */
#define AIAO_MCLK_32K_320FS_1188M    0x0011A71E /* 32k * 320 */

#define RX_IF_ATTR1_INIT_VAL 0xe4880014
#define TX_IF_ATTR1_INIT_VAL 0xe4000054
#define TX_DSP_CTRL_INIT_VAL 0x7900
#define RX_DSP_CTRL_INIT_VAL 0x0
#define I2S_CRG_CFG0_08_INIT_VAL 0x152ef0
#define I2S_CRG_CFG1_08_INIT_VAL 0xc115
#define I2S_CRG_CFG0_00_INIT_VAL 0x152ef0
#define I2S_CRG_CFG1_00_INIT_VAL 0xc115


void GetI2sAiaoRxInfo(const struct I2sConfigInfo *i2sCfg);
void GetI2sAiaoTxInfo(const struct I2sConfigInfo *i2sCfg);
int32_t Hi35xxSampleRateShift(enum I2sSampleRate  sampleRate);
int32_t Hi35xxSetCfgAiaoFsclkDiv(uint8_t *pAiaoFsclkDiv, uint16_t fsNum);
int32_t Hi35xxSetCfgAiaoBclkDiv(uint8_t *pAiaoBclkDiv, uint16_t bclkNum);
uint32_t AiaoGetRxIfAttri(struct I2sConfigInfo *i2sCfg, enum I2sProtocolType type, enum I2sChannelMode channelMode,
    enum I2sChannelIfMode channelIfMode, uint8_t samplePrecision);
void CfgSetI2sCrgCfg000(const struct I2sConfigInfo *i2sCfg, enum I2slFsSel i2slFsSel, enum I2sSampleRate sampleRate);
void CfgSetI2sCrgCfg100(const struct I2sConfigInfo *i2sCfg);
void CfgSetRxIfSAttr1(const struct I2sConfigInfo *i2sCfg);
void CfgSetI2sCrgCfg008(const struct I2sConfigInfo *i2sCfg, enum I2slFsSel i2slFsSel, enum I2sSampleRate sampleRate);
void CfgSetI2sCrgCfg108(const struct I2sConfigInfo *i2sCfg);
void CfgSetTxIfSAttr1(const struct I2sConfigInfo *i2sCfg);
void CfgSetTxBuffInfo(struct I2sConfigInfo *i2sCfg);
void CfgSetRxBuffInfo(struct I2sConfigInfo *i2sCfg);
void CfgStartRecord(const struct I2sConfigInfo *i2sCfg);
void CfgStartPlay(struct I2sConfigInfo *i2sCfg);
int32_t Hi35xxI2sReadGetBuff(struct I2sConfigInfo *i2sInfo);
int32_t Hi35xxI2sWriteGetBuff(struct I2sConfigInfo *i2sInfo);
int32_t GetRxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs, uint32_t *pOffset);
int32_t WriteTxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs, 
    uint32_t txWptr, uint32_t *pOffset);
int32_t UpdateTxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs, uint32_t *pOffset);
uint32_t AiaoInit(struct I2sConfigInfo *i2sCfg);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* I2S_AIAO_HI35XX_H */
