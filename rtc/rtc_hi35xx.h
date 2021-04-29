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

#ifndef PLATFORM_RTC_HI35XX_H
#define PLATFORM_RTC_HI35XX_H

#include "rtc_core.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

enum RtcErrorType {
    RTC_ERROR_READ_FAIL     = 1,
    RTC_ERROR_WRITE_FAIL    = 2,
    RTC_ERROR_READ_BUSY     = 3,
    RTC_ERROR_WRITE_BUSY    = 4,
    RTC_ERROR_NULL          = 5,
};

enum RtcFeatureSupportType {
    RTC_FEATURE_NO_SUPPORT = 0,
    RTC_FEATURE_SUPPORT    = 1,
};

/* define the union SPI_RW */
union RtcSpiConfig {
    struct {
        uint32_t spiWriteData   : 8; /* [7:0] */
        uint32_t spiReadData    : 8; /* [15:8] */
        uint32_t spiAddr        : 7; /* [22:16] */
        uint32_t spiOperateType : 1; /* [23] */
        uint32_t spiStart       : 1; /* [24] */
        uint32_t reserved       : 6; /* [30:25] */
        uint32_t spiBusy        : 1; /* [31] */
    } bits;
    uint32_t data; /* define an unsigned int member */
};

struct RtcLockAddr {
    uint8_t lock0Addr;
    uint8_t lock1Addr;
    uint8_t lock2Addr;
    uint8_t lock3Addr;
};

struct RtcConfigInfo {
    uint32_t spiBaseAddr;
    volatile void *remapBaseAddr;
    uint16_t regAddrLength;
    uint8_t supportAnaCtrl;
    uint8_t supportLock;
    uint8_t irq;
    uint8_t alarmIndex;
    uint8_t anaCtrlAddr;
    struct RtcLockAddr lockAddr;
    RtcAlarmCallback cb;
    struct OsalMutex mutex;
};
struct RtcTimeReg {
    uint8_t millisecondAddr;
    uint8_t secondAddr;
    uint8_t minuteAddr;
    uint8_t hourAddr;
    uint8_t dayLowAddr;
    uint8_t dayHighAddr;
};

#define RTC_SPI_WRITE           0
#define RTC_SPI_READ            1

/* RTC control over SPI */
#define RTC_SPI_CLK_DIV(base)   ((base) + 0x000)
#define RTC_SPI_RW(base)        ((base) + 0x004)

/* RTC reg */
#define RTC_10MS_COUN           0x00
#define RTC_S_COUNT             0x01
#define RTC_M_COUNT             0x02
#define RTC_H_COUNT             0x03
#define RTC_D_COUNT_L           0x04
#define RTC_D_COUNT_H           0x05
#define RTC_MR_10MS             0x06
#define RTC_MR_S                0x07
#define RTC_MR_M                0x08
#define RTC_MR_H                0x09
#define RTC_MR_D_L              0x0A
#define RTC_MR_D_H              0x0B
#define RTC_LR_10MS             0x0C
#define RTC_LR_S                0x0D
#define RTC_LR_M                0x0E
#define RTC_LR_H                0x0F
#define RTC_LR_D_L              0x10
#define RTC_LR_D_H              0x11
#define RTC_LORD                0x12
#define RTC_MSC                 0x13
#define RTC_INT_CLR             0x14
#define RTC_INT                 0x15
#define RTC_INT_RAW             0x16
#define RTC_CLK                 0x17
#define RTC_POR_N               0x18
#define RTC_SAR_CTRL            0x1A
#define RTC_FREQ_H              0x51
#define RTC_FREQ_L              0x52

#define RTC_USER_REG1           0x53
#define RTC_USER_REG2           0x54
#define RTC_USER_REG3           0x55
#define RTC_USER_REG4           0x56
#define RTC_USER_REG5           0x57
#define RTC_USER_REG6           0x58
#define RTC_USER_REG7           0x59
#define RTC_USER_REG8           0x5A

/* RTC reg value */
#define RTC_CLK_DIV_VALUE       0X4
#define RTC_MSC_ENABLE          0x4  /* 0x4:[2] bit,irq enable */
#define RTC_UV_CTRL_ENABLE      0x20 /* 0x20:[5] bit,low-power detect */
#define RTC_ANA_CTRL_ENABLE     0x02 /* 0x20:[2],ana ctl */
#define RTC_ANA_CTRL_ORDER      0x03 /* 0x03:ana ctl order */
#define RTC_LOCK_ORDER0         0xCD /* 0xCD:ctl order */
#define RTC_LOCK_ORDER1         0xAB /* 0xAB:ctl order */
#define RTC_LOCK_ORDER2         0x5A /* 0x5A:ctl order */
#define FREQ_H_DEFAULT          0x8
#define FREQ_L_DEFAULT          0x1B
#define RTC_CLK_OUT_SEL         0x01
#define RTC_INT_CLR_MASK        0x1
#define RTC_INT_RAW_MASK        0x2
#define RTC_MSC_TIME_MASK       0x1
#define RTC_INT_MASK            0x1
#define RTC_INT_UV_MASK         0x2
#define RTC_LOCK_BYPASS_MASK    0x4
#define RTC_LOCK_MASK           0x2
#define RTC_LOAD_MASK           0x1

#define RETRY_CNT               500
#define RTC_WAIT_TIME           10
#define FREQ_MAX_VAL            3277000
#define FREQ_MIN_VAL            3276000
#define FREQ_ROUND_OFF_NUMBER   100 /* freq * 100 round-ff number */
#define REG_INDEX_MAX_VAL       8
#define SHIFT_BYTE              8
#define FREQ_DIFF               3270000
#define FREQ_COEFFICIENT        3052
#define FREQ_UNIT               10000
#define SHIFT_BYTE              8
#define MS_OF_ACCURACY          10

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* PLATFORM_RTC_HI35XX_H */
