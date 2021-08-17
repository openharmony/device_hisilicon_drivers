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

#include "i2s_aiao_hi35xx.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "osal_mem.h"
#include "los_vm_iomap.h"
#include "osal_io.h"


#define HDF_LOG_TAG i2s_aiao_hi35xx
void GetI2sAiaoRxInfo(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->crg103Addr);
    I2S_PRINT_LOG_DBG("%s: PERI_CRG103[0x%px][0x%x]", __func__, i2sCfg->crg103Addr, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_AIAO_SWITCH_RX_BCLK);
    I2S_PRINT_LOG_DBG("%s: I2S_AIAO_SWITCH_RX_BCLK[0x%x][0x%08x]", __func__, I2S_AIAO_SWITCH_RX_BCLK, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG0_00);
    I2S_PRINT_LOG_DBG("%s: I2S_CRG_CFG0_00[0x%x][0x%08x]", __func__, I2S_CRG_CFG0_00, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG1_00);
    I2S_PRINT_LOG_DBG("%s: I2S_CRG_CFG1_00[0x%x][0x%08x]", __func__, I2S_CRG_CFG1_00, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_IF_ATTR1);
    I2S_PRINT_LOG_DBG("%s: RX_IF_ATTR1[0x%x][0x%08x]", __func__, RX_IF_ATTR1, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_INT_ENA);
    I2S_PRINT_LOG_DBG("%s: RX_INT_ENA[0x%x][0x%08x]", __func__, RX_INT_ENA, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_DSP_CTRL);
    I2S_PRINT_LOG_DBG("%s: RX_DSP_CTRL[0x%x][0x%08x]", __func__, RX_DSP_CTRL, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_ASDDR);
    I2S_PRINT_LOG_DBG("%s: RX_BUFF_ASDDR[0x%x][0x%08x]", __func__, RX_BUFF_ASDDR, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_SIZE);
    I2S_PRINT_LOG_DBG("%s: RX_BUFF_SIZE[0x%x][0x%08x]", __func__, RX_BUFF_SIZE, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_WPTR);
    I2S_PRINT_LOG_DBG("%s: RX_BUFF_WPTR[0x%x][0x%08x]", __func__, RX_BUFF_WPTR, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_RPTR);
    I2S_PRINT_LOG_DBG("%s: RX_BUFF_RPTR[0x%x][0x%08x]", __func__, RX_BUFF_RPTR, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_TRANS_SIZE);
    I2S_PRINT_LOG_DBG("%s: RX_TRANS_SIZE[0x%x][0x%08x]", __func__, RX_TRANS_SIZE, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_INT_STATUS);
    I2S_PRINT_LOG_DBG("%s: RX_INT_STATUS[0x%x][0x%08x]", __func__, RX_INT_STATUS, value);
}

