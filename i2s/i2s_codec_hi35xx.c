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

#include "i2s_codec_hi35xx.h"
#include "hdf_base.h"
#include "hdf_log.h"
#include "osal_io.h"
#include "osal_time.h"

#define HDF_LOG_TAG i2s_codec_hi35xx
void GetI2sCodecInfo(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);
    I2S_PRINT_LOG_ERR("%s: AUDIO_ANA_CTRL_0[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_0, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_1);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ANA_CTRL_1[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_1, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ANA_CTRL_2[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_2, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ANA_CTRL_3[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_3, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ANA_CTRL_4[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_4, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_5);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ANA_CTRL_5[0x%x][0x%08x]", __func__, AUDIO_ANA_CTRL_5, value);

    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_CTRL_REG_1);
    I2S_PRINT_LOG_DBG("%s: AUDIO_CTRL_REG_1[0x%x][0x%08x]", __func__, AUDIO_CTRL_REG_1, value);

    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_DAC_REG_0);
    I2S_PRINT_LOG_DBG("%s: AUDIO_DAC_REG_0[0x%x][0x%08x]", __func__, AUDIO_DAC_REG_0, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_DAC_RG_1);
    I2S_PRINT_LOG_DBG("%s: AUDIO_DAC_RG_1[0x%x][0x%08x]", __func__, AUDIO_DAC_RG_1, value);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ADC_REG_0);
    I2S_PRINT_LOG_DBG("%s: AUDIO_ADC_REG_0[0x%x][0x%08x]", __func__, AUDIO_ADC_REG_0, value);

    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + REG_ACODEC_REG18);
    I2S_PRINT_LOG_DBG("%s: REG_ACODEC_REG18[0x%x][0x%08x]", __func__, REG_ACODEC_REG18, value);
}

uint16_t AudioCodecAdcModeSelShift(enum I2sSampleRate  sampleRate)
{
    switch (sampleRate) {
        case I2S_SAMPLE_RATE_8K:
        case I2S_SAMPLE_RATE_16K:
        case I2S_SAMPLE_RATE_32K:
            return ACODEC_ADC_MODESEL_4096;
        case I2S_SAMPLE_RATE_48K:
        case I2S_SAMPLE_RATE_96K:
        case I2S_SAMPLE_RATE_192K:
            return ACODEC_ADC_MODESEL_6144;
        default:
        {
            I2S_PRINT_LOG_ERR("%s: unsupported sampleRate [%d]", __func__, sampleRate);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    return HDF_SUCCESS;
}

uint16_t AudioCodecI2s1DataBits(enum I2sWordWidth width)
{
    switch (width) {
        case I2S_WORDWIDTH_16BIT:
            return I2S_DATA_BITS_16;
        case I2S_WORDWIDTH_18BIT:
            return I2S_DATA_BITS_18;
        case I2S_WORDWIDTH_20BIT:
            return I2S_DATA_BITS_20;
        case I2S_WORDWIDTH_24BIT:
            return I2S_DATA_BITS_24;
        default:
            {
                I2S_PRINT_LOG_ERR("%s: unsupported width [%d]", __func__, width);
                return HDF_ERR_INVALID_PARAM;
            }
    }
    return HDF_SUCCESS;
}

int32_t AudioCodecSetCfgI2slFsSel(uint16_t *pI2slFsSel, enum I2slFsSel i2slFsSel)
{
    if (pI2slFsSel == NULL) {
        I2S_PRINT_LOG_ERR("%s: pI2slFsSel NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    switch (i2slFsSel) {
        case I2SL_FS_SEL_1024_FS:
            *pI2slFsSel = I2S_FS_SEL_MCLK_1024FS;
            break;
        case I2SL_FS_SEL_512_FS:
            *pI2slFsSel = I2S_FS_SEL_MCLK_512FS;
            break;
        case I2SL_FS_SEL_256_FS:
            *pI2slFsSel = I2S_FS_SEL_MCLK_256FS;
            break;
        case I2SL_FS_SEL_128_FS:
            *pI2slFsSel = I2S_FS_SEL_MCLK_128FS;
            break;
        case I2SL_FS_SEL_64_FS:
            *pI2slFsSel = I2S_FS_SEL_MCLK_64FS;
            break;
        default:
            {
                I2S_PRINT_LOG_ERR("%s: error i2slFsSel [%d]", __func__, i2slFsSel);
                return HDF_ERR_INVALID_PARAM;
            }
    }

    I2S_PRINT_LOG_DBG("%s: *pI2slFsSel [%x]", __func__, *pI2slFsSel);
    return HDF_SUCCESS;
}

int32_t AudioCodecGetCfgI2slFsSel(uint16_t i2slFsSel, enum I2slFsSel *pEI2slFsSel)
{
    if (pEI2slFsSel == NULL) {
        I2S_PRINT_LOG_ERR("%s: pEI2slFsSel NULL", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    switch (i2slFsSel) {
        case I2S_FS_SEL_MCLK_1024FS:
            *pEI2slFsSel = I2SL_FS_SEL_1024_FS;
            break;
        case I2S_FS_SEL_MCLK_512FS:
            *pEI2slFsSel = I2SL_FS_SEL_512_FS;
            break;
        case I2S_FS_SEL_MCLK_256FS:
            *pEI2slFsSel = I2SL_FS_SEL_256_FS;
            break;
        case I2S_FS_SEL_MCLK_128FS:
            *pEI2slFsSel = I2SL_FS_SEL_128_FS;
            break;
        case I2S_FS_SEL_MCLK_64FS:
            *pEI2slFsSel = I2SL_FS_SEL_64_FS;
            break;
        default:
        {
            I2S_PRINT_LOG_ERR("%s: error i2slFsSel [%d]", __func__, i2slFsSel);
            return HDF_ERR_INVALID_PARAM;
        }
    }

    I2S_PRINT_LOG_DBG("%s: *pEI2slFsSel [%x]", __func__, *pEI2slFsSel);
    return HDF_SUCCESS;
}

void AudioCodecSetI2slFsSel(const struct I2sConfigInfo *i2sCfg, enum I2sSampleRate sampleRate)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }

    uint32_t value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_CTRL_REG_1);
    value &= ~I2S1_FS_SEL;
    value |= (i2sCfg->i2slFsSel << I2S1_FS_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_CTRL_REG_1);

    uint16_t tmp = AudioCodecAdcModeSelShift(sampleRate);
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);
    value &= ~AUDIO_ANA_CTRL_2_ADCL_MODE_SEL;
    value |= (tmp << AUDIO_ANA_CTRL_2_ADCL_MODE_SEL_SHIFT);
    value &= ~AUDIO_ANA_CTRL_2_ADCR_MODE_SEL;
    value |= (tmp << AUDIO_ANA_CTRL_2_ADCR_MODE_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    /* rctune */
    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);
    value &= ~ADC_TUNE_EN_09;
    value |= (0x0 << ADC_TUNE_EN_09_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);

    OsalUDelay(30); /* wait 30 us. */

    value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);
    value &= ~ADC_TUNE_EN_09;
    value |= (0x1 << ADC_TUNE_EN_09_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);
}

