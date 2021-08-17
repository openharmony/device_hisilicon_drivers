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

#ifndef I2S_CODEC_HI35XX_H
#define I2S_CODEC_HI35XX_H

#include "i2s_hi35xx.h"
#include "i2s_if.h"
#include "los_vm_zone.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define AUDIO_ANA_CTRL_0                 0x0014
#define DACL_POP_EN                      (0x1 << 18)
#define DACL_POP_EN_SHIFT                18
#define DACR_POP_EN                      (0x1 << 16)
#define DACR_POP_EN_SHIFT                16
#define MUTE_DACR                        (0x1 << 14)
#define MUTE_DACR_SHIFT                  14
#define MUTE_DACL                        (0x1 << 13)
#define MUTE_DACL_SHIFT                  13
#define PD_DACR                          (0x1 << 12)
#define PD_DACR_SHIFT                    12
#define PD_DACL                          (0x1 << 11)
#define PD_DACL_SHIFT                    11
#define PD_MICBIAS1                      (0x1 << 5)
#define PD_MICBIAS1_SHIFT                5
#define PDB_CTCM_IBIAS                   (0x1 << 1)
#define PDB_CTCM_IBIAS_SHIFT             1
#define PD_VERF                          (0x1 << 1)
#define PD_VERF_SHIFT                    1

#define AUDIO_ANA_CTRL_1              0x0018
#define RX_CTCM_PD                    (0x1 << 17)
#define RX_CTCM_PD_SHIFT              17

#define AUDIO_ANA_CTRL_2        0x001c
#define AUDIO_ANA_CTRL_2_RST                (0x1 << 23)
#define AUDIO_ANA_CTRL_2_RST_SHIFT          23
#define AUDIO_ANA_CTRL_2_VREF_SEL           (0x1f << 18)
#define AUDIO_ANA_CTRL_2_VREF_SEL_SHIFT     18
#define AUDIO_ANA_CTRL_2_ADCL_MODE_SEL           (0x1 << 7)
#define AUDIO_ANA_CTRL_2_ADCL_MODE_SEL_SHIFT     7
#define AUDIO_ANA_CTRL_2_ADCR_MODE_SEL           (0x1 << 6)
#define AUDIO_ANA_CTRL_2_ADCR_MODE_SEL_SHIFT     6
#define AUDIO_ANA_CTRL_2_LDO_PD_SEL          (0x1 << 0)
#define AUDIO_ANA_CTRL_2_LDO_PD_SEL_SHIFT    0

#define AUDIO_ANA_CTRL_3                0x0020
#define PD_ADC_TUNE_09                  (0x1 << 11)
#define PD_ADC_TUNE_09_SHIFT            11
#define POP_RAMPCLK_SEL                 (0x3 << 5)
#define POP_RAMPCLK_SEL_SHIFT           5
#define POP_RES_SEL                     (0x3 << 3)
#define POP_RES_SEL_SHIFT               3
#define DACL_POP_DIRECT                 (0x1 << 2)
#define DACL_POP_DIRECT_SHIFT           2
#define DACR_POP_DIRECT                 (0x1 << 0)
#define DACR_POP_DIRECT_SHIFT           0

#define AUDIO_ANA_CTRL_4                0x0024
#define ADC_TUNE_EN_09                  (0x1 << 11)
#define ADC_TUNE_EN_09_SHIFT            11
#define ADC_TUNE_SEL_09                 (0x1 << 10)
#define ADC_TUNE_SEL_09_SHIFT           10

#define AUDIO_ANA_CTRL_5        0x0028

#define AUDIO_CTRL_REG_1        0x0030
#define I2S1_FS_SEL             (0x1f << 13)
#define I2S1_FS_SEL_SHIFT       13
#define I2S1_DATA_BITS          (0x3 << 22)
#define I2S1_DATA_BITS_SHIFT    22
#define I2S2_DATA_BITS          (0x3 << 20)
#define I2S2_DATA_BITS_SHIFT    20

#define I2S_DATA_BITS_16   0x00
#define I2S_DATA_BITS_18   0x01
#define I2S_DATA_BITS_20   0x10
#define I2S_DATA_BITS_24   0x11


#define I2S_FS_SEL_MCLK_1024FS    0x18
#define I2S_FS_SEL_MCLK_512FS     0x19
#define I2S_FS_SEL_MCLK_256FS     0x1a
#define I2S_FS_SEL_MCLK_128FS     0x1b
#define I2S_FS_SEL_MCLK_64FS      0x1c

#define AUDIO_DAC_REG_0         0x0034
#define AUDIO_DAC_RG_1          0x0038
#define AUDIO_ADC_REG_0         0x003c

#define REG_ACODEC_REG18        0x48
#define I2S_PAD_ENABLE          (0x1 << 1)
#define I2S_PAD_ENABLE_SHIFT    1
#define AUDIO_ENABLE            (0x1 << 0)
#define AUDIO_ENABLE_SHIFT      0

#define AUDIO_ANA_CTRL_0_INIT_VAL 0x040578E1
#define AUDIO_ANA_CTRL_1_INIT_VAL 0xFD220004
#define AUDIO_ANA_CTRL_2_INIT_VAL 0x4098001b
#define AUDIO_ANA_CTRL_3_INIT_VAL 0x8383fe00
#define AUDIO_ANA_CTRL_4_INIT_VAL 0x0000505C
#define AUDIO_ANA_CTRL_5_INIT_VAL 0x0

#define AUDIO_ANA_CTRL_0_RESET_VAL 0x4000002
#define AUDIO_ANA_CTRL_1_RESET_VAL 0xfd200004
#define AUDIO_ANA_CTRL_2_RESET_VAL 0x180018
#define AUDIO_ANA_CTRL_3_RESET_VAL 0x83830028
#define AUDIO_ANA_CTRL_4_RESET_VAL 0x5c5c
#define AUDIO_ANA_CTRL_5_RESET_VAL 0x130000
#define AUDIO_CTRL_REG_1_RESET_VAL 0xff035a00
#define AUDIO_DAC_REG_0_RESET_VAL 0x8000001
#define AUDIO_DAC_RG_1_RESET_VAL 0x6062424
#define AUDIO_ADC_REG_0_RESET_VAL 0x1e1ec001


#define CODEC_DEFAULT_MSLEEP    30

void GetI2sCodecInfo(const struct I2sConfigInfo *i2sCfg);
int32_t AudioCodecSetCfgI2slFsSel(uint16_t *pI2slFsSel, enum I2slFsSel i2slFsSel);
int32_t AudioCodecGetCfgI2slFsSel(uint16_t i2slFsSel, enum I2slFsSel *pEI2slFsSel);
void Hi35xxSetAudioCodec(struct I2sConfigInfo *i2sCfg, enum I2sSampleRate sampleRate, enum I2sWordWidth width);
void CodecInnerInit(const struct I2sConfigInfo *i2sCfg);
void CodecReset(const struct I2sConfigInfo *i2sCfg);
uint32_t CodecInit(const struct I2sConfigInfo *i2sCfg);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* I2S_CODEC_HI35XX_H */
