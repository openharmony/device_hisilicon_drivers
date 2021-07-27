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

#ifndef ADC_HI35XX_H
#define ADC_HI35XX_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/*
 * Adc Register Offsets
 */

#define HI35XX_ADC_CONFIG           0x0
#define HI35XX_ADC_GLITCH_SAMPLE    0x4
#define HI35XX_ADC_TIME_SCAN        0x8
#define HI35XX_ADC_INTR_EN          0x10
#define HI35XX_ADC_INTR_STATUS      0x14
#define HI35XX_ADC_INTR_CLEAR       0x18
#define HI35XX_ADC_START            0x1c
#define HI35XX_ADC_STOP             0x20
#define HI35XX_ADC_ACCURACY         0x24
#define HI35XX_ADC_ZERO             0x28
#define HI35XX_ADC_DATA0            0x2c
#define HI35XX_ADC_DATA1            0x30

#define HI35XX_ADC_IO_CONFIG_BASE   0x111f0030
#define HI35XX_ADC_IO_CONFIG_SIZE   0x8
#define HI35XX_ADC_IO_CONFIG_0      0x0
#define HI35XX_ADC_IO_CONFIG_1      0x4

#define CYCLE_SCAN_MODE             1
#define DEFAULT_DATA_WIDTH          8
#define MAX_DATA_WIDTH              10
#define DEFAULT_GLITCHSAMPLE        5000
#define TIME_SCAN_MINIMUM            20
#define TIME_SCAN_CALCULATOR     (1000000 * 3)

#define DELTA_OFFSET                 20
#define DEGLITCH_OFFSET              17
#define SCAN_MODE_OFFSET             13
#define VALID_CHANNEL_OFFSET         8

#define DATA_WIDTH_MASK             0x3ff
#define DELTA_MASK                  0xf
#define VALID_CHANNEL_MASK          0x3
#define CONFIG_REG_RESET_VALUE      0x80ff
#define PINCTRL_MASK                0xf

enum ScanMode {
    CYCLE_MODE = 0,
    SINGLE_MODE,
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* ADC_HI35XX_H */