void GetI2sAiaoTxInfo(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG0_08);
    I2S_PRINT_LOG_DBG("%s: I2S_CRG_CFG0_08[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG1_08);
    I2S_PRINT_LOG_DBG("%s: I2S_CRG_CFG1_08[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_IF_ATTR1);
    I2S_PRINT_LOG_DBG("%s: TX_IF_ATTR1[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_INT_ENA);
    I2S_PRINT_LOG_DBG("%s: TX_INT_ENA[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_DSP_CTRL);
    I2S_PRINT_LOG_DBG("%s: TX_DSP_CTRL[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_SADDR);
    I2S_PRINT_LOG_DBG("%s: TX_BUFF_SADDR[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_SIZE);
    I2S_PRINT_LOG_DBG("%s: TX_BUFF_SIZE[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_WPTR);
    I2S_PRINT_LOG_DBG("%s: TX_BUFF_WPTR[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_RPTR);
    I2S_PRINT_LOG_DBG("%s: TX_BUFF_RPTR[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_TRANS_SIZE);
    I2S_PRINT_LOG_DBG("%s: TX_TRANS_SIZE[0x%08x]", __func__, value);
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_INT_STATUS);
    I2S_PRINT_LOG_DBG("%s: TX_INT_STATUS[0x%08x]", __func__, value);
}

int32_t Hi35xxSampleRateShift(enum I2sSampleRate  sampleRate)
{
    uint32_t rate;
    switch (sampleRate) {
        case I2S_SAMPLE_RATE_8K:
            rate = I2S_AIAO_SAMPLE_RATE_8;
            break;
        case I2S_SAMPLE_RATE_16K:
            rate = I2S_AIAO_SAMPLE_RATE_16;
            break;
        case I2S_SAMPLE_RATE_32K:
            rate = I2S_AIAO_SAMPLE_RATE_32;
            break;
        case I2S_SAMPLE_RATE_48K:
            rate = I2S_AIAO_SAMPLE_RATE_48;
            break;
        case I2S_SAMPLE_RATE_96K:
            rate = I2S_AIAO_SAMPLE_RATE_96;
            break;
        case I2S_SAMPLE_RATE_192K:
            rate = I2S_AIAO_SAMPLE_RATE_192;
            break;
        default:
        {
            I2S_PRINT_LOG_ERR("%s: error sampleRate [%d]", __func__, sampleRate);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return rate;
}

int32_t Hi35xxSetCfgAiaoFsclkDiv(uint8_t *pAiaoFsclkDiv, uint16_t fsNum)
{
    if (pAiaoFsclkDiv == NULL) {
            HDF_LOGE("%s: pAiaoFsclkDiv NULL", __func__);
            return HDF_ERR_INVALID_PARAM;
        }
    switch (fsNum) {
        case AIAO_FSCLK_DIV_16:
            *pAiaoFsclkDiv = 0x0;
            break;
        case AIAO_FSCLK_DIV_32:
            *pAiaoFsclkDiv = 0x1;
            break;
        case AIAO_FSCLK_DIV_48:
            *pAiaoFsclkDiv = 0x2;
            break;
        case AIAO_FSCLK_DIV_64:
            *pAiaoFsclkDiv = 0x3;
            break;
        case AIAO_FSCLK_DIV_128:
                *pAiaoFsclkDiv = 0x4;
                break;
        case AIAO_FSCLK_DIV_256:
            *pAiaoFsclkDiv = 0x5;
            break;
        case AIAO_FSCLK_DIV_8:
            *pAiaoFsclkDiv = 0x6;
            break;
        default:
        {
            I2S_PRINT_LOG_ERR("%s: error fsNum [%d]", __func__, fsNum);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return HDF_SUCCESS;
}

int32_t Hi35xxSetCfgAiaoBclkDiv(uint8_t *pAiaoBclkDiv, uint16_t bclkNum)
{
    if (pAiaoBclkDiv == NULL) {
        HDF_LOGE("%s: pAiaoBclkDiv NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    switch (bclkNum) {
        case AIAO_BCLK_DIV_4:
            *pAiaoBclkDiv = 0x3;
            break;
        case AIAO_BCLK_DIV_8:
            *pAiaoBclkDiv = 0x5;
            break;
        default:
        {
            HDF_LOGE("%s: error bclkNum [%d]", __func__, bclkNum);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return HDF_SUCCESS;
}

uint32_t AiaoGetRxIfAttri(struct I2sConfigInfo *i2sCfg, enum I2sProtocolType type, enum I2sChannelMode channelMode, 
    enum I2sChannelIfMode channelIfMode, uint8_t samplePrecision)
{
    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input null", __func__);
        return HDF_FAILURE;
    }
    if (type == I2S_PROTOCOL_I2S_LSB) {
        i2sCfg->regRxIfAttr1.rxSdOffset = AIAO_RX_SD_OFFSET_LSB;
    } else if (type == I2S_PROTOCOL_I2S_STD) {
        i2sCfg->regRxIfAttr1.rxSdOffset = AIAO_RX_SD_OFFSET_STD;
    }  else {
        HDF_LOGE("%s: rxSdOffset set error %d", __func__, type);
        return HDF_FAILURE;
    }
    I2S_PRINT_LOG_DBG("%s: regRxIfAttr1.rxSdOffset %d", __func__, i2sCfg->regRxIfAttr1.rxSdOffset);

    i2sCfg->channelMode = channelMode;
    if (channelMode == I2S_CHANNEL_MODE_STEREO) {
        i2sCfg->regRxIfAttr1.rxChNum = AIAO_RX_CH_NUM_2;
    } else if (channelMode == I2S_CHANNEL_MODE_MONO) {
        i2sCfg->regRxIfAttr1.rxChNum = AIAO_RX_CH_NUM_1;
    } else {
        HDF_LOGE("%s: rxChNum set error %d", __func__, channelMode);
        return HDF_FAILURE;
    }
    I2S_PRINT_LOG_DBG("%s: regRxIfAttr1.rxChNum %d", __func__, i2sCfg->regRxIfAttr1.rxChNum);

    i2sCfg->channelIfMode = channelIfMode;
    i2sCfg->samplePrecision = samplePrecision;
    if (channelIfMode == I2S_CHANNEL_IF_MODE_I2S) {
        i2sCfg->regRxIfAttr1.rxMode = AIAO_RX_MODE_I2S;
        if (samplePrecision == I2S_AIAO_SAMPLE_PRECISION_16) {
            i2sCfg->regRxIfAttr1.rxI2sPrecision = AIAO_RX_I2S_PRECISION_I2S_16;
        } else if (samplePrecision == I2S_AIAO_SAMPLE_PRECISION_24) {
            i2sCfg->regRxIfAttr1.rxI2sPrecision = AIAO_RX_I2S_PRECISION_I2S_24;
        } else {
            HDF_LOGE("%s: I2S rxI2sPrecision set error %d", __func__, samplePrecision);
            return HDF_FAILURE;
        }
    } else if (channelIfMode == I2S_CHANNEL_IF_MODE_PCM) {
        i2sCfg->regRxIfAttr1.rxMode = AIAO_RX_MODE_PCM;
        if (samplePrecision == I2S_AIAO_SAMPLE_PRECISION_16) {
            i2sCfg->regRxIfAttr1.rxI2sPrecision = AIAO_RX_I2S_PRECISION_PCM_16;
        } else {
            HDF_LOGE("%s: PCM rxI2sPrecision set error %d", __func__, samplePrecision);
            return HDF_FAILURE;
        }
    }
    I2S_PRINT_LOG_DBG("%s: regRxIfAttr1.rxMode %d, rxI2sPrecision %d", __func__,
        i2sCfg->regRxIfAttr1.rxMode, i2sCfg->regRxIfAttr1.rxI2sPrecision);

    return HDF_SUCCESS;
}

uint32_t AiaogetPllFre()
{
    return I2S_AIAO_PLL_FEQ; // default 1188M
}

uint32_t AiaoCrgCfg0256Fs(enum I2sSampleRate sampleRate, uint32_t feq)
{
    if ((sampleRate == I2S_SAMPLE_RATE_48K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_48K_256FS_1188M;
    } else if ((sampleRate == I2S_SAMPLE_RATE_44_1K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_441K_256FS_1188M;
    } else if ((sampleRate == I2S_SAMPLE_RATE_32K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_32K_256FS_1188M;
    } else {
        return AIAO_MCLK_48K_256FS_1188M;
    }
}

uint32_t AiaoCrgCfg0320Fs(enum I2sSampleRate sampleRate, uint32_t feq)
{
    if ((sampleRate == I2S_SAMPLE_RATE_48K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_48K_320FS_1188M;
    } else if ((sampleRate == I2S_SAMPLE_RATE_44_1K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_441K_320FS_1188M;
    } else if ((sampleRate == I2S_SAMPLE_RATE_32K) && (feq == I2S_AIAO_PLL_FEQ)) {
        return AIAO_MCLK_32K_320FS_1188M;
    } else {
        return AIAO_MCLK_48K_256FS_1188M;
    }
}

void CfgSetI2sCrgCfg000(const struct I2sConfigInfo *i2sCfg, enum I2slFsSel i2slFsSel, enum I2sSampleRate sampleRate)
{
    uint32_t value;
    uint32_t feq = AiaogetPllFre();
    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }
    I2S_PRINT_LOG_DBG("%s:i2slFsSel[0x%d], sampleRate[0x%d], feq [0x%d]", __func__, i2slFsSel, sampleRate, feq);

    if (i2slFsSel == I2SL_FS_SEL_256_FS) {
        value = AiaoCrgCfg0256Fs(sampleRate, feq);
    } else if (i2slFsSel == I2SL_FS_SEL_320_FS) {
        value = AiaoCrgCfg0320Fs(sampleRate, feq);
    } else {
        value = AIAO_MCLK_48K_256FS_1188M;
    }

    Hi35xxI2sRegWrite(value, i2sCfg->regBase + I2S_CRG_CFG0_00);
}

void CfgSetI2sCrgCfg100(const struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: input NULL", __func__);
        return;
    }

    I2S_PRINT_LOG_DBG("%s:[0x%x][0x%x][0x%x][0x%x]", __func__,
        i2sCfg->regCfg100.aiaoSrstReq, i2sCfg->regCfg100.aiaoCken, i2sCfg->regCfg100.aiaoFsclkDiv,
        i2sCfg->regCfg100.aiaoBclkDiv);

    /**< bit 9, aiao_srst_req RX0 channel reset */
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG1_00);
    value &= ~I2S_AIAO_SRST_REQ;
    value |= (i2sCfg->regCfg100.aiaoSrstReq << I2S_AIAO_SRST_REQ_SHIFT);

    /**< bit 8, aiao_cken MCLK/BCLK/WS clk gate */
    value &= ~I2S_AIAO_CKEN;
    value |= (i2sCfg->regCfg100.aiaoCken << I2S_AIAO_CKEN_SHIFT);

    /**< bit [6:4] aiao_fsclk_div, fs=xxx*BCLK */
    value &= ~I2S_AIAO_FSCLK_DIV;
    value |= (i2sCfg->regCfg100.aiaoFsclkDiv << I2S_AIAO_FSCLK_DIV_SHIFT);

    /**< bit [3:0], aiao_bclk_div,MCLK=xxx*BCLK */
    value &= ~I2S_AIAO_BCLK_DIV;
    value |= (i2sCfg->regCfg100.aiaoBclkDiv << I2S_AIAO_BCLK_DIV_SHIFT);

    Hi35xxI2sRegWrite(value, i2sCfg->regBase + I2S_CRG_CFG1_00);
}

void CfgSetRxIfSAttr1(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;

    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }

    I2S_PRINT_LOG_DBG("%s:[0x%x][0x%x][0x%x][0x%x][0x%x]", __func__,
        i2sCfg->regRxIfAttr1.rxSdSourceSel, i2sCfg->regRxIfAttr1.rxSdOffset, i2sCfg->regRxIfAttr1.rxChNum,
        i2sCfg->regRxIfAttr1.rxI2sPrecision, i2sCfg->regRxIfAttr1.rxMode);

    /**< bit [23:20], rx_sd_source_sel, normal work val = 0x1000 */
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_IF_ATTR1);
    value &= ~RX_SD_SOURCE_SEL;
    value |= (i2sCfg->regRxIfAttr1.rxSdSourceSel << RX_SD_SOURCE_SEL_SHIFT);

    /**< bit [15:8], rx_sd_offset, 0x1 STD/ 0x0LSB */
    value &= ~RX_SD_OFFSET;
    value |= (i2sCfg->regRxIfAttr1.rxSdOffset << RX_SD_OFFSET_SHIFT);

    /**< bit [6:4], rx_ch_num, rx channel num */
    value &= ~RX_CH_NUM;
    value |= (i2sCfg->regRxIfAttr1.rxChNum << RX_CH_NUM_SHIFT);

    /**< bit [3:2], rx_i2s_precision, date sample precision config bit */
    value &= ~RX_I2S_PRECISION;
    value |= (i2sCfg->regRxIfAttr1.rxI2sPrecision << RX_I2S_PRECISION_SHIFT);

    /**< bit [1:0], recv channel mode I2S 00/PCM 01 */
    value &= ~RX_MODE;
    value |= (i2sCfg->regRxIfAttr1.rxMode << RX_MODE_SHIFT);

    Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_IF_ATTR1);
}

void CfgSetI2sCrgCfg008(const struct I2sConfigInfo *i2sCfg, enum I2slFsSel i2slFsSel, enum I2sSampleRate sampleRate)
{
    uint32_t value;
    uint32_t feq = AiaogetPllFre();
    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }
    I2S_PRINT_LOG_DBG("%s:i2slFsSel[0x%d], sampleRate[0x%d], feq [0x%d]", __func__, i2slFsSel, sampleRate, feq);

    if (i2slFsSel == I2SL_FS_SEL_256_FS) {
        value = AiaoCrgCfg0256Fs(sampleRate, feq);
    } else if (i2slFsSel == I2SL_FS_SEL_320_FS) {
        value = AiaoCrgCfg0320Fs(sampleRate, feq);
    } else {
        value = AIAO_MCLK_48K_256FS_1188M;
    }

    Hi35xxI2sRegWrite(value, i2sCfg->regBase + I2S_CRG_CFG0_08);
}

void CfgSetI2sCrgCfg108(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: input NULL", __func__);
        return;
    }

    I2S_PRINT_LOG_DBG("%s:[0x%x][0x%x][0x%x][0x%x]", __func__,
        i2sCfg->regCfg100.aiaoSrstReq, i2sCfg->regCfg100.aiaoCken, i2sCfg->regCfg100.aiaoFsclkDiv, 
        i2sCfg->regCfg100.aiaoBclkDiv);

    /**< bit 9, aiao_srst_req RX0 channel reset */
    value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_CRG_CFG1_08);
    value &= ~I2S_AIAO_SRST_REQ;
    value |= (i2sCfg->regCfg100.aiaoSrstReq << I2S_AIAO_SRST_REQ_SHIFT);

    /**< bit 8, aiao_cken MCLK/BCLK/WS clk gate */
    value &= ~I2S_AIAO_CKEN;
    value |= (i2sCfg->regCfg100.aiaoCken << I2S_AIAO_CKEN_SHIFT);

    /**< bit [6:4] aiao_fsclk_div, fs=xxx*BCLK */
    value &= ~I2S_AIAO_FSCLK_DIV;
    value |= (i2sCfg->regCfg100.aiaoFsclkDiv << I2S_AIAO_FSCLK_DIV_SHIFT);

    /**< bit [3:0], aiao_bclk_div,MCLK=xxx*BCLK */
    value &= ~I2S_AIAO_BCLK_DIV;
    value |= (i2sCfg->regCfg100.aiaoBclkDiv << I2S_AIAO_BCLK_DIV_SHIFT);
    
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + I2S_CRG_CFG1_08);
}

void CfgSetTxIfSAttr1(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: input NULL", __func__);
        return;
    }

    I2S_PRINT_LOG_DBG("%s:[0x%x][0x%x][0x%x][0x%x][0x%x]", __func__,
        i2sCfg->regRxIfAttr1.rxSdSourceSel, i2sCfg->regRxIfAttr1.rxSdOffset, i2sCfg->regRxIfAttr1.rxChNum,
        i2sCfg->regRxIfAttr1.rxI2sPrecision, i2sCfg->regRxIfAttr1.rxMode);

    /**< bit [5:4], Tx_ch_num, Tx channel num */
    value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_IF_ATTR1);
    value &= ~TX_CH_NUM;
    value |= (i2sCfg->regRxIfAttr1.rxChNum << TX_CH_NUM_SHIFT);

    /**< bit [3:2], Tx_i2s_precision, date sample precision config bit */
    value &= ~TX_I2S_PRECISION;
    value |= (i2sCfg->regRxIfAttr1.rxI2sPrecision << TX_I2S_PRECISION_SHIFT);

    /**< bit [0:0], Tx_i2s_precision, date sample precision config bit */
    value &= ~TX_MODE;
    value |= (i2sCfg->regRxIfAttr1.rxMode << TX_MODE_SHIFT);

    Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_IF_ATTR1);
}

void CfgSetTxBuffInfo(struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;

    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }

    // record cfg
    if (i2sCfg->txVirData == NULL) {
        value = 0x0;
        I2S_PRINT_LOG_DBG("%s: txData null", __func__);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_BUFF_SADDR);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_BUFF_WPTR);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_BUFF_RPTR);
        return;
    }

    Hi35xxI2sRegWrite(i2sCfg->txData, i2sCfg->regBase + TX_BUFF_SADDR);

    i2sCfg->txSize = I2S_AIAO_BUFF_SIZE;
    Hi35xxI2sRegWrite(I2S_AIAO_BUFF_SIZE, i2sCfg->regBase + TX_BUFF_SIZE);

    value = i2sCfg->txWptr;
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_BUFF_WPTR);

    value = i2sCfg->txRptr;
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_BUFF_RPTR);

    i2sCfg->txTransSize = I2X_RX_TRANS_SIZE;
    Hi35xxI2sRegWrite(i2sCfg->txTransSize, i2sCfg->regBase + TX_TRANS_SIZE);
}