void AudioCodecSetWidth(const struct I2sConfigInfo *i2sCfg, enum I2sWordWidth width)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }

    uint16_t tmp = AudioCodecI2s1DataBits(width);
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->codecAddr + AUDIO_CTRL_REG_1);
    value &= ~I2S1_DATA_BITS;
    value |= (tmp << I2S1_DATA_BITS_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_CTRL_REG_1);
}

void AudioCodecSetAiaoChl(const struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }

    uint32_t value = Hi35xxI2sRegRead(i2sCfg->codecAddr + REG_ACODEC_REG18);
    uint16_t tmp;
    if (i2sCfg->writeChannel == I2S_WRITE_CHANNEL_AUDIO) {
        value &= ~I2S_PAD_ENABLE;
        tmp = 0x0;
        value |= (tmp << I2S_PAD_ENABLE_SHIFT);
        value &= ~AUDIO_ENABLE;
        tmp = 0x1;
        value |= (tmp << AUDIO_ENABLE_SHIFT);
    } else { // output
        value &= ~I2S_PAD_ENABLE;
        tmp = 0x1;
        value |= (tmp << I2S_PAD_ENABLE_SHIFT);
        value &= ~AUDIO_ENABLE;
        tmp = 0x0;
        value |= (tmp << AUDIO_ENABLE_SHIFT);
    }

    // audio enable value is 0x9
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + REG_ACODEC_REG18);
}

void Hi35xxSetAudioCodec(struct I2sConfigInfo *i2sCfg, enum I2sSampleRate sampleRate, enum I2sWordWidth width)
{ 
    if (i2sCfg == NULL) {
            I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }

    I2S_PRINT_LOG_DBG("%s: i2slFsSel[0x%x] sampleRate[0x%d] writeChannel[%d]", __func__, 
        i2sCfg->i2slFsSel, sampleRate, i2sCfg->writeChannel);
    i2sCfg->sampleRate = sampleRate;
    i2sCfg->width = width;
    AudioCodecSetI2slFsSel(i2sCfg, sampleRate);
    AudioCodecSetWidth(i2sCfg, width);
    AudioCodecSetAiaoChl(i2sCfg);
}

