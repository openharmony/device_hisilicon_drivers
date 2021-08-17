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

#ifndef I2S_HI35XX_H
#define I2S_HI35XX_H

#include "i2s_if.h"
#include "los_vm_zone.h"
#include "hdf_base.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef I2S_PRINTK_OPEN
#define I2S_PRINT_LOG_DBG(fmt, arg...) printk("[HDF]-[I2S]:" fmt "\r\n",  ##arg)
#define I2S_PRINT_LOG_ERR(fmt, arg...) printk("[HDF]-[I2S]:" fmt "\r\n",  ##arg)
#else
#define I2S_PRINT_LOG_DBG(fmt, arg...) HDF_LOGD_WRAPPER("[HDF]-[I2S]:" fmt "\r\n",  ##arg)
#define I2S_PRINT_LOG_ERR(fmt, arg...) HDF_LOGE_WRAPPER("[HDF]-[I2S]:" fmt "\r\n",  ##arg)
#endif

#define I2S_PRINT_DATA_LOG_DBG(fmt, arg...) HDF_LOGD_WRAPPER(fmt, ##arg)

#define I2S_AIAO_BUSNUM               0
#define I2S_AUDIO_CODEC_BUSNUM        1

#define I2S_AIAO_SAMPLE_PRECISION_24   24
#define I2S_AIAO_SAMPLE_PRECISION_16   16

#define AUDIO_CODEC_BASE_ADDR   0x113C0000
#define ACODEC_MAX_REG_SIZE     0x1000

#define CRG_BASE_ADDR           0x12010000
#define PERI_CRG103             (CRG_BASE_ADDR + 0x019c)
#define AIAO_PLL_CKEN           (0x1 << 3)
#define AIAO_PLL_CKEN_SHIFT     3
#define AIAO_CKEN               (0x1 << 1)
#define AIAO_CKEN_SHIFT         1

typedef enum {
    ACODEC_ADC_MODESEL_6144 = 0x0,
    ACODEC_ADC_MODESEL_4096 = 0x1,
    ACODEC_ADC_MODESEL_BUTT = 0xff,
} AcodecAdcModeSel;

enum I2sHi35xxAiaoFsclkDiv {
    AIAO_FSCLK_DIV_16 = 16, /* aiao_fsclk_div [6:4] 000 */
    AIAO_FSCLK_DIV_32 = 32, /* aiao_fsclk_div [6:4] 001 */
    AIAO_FSCLK_DIV_48 = 48, /* aiao_fsclk_div [6:4] 010 */
    AIAO_FSCLK_DIV_64 = 64, /* aiao_fsclk_div [6:4] 011 */
    AIAO_FSCLK_DIV_128 = 128, /* aiao_fsclk_div [6:4] 100 */
    AIAO_FSCLK_DIV_256 = 256, /* aiao_fsclk_div [6:4] 101 */
    AIAO_FSCLK_DIV_8 = 8,  /* aiao_fsclk_div [6:4] 110 */
};

enum I2sHi35xxAiaoBclkDiv {
    AIAO_BCLK_DIV_4 = 4,       /* aiao_bclk_div  [3:0] 0011 */
    AIAO_BCLK_DIV_8 = 8,       /* aiao_bclk_div  [3:0] 0101 */
};

/**< REG I2S_CFG_CFG1_00 CFG*/
struct I2sCfgCfg100 {
    uint8_t aiaoSrstReq;    /**< bit 9, aiao_srst_req RX0 channel reset */
    uint8_t aiaoCken;       /**< bit 8, aiao_cken MCLK/BCLK/WS clk gate */
    uint8_t aiaoFsclkDiv;   /**< bit [6:4] aiao_fsclk_div, fs=xxx*BCLK */
    uint8_t aiaoBclkDiv;    /**< bit [3:0], aiao_bclk_div,MCLK=xxx*BCLK */
};

/**< REG RX_IF_ATTR1 CFG*/
struct RxIfAttr1Info {
    uint32_t rxSdSourceSel;    /**< bit [23:20], rx_sd_source_sel, normal work val = 0x1000 */
    uint8_t rxTrackmode;      /**< bit [18:16], rx_trackmode, if mode=I2S, channel control */
    uint8_t rxSdOffset;       /**< bit [15:8], rx_sd_offset, 0x1 STD/ 0x0LSB */
    uint8_t rxChNum;      /**< bit [6:4], rx_ch_num, rx channel num */
    uint8_t rxI2sPrecision;   /**< bit [3:2], rx_i2s_precision, date sample precision config bit */
    uint8_t rxMode;       /**< bit [1:0], rx_mode, 00--I2S 01--PCM */
};

struct RxBuffInfo {
    uint32_t saddr;        /**< REG RX_BUFF_SADDR CFG*/
    uint32_t size;          /**< REG RX_BUFF_SIZE CFG*/
    uint32_t wptrAddr;     /**< REG RX_BUFF_WPTR CFG*/
    uint32_t rptrAddr;     /**< REG RX_BUFF_RPTR CFG*/
    uint32_t intEna;       /**< REG RX_INT_ENA CFG*/
    uint32_t dspCtrl;      /**< REG RX_DSP_CTRL CFG*/
    uint32_t transSize;   /**< REG RX_TRANS_SIZE CFG*/
};

struct I2sConfigInfo {
    uint8_t i2sPadEnable;
    uint8_t    audioEnable;
    uint32_t    PERICRG103;
    uint32_t    I2sCfgCfg000;
    uint32_t    mclk;    /**< KHZ */
    uint32_t    bclk;    /**< KHZ */
    struct I2sCfgCfg100    regCfg100;
    struct RxIfAttr1Info   regRxIfAttr1;
    volatile unsigned char *phyBase;
    volatile unsigned char *regBase;
    enum I2sWriteChannel writeChannel;
    enum I2sSampleRate sampleRate;
    enum I2sWordWidth width;
    enum I2sChannelIfMode channelIfMode;
    enum I2sChannelMode channelMode;
    enum I2sProtocolType type;
    uint8_t samplePrecision;
    uint16_t i2slFsSel;
    DMA_ADDR_T rxData;
    uint8_t *rxVirData;
    uint32_t rxWptr;
    uint32_t rxRptr;
    uint32_t rxSize;
    uint32_t rxTransSize;
    DMA_ADDR_T txData;
    uint8_t *txVirData;
    uint32_t txWptr;
    uint32_t txRptr;
    uint32_t txSize;
    uint32_t txTransSize;
    volatile unsigned char *codecAddr;
    volatile unsigned char *crg103Addr;
    bool isplay;
    bool txEn;
};

int32_t Hi35xxI2sRegWrite(uint32_t value, volatile unsigned char *addr);
uint32_t Hi35xxI2sRegRead(volatile unsigned char *addr);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif /* I2S_HI35XX_H */