void CfgSetRxBuffInfo(struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;

    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: input NULL", __func__);
        return;
    }

    if (i2sCfg->rxVirData == NULL) {
        value = 0x0;
        I2S_PRINT_LOG_DBG("%s: rxVirData null", __func__);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_BUFF_ASDDR);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_BUFF_WPTR);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_BUFF_RPTR);
        return;
    }

    Hi35xxI2sRegWrite(i2sCfg->rxData, i2sCfg->regBase + RX_BUFF_ASDDR);

    i2sCfg->rxSize = I2S_RX_BUFF_SIZE;
    Hi35xxI2sRegWrite(i2sCfg->rxSize, i2sCfg->regBase + RX_BUFF_SIZE);

    i2sCfg->rxTransSize = I2X_RX_TRANS_SIZE;
    Hi35xxI2sRegWrite(i2sCfg->rxTransSize, i2sCfg->regBase + RX_TRANS_SIZE);

    value = i2sCfg->rxWptr;
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_BUFF_WPTR);

    value = i2sCfg->rxRptr;
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_BUFF_RPTR);
}

void CfgStartRecord(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;
    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }

    // START RECORD
    value = Hi35xxI2sRegRead(i2sCfg->regBase + RX_DSP_CTRL);
    value &= ~RX_ENABLE;
    value |= (0x1 << RX_ENABLE_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + RX_DSP_CTRL);
}