void CodecAnaCtrl0Init(const struct I2sConfigInfo *i2sCfg)
{
    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_0);
    value &= ~PD_DACL;
    value |= (0x0 << PD_DACL_SHIFT);
    value &= ~PD_DACR;
    value |= (0x0 << PD_DACR_SHIFT);
    value &= ~MUTE_DACL;
    value |= (0x0 << MUTE_DACL_SHIFT);
    value &= ~MUTE_DACR;
    value |= (0x0 << MUTE_DACR_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_0);
    value &= ~DACR_POP_EN;
    value |= (0x0 << DACR_POP_EN_SHIFT);
    value &= ~DACL_POP_EN;
    value |= (0x0 << DACL_POP_EN_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);
}


void CodecInnerInit(const struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }

    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_2);
    value &= ~AUDIO_ANA_CTRL_2_RST;
    value |= (0x0 << AUDIO_ANA_CTRL_2_RST_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_3);
    value &= ~POP_RAMPCLK_SEL;
    value |= (0x1 << POP_RAMPCLK_SEL_SHIFT);
    value &= ~POP_RES_SEL;
    value |= (0x1 << POP_RES_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_2);
    value &= ~AUDIO_ANA_CTRL_2_VREF_SEL;
    value |= (0x0 << AUDIO_ANA_CTRL_2_VREF_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_0);
    value &= ~PDB_CTCM_IBIAS;
    value |= (0x1 << PDB_CTCM_IBIAS_SHIFT);
    value &= ~PD_MICBIAS1;
    value |= (0x1 << PD_MICBIAS1_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_1);
    value &= ~RX_CTCM_PD;
    value |= (0x0 << RX_CTCM_PD_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_1);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_2);
    value &= ~AUDIO_ANA_CTRL_2_LDO_PD_SEL;
    value |= (0x0 << AUDIO_ANA_CTRL_2_LDO_PD_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_0);
    value &= ~PD_VERF;
    value |= (0x0 << PD_VERF_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_3);
    value &= ~DACL_POP_DIRECT;
    value |= (0x1 << DACL_POP_DIRECT_SHIFT);
    value &= ~DACR_POP_DIRECT;
    value |= (0x1 << DACR_POP_DIRECT_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);

    OsalMSleep(CODEC_DEFAULT_MSLEEP);

    CodecAnaCtrl0Init(i2sCfg);
}

void CodecReset(const struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s:i2sCfg null", __func__);
        return;
    }
    
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_0_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_1_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_1);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_2_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_3_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_4_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);

    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_5_RESET_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_5);

    Hi35xxI2sRegWrite(AUDIO_CTRL_REG_1_RESET_VAL, i2sCfg->codecAddr + AUDIO_CTRL_REG_1);
    Hi35xxI2sRegWrite(AUDIO_DAC_REG_0_RESET_VAL, i2sCfg->codecAddr + AUDIO_DAC_REG_0);
    Hi35xxI2sRegWrite(AUDIO_DAC_RG_1_RESET_VAL, i2sCfg->codecAddr + AUDIO_DAC_RG_1);
    Hi35xxI2sRegWrite(AUDIO_ADC_REG_0_RESET_VAL, i2sCfg->codecAddr + AUDIO_ADC_REG_0);
}

uint32_t CodecInit(const struct I2sConfigInfo *i2sCfg)
{
    if (i2sCfg == NULL) {
        I2S_PRINT_LOG_ERR("%s: i2sCfg is null", __func__);
        return HDF_FAILURE;
    }

    // init
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_0_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_0);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_1_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_1);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_2_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_3_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_4_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);
    Hi35xxI2sRegWrite(AUDIO_ANA_CTRL_5_INIT_VAL, i2sCfg->codecAddr + AUDIO_ANA_CTRL_5);

    CodecInnerInit(i2sCfg);

    uint32_t value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_2);
    value &= ~AUDIO_ANA_CTRL_2_LDO_PD_SEL;
    value |= (0x0 << AUDIO_ANA_CTRL_2_LDO_PD_SEL_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_2);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_3);
    value &= ~PD_ADC_TUNE_09;
    value |= (0x0 << PD_ADC_TUNE_09_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_3);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_4);
    value &= ~ADC_TUNE_SEL_09;
    value |= (0x1 << ADC_TUNE_SEL_09_SHIFT);

    value = Hi35xxI2sRegRead(i2sCfg->regBase + AUDIO_ANA_CTRL_4);
    value &= ~ADC_TUNE_EN_09;
    value |= (0x1 << ADC_TUNE_EN_09_SHIFT);
    Hi35xxI2sRegWrite(value, i2sCfg->codecAddr + AUDIO_ANA_CTRL_4);

    CodecReset(i2sCfg);
    return HDF_SUCCESS;
}