void CfgStartPlay(struct I2sConfigInfo *i2sCfg)
{
    uint32_t value;
    if (i2sCfg == NULL) {
        HDF_LOGE("%s: input NULL", __func__);
        return;
    }

    if (i2sCfg->txEn == false) {
        // START PLAY
        value = Hi35xxI2sRegRead(i2sCfg->regBase + TX_DSP_CTRL);
        value &= ~TX_ENABLE;
        value |= (0x1 << TX_ENABLE_SHIFT);
        Hi35xxI2sRegWrite(value, i2sCfg->regBase + TX_DSP_CTRL);
        i2sCfg->txEn = true;
    }
}

int32_t Hi35xxI2sReadGetBuff(struct I2sConfigInfo *i2sInfo)
{
    if (i2sInfo == NULL) {
        HDF_LOGE("%s: input param is invalid", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    i2sInfo->rxVirData = LOS_DmaMemAlloc(&i2sInfo->rxData, I2S_AIAO_BUFF_SIZE, I2S_DDR_BUFF_ALIGN_SIZE, DMA_NOCACHE);
    if (i2sInfo->rxVirData == NULL) {
        HDF_LOGE("%s: LOS_DmaMemAlloc rxData error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    I2S_PRINT_LOG_DBG("%s: rxVirData:[%px]", __func__, i2sInfo->rxVirData);
    i2sInfo->rxWptr = 0x0;
    i2sInfo->rxRptr = 0x0;
    i2sInfo->rxSize = I2S_AIAO_BUFF_SIZE;

    return HDF_SUCCESS;
}

int32_t Hi35xxI2sWriteGetBuff(struct I2sConfigInfo *i2sInfo)
{
    if (i2sInfo == NULL) {
        HDF_LOGE("%s: input param is invalid", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    i2sInfo->txVirData = LOS_DmaMemAlloc(&i2sInfo->txData, I2S_AIAO_BUFF_SIZE, I2S_DDR_BUFF_ALIGN_SIZE, DMA_NOCACHE);
    if (i2sInfo->txVirData == NULL) {
        HDF_LOGE("%s: LOS_DmaMemAlloc txVirData error", __func__);
        return HDF_ERR_MALLOC_FAIL;
    }

    I2S_PRINT_LOG_DBG("%s: txVirData:[%px], txData[%lx]", __func__, i2sInfo->txVirData, i2sInfo->txData);
    i2sInfo->txWptr = 0x0;
    i2sInfo->txRptr = 0x0;
    i2sInfo->txSize = I2S_AIAO_BUFF_SIZE;
    
    return HDF_SUCCESS;
}

#define AIAO_ONETIME_TRANS_SIZE 0x1000
int32_t GetRxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs, uint32_t *pOffset)
{
    if (i2sCfg == NULL || msgs == NULL || pOffset == NULL) {
        HDF_LOGE("%s: input param is invalid", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    uint32_t ddrRxDataLen;
    uint32_t dataLen = 0;
    uint8_t *pData = NULL;
    uint32_t transSize = AIAO_ONETIME_TRANS_SIZE;
    uint32_t rxWptr = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_WPTR);
    uint32_t rxRptr = Hi35xxI2sRegRead(i2sCfg->regBase + RX_BUFF_RPTR);

    pData = i2sCfg->rxVirData + rxRptr;

    if (rxRptr <= rxWptr) {
        ddrRxDataLen = rxWptr - rxRptr;
        if (ddrRxDataLen > transSize) {
            dataLen = transSize;
            *pOffset = rxRptr + transSize;
        } else {
            I2S_PRINT_DATA_LOG_DBG("%s: not available data ddrRxDataLen[0x%x], rxRptr[0x%x], rxWptr[0x%x].",
                __func__, ddrRxDataLen, rxRptr, rxWptr);
            *pOffset = 0;
            *msgs->pRlen = 0;
            return HDF_SUCCESS;
        }
    } else {
        ddrRxDataLen = rxRptr + transSize;
        if (ddrRxDataLen < i2sCfg->rxSize) {
            dataLen = transSize;
            *pOffset = rxRptr + transSize;
        } else {
            dataLen = i2sCfg->rxSize - rxRptr;
            *pOffset = 0;
        }
    }

    int ret = LOS_CopyFromKernel((void *)msgs->rbuf, dataLen, i2sCfg->rxVirData + rxRptr, dataLen);
    if (ret != LOS_OK) {
        HDF_LOGE("%s: copy from kernel fail:%d", __func__, ret);
        return HDF_FAILURE;
    }

    *msgs->pRlen = dataLen;

    return HDF_SUCCESS;
}

int32_t WriteTxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs,
    uint32_t txWptr, uint32_t *pOffset)
{
    int ret;
    uint32_t buffFirstSize;
    uint32_t buffSecordSize;

    if (i2sCfg == NULL || msgs == NULL || pOffset == NULL) {
        HDF_LOGE("%s: input param is invalid", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((i2sCfg->txSize - txWptr) >= msgs->len) {
        *pOffset = txWptr + msgs->len;
        ret = LOS_CopyToKernel(i2sCfg->txVirData + txWptr, msgs->len, msgs->wbuf, msgs->len);
        if (ret != LOS_OK) {
            HDF_LOGE("%s: copy to kernel fail:%d", __func__, ret);
            return HDF_FAILURE;
        }
        if (*pOffset >= i2sCfg->txSize) {
            *pOffset = 0;
        }
    } else {
        buffFirstSize = i2sCfg->txSize - txWptr;
        ret = LOS_CopyToKernel(i2sCfg->txVirData + txWptr, buffFirstSize, msgs->wbuf, buffFirstSize);
        if (ret != LOS_OK) {
            HDF_LOGE("%s: copy to kernel fail:%d", __func__, ret);
            return HDF_FAILURE;
        }

        buffSecordSize = msgs->len - buffFirstSize;
        ret = LOS_CopyToKernel(i2sCfg->txVirData, buffSecordSize, msgs->wbuf + buffFirstSize, buffSecordSize);
        if (ret != LOS_OK) {
            HDF_LOGE("%s: copy to kernel fail:%d", __func__, ret);
            return HDF_FAILURE;
        }
        *pOffset = buffSecordSize;
    }

    return HDF_SUCCESS;
}

int32_t UpdateTxBuffData(struct I2sConfigInfo *i2sCfg, struct I2sMsg *msgs, uint32_t *pOffset)
{
    if (i2sCfg == NULL || msgs == NULL || msgs->wbuf == NULL || pOffset == NULL) {
        HDF_LOGE("%s: input param is invalid", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    uint32_t ddrtxDataLen;
    uint32_t txWptr = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_WPTR);
    uint32_t txRptr = Hi35xxI2sRegRead(i2sCfg->regBase + TX_BUFF_RPTR);



    if (txRptr <= txWptr) {
        ddrtxDataLen = i2sCfg->txSize - (txWptr - txRptr);
        if (ddrtxDataLen < (msgs->len + I2S_TX_DATA_MIN)) {
            I2S_PRINT_LOG_DBG("%s: tx buff full, wait to write", __func__);
            *msgs->pRlen = 0;
            *pOffset = 0;
            return HDF_SUCCESS;
        }
        if (WriteTxBuffData(i2sCfg, msgs, txWptr, pOffset) != HDF_SUCCESS) {
            I2S_PRINT_LOG_ERR("%s: WriteTxBuffData FAILED", __func__);
            return HDF_FAILURE;
        }
        *msgs->pRlen = msgs->len;
    } else {
        ddrtxDataLen = txRptr - txWptr;
        if (ddrtxDataLen < (msgs->len + I2S_TX_DATA_MIN)) {
            I2S_PRINT_LOG_DBG("%s: tx buff full, wait to write", __func__);
            *msgs->pRlen = 0;
            *pOffset = 0;
            return HDF_SUCCESS;
        }
        *pOffset = txWptr + msgs->len;

        int ret = LOS_CopyToKernel(i2sCfg->txVirData + txWptr, msgs->len, msgs->wbuf, msgs->len);
        if (ret != LOS_OK) {
            HDF_LOGE("%s: copy to kernel fail:%d", __func__, ret);
            return HDF_FAILURE;
        }
        *msgs->pRlen = msgs->len;
    }

    return HDF_SUCCESS;
}

uint32_t AiaoInit(struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: i2sCfg is null", __func__);
        return HDF_FAILURE;
    }

    Hi35xxI2sRegWrite(RX_IF_ATTR1_INIT_VAL, i2sCfg->regBase + RX_IF_ATTR1);
    Hi35xxI2sRegWrite(TX_IF_ATTR1_INIT_VAL, i2sCfg->regBase + TX_IF_ATTR1);
    Hi35xxI2sRegWrite(TX_DSP_CTRL_INIT_VAL, i2sCfg->regBase + TX_DSP_CTRL);
    Hi35xxI2sRegWrite(RX_DSP_CTRL_INIT_VAL, i2sCfg->regBase + RX_DSP_CTRL);

    Hi35xxI2sRegWrite(I2S_CRG_CFG0_08_INIT_VAL, i2sCfg->regBase + I2S_CRG_CFG0_08);
    Hi35xxI2sRegWrite(I2S_CRG_CFG1_08_INIT_VAL, i2sCfg->regBase + I2S_CRG_CFG1_08);
    Hi35xxI2sRegWrite(I2S_CRG_CFG0_00_INIT_VAL, i2sCfg->regBase + I2S_CRG_CFG0_00);
    Hi35xxI2sRegWrite(I2S_CRG_CFG1_00_INIT_VAL, i2sCfg->regBase + I2S_CRG_CFG1_00);

    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + I2S_AIAO_SWITCH_RX_BCLK);
    value &= ~INNER_BCLK_WS_SEL_RX_00;
    value |= (0x8 << INNER_BCLK_WS_SEL_RX_00_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->regBase + I2S_AIAO_SWITCH_RX_BCLK);

    return HDF_SUCCESS;
}
