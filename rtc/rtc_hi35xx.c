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

#include "device_resource_if.h"
#include "hdf_device_desc.h"
#include "hdf_log.h"
#include "osal_io.h"
#include "osal_irq.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "rtc_base.h"
#include "rtc_core.h"
#include "rtc_hi35xx.h"

#define HDF_LOG_TAG rtc_hi35xx

static uint8_t g_usrRegAddr[REG_INDEX_MAX_VAL] = {
    RTC_USER_REG1, RTC_USER_REG2, RTC_USER_REG3, RTC_USER_REG4,
    RTC_USER_REG5, RTC_USER_REG6, RTC_USER_REG7, RTC_USER_REG8,
};

static uint32_t HiSpiRead(struct RtcConfigInfo *rtcInfo, uint8_t regAdd, uint8_t *value)
{
    uint16_t cnt = RETRY_CNT;
    union RtcSpiConfig readConfig;
    union RtcSpiConfig writeConfig;

    readConfig.data = 0;
    writeConfig.data = 0;
    writeConfig.bits.spiAddr = regAdd;
    writeConfig.bits.spiOperateType = RTC_SPI_READ;
    writeConfig.bits.spiStart = RTC_TRUE;
    OSAL_WRITEL(writeConfig.data, RTC_SPI_RW((uintptr_t)rtcInfo->remapBaseAddr));

    do {
        readConfig.data = OSAL_READL(RTC_SPI_RW((uintptr_t)rtcInfo->remapBaseAddr));
        --cnt;
    } while ((readConfig.bits.spiBusy == RTC_TRUE) && (cnt != 0));

    if (readConfig.bits.spiBusy == RTC_TRUE) {
        HDF_LOGE("HiRtcSpiRead: spi busy!");
        return RTC_ERROR_READ_BUSY;
    }

    *value = readConfig.bits.spiReadData;
    return HDF_SUCCESS;
}

static uint32_t HiRtcSpiRead(struct RtcConfigInfo *rtcInfo, uint8_t regAdd, uint8_t *value)
{
    uint32_t ret;
    OsalMutexLock(&rtcInfo->mutex);
    ret = HiSpiRead(rtcInfo, regAdd, value);
    OsalMutexUnlock(&rtcInfo->mutex);
    return ret;
}

static uint32_t HiSpiWrite(struct RtcConfigInfo *rtcInfo, uint8_t regAdd, uint8_t value)
{
    uint16_t cnt = RETRY_CNT;
    union RtcSpiConfig readConfig;
    union RtcSpiConfig writeConfig;

    readConfig.data = 0;
    writeConfig.data = 0;
    writeConfig.bits.spiWriteData = value;
    writeConfig.bits.spiAddr = regAdd;
    writeConfig.bits.spiOperateType = RTC_SPI_WRITE;
    writeConfig.bits.spiStart = RTC_TRUE;
    OSAL_WRITEL(writeConfig.data, RTC_SPI_RW((uintptr_t)rtcInfo->remapBaseAddr));

    do {
        readConfig.data = OSAL_READL(RTC_SPI_RW((uintptr_t)rtcInfo->remapBaseAddr));
        --cnt;
    } while ((readConfig.bits.spiBusy == RTC_TRUE) && (cnt != 0));

    if (readConfig.bits.spiBusy == RTC_TRUE) {
        HDF_LOGE("HiRtcSpiWrite: spi busy!");
        return RTC_ERROR_WRITE_BUSY;
    }

    return HDF_SUCCESS;
}

static uint32_t HiRtcSpiWrite(struct RtcConfigInfo *rtcInfo, uint8_t regAdd, uint8_t value)
{
    uint32_t ret;
    OsalMutexLock(&rtcInfo->mutex);
    ret = HiSpiWrite(rtcInfo, regAdd, value);
    OsalMutexUnlock(&rtcInfo->mutex);
    return ret;
}

static int32_t HiRtcReadTimeData(struct RtcConfigInfo *rtcInfo, struct RtcTimeReg *regAddr, struct RtcTime *time)
{
    uint64_t seconds;
    uint32_t ret;
    uint8_t millisecond = 0;
    uint8_t dayLow;
    uint8_t dayHigh;
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint16_t day;

    if (regAddr == NULL || time == NULL) {
        HDF_LOGE("HiRtcReadTimeData: para is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    ret = HiRtcSpiRead(rtcInfo, regAddr->millisecondAddr, &millisecond);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    time->millisecond = millisecond * MS_OF_ACCURACY;
    ret = HiRtcSpiRead(rtcInfo, regAddr->secondAddr, &second);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiRead(rtcInfo, regAddr->minuteAddr, &minute);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiRead(rtcInfo, regAddr->hourAddr, &hour);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiRead(rtcInfo, regAddr->dayLowAddr, &dayLow);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiRead(rtcInfo, regAddr->dayHighAddr, &dayHigh);
    if (ret != 0) {
        return HDF_FAILURE;
    }

    day = (uint16_t)(dayLow | (dayHigh << SHIFT_BYTE)); /* 8:[15:8] for day high bit */
    seconds = (uint64_t)second + (uint64_t)minute * RTC_TIME_UNIT + (uint64_t)hour * RTC_TIME_UNIT * RTC_TIME_UNIT +
        (uint64_t)day * RTC_DAY_SECONDS;
    TimestampToRtcTime(time, seconds);

    return HDF_SUCCESS;
}

static uint32_t HiRtcReadPreviousConfig(struct RtcConfigInfo *rtcInfo)
{
    uint32_t ret;
    uint8_t value = 0;

    ret = HiRtcSpiRead(rtcInfo, RTC_INT_RAW, &value);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiRtcReadPreviousConfig: read fail!");
        return ret;
    }

    if (value & RTC_INT_RAW_MASK) {
        HDF_LOGW("low voltage detected, date/time is not reliable");
    }

    ret = HiRtcSpiRead(rtcInfo, RTC_LORD, &value);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    if (value & RTC_LOCK_BYPASS_MASK) {
        ret = HiRtcSpiWrite(rtcInfo, RTC_LORD, (~(RTC_LOCK_BYPASS_MASK)) & value);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
    }

    ret = HiRtcSpiRead(rtcInfo, RTC_LORD, &value);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    ret = HiRtcSpiWrite(rtcInfo, RTC_LORD, (value | RTC_LOCK_MASK));
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiRtcReadPreviousConfig: write fail!");
        return ret;
    }
    return HDF_SUCCESS;
}

static int32_t HiRtcReadTime(struct RtcHost *host, struct RtcTime *time)
{
    uint32_t ret;
    uint16_t cnt = RETRY_CNT;
    uint8_t value;
    struct RtcTimeReg regAddr;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiRtcReadTime: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    ret = HiRtcReadPreviousConfig(rtcInfo);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiRtcReadTime: previous config fail!");
        return HDF_FAILURE;
    }

    do {
        ret = HiRtcSpiRead(rtcInfo, RTC_LORD, &value);
        OsalMSleep(1);
        --cnt;
    } while (((ret != HDF_SUCCESS) || ((value & RTC_LOCK_MASK) == RTC_LOCK_MASK)) && (cnt != 0));

    if ((ret == HDF_SUCCESS) && ((value & RTC_LOCK_MASK) == RTC_LOCK_MASK)) {
        return HDF_ERR_DEVICE_BUSY;
    }

    regAddr.millisecondAddr = RTC_10MS_COUN;
    regAddr.secondAddr = RTC_S_COUNT;
    regAddr.minuteAddr = RTC_M_COUNT;
    regAddr.hourAddr = RTC_H_COUNT;
    regAddr.dayLowAddr = RTC_D_COUNT_L;
    regAddr.dayHighAddr = RTC_D_COUNT_H;
    return HiRtcReadTimeData(rtcInfo, &regAddr, time);
}

static int32_t HiRtcWriteTimeData(struct RtcConfigInfo *rtcInfo, struct RtcTimeReg *regAddr, const struct RtcTime *time)
{
    uint64_t seconds;
    uint32_t ret;
    uint16_t day;
    uint16_t millisecond;

    if (regAddr == NULL || time == NULL) {
        HDF_LOGE("HiRtcWriteTimeData: para is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    seconds = RtcTimeToTimestamp(time);
    day = (uint16_t)(seconds / RTC_DAY_SECONDS);
    millisecond = time->millisecond / MS_OF_ACCURACY;

    ret = HiRtcSpiWrite(rtcInfo, regAddr->millisecondAddr, millisecond);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiWrite(rtcInfo, regAddr->secondAddr, time->second);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiWrite(rtcInfo, regAddr->minuteAddr, time->minute);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiWrite(rtcInfo, regAddr->hourAddr, time->hour);
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiWrite(rtcInfo, regAddr->dayLowAddr, (day & 0xFF)); /* 0xFF:mask */
    if (ret != 0) {
        return HDF_FAILURE;
    }
    ret = HiRtcSpiWrite(rtcInfo, regAddr->dayHighAddr, (day >> SHIFT_BYTE)); /* 8:[15:8] for day high bit */
    if (ret != 0) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t HiRtcWriteTime(struct RtcHost *host, const struct RtcTime *time)
{
    uint32_t ret;
    uint16_t cnt = RETRY_CNT;
    uint8_t value = 0;
    struct RtcTimeReg regAddr;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiRtcWriteTime: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    regAddr.millisecondAddr = RTC_LR_10MS;
    regAddr.secondAddr = RTC_LR_S;
    regAddr.minuteAddr = RTC_LR_M;
    regAddr.hourAddr = RTC_LR_H;
    regAddr.dayLowAddr = RTC_LR_D_L;
    regAddr.dayHighAddr = RTC_LR_D_H;

    if (HiRtcWriteTimeData(rtcInfo, &regAddr, time) != HDF_SUCCESS) {
        HDF_LOGE("HiRtcWriteTime: write time data fail!");
        return HDF_FAILURE;
    }

    ret = HiRtcSpiWrite(rtcInfo, RTC_LORD, (value | RTC_LOAD_MASK));
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiRtcWriteTime: write fail!");
        return HDF_FAILURE;
    }

    do {
        ret = HiRtcSpiRead(rtcInfo, RTC_LORD, &value);
        OsalMSleep(RTC_WAIT_TIME);
        --cnt;
    } while (((ret != HDF_SUCCESS) || ((value & RTC_LOAD_MASK) == RTC_LOAD_MASK)) && (cnt != 0));

    OsalMSleep(RTC_WAIT_TIME);

    if ((ret == HDF_SUCCESS) && ((value & RTC_LOAD_MASK) == RTC_LOAD_MASK)) {
        HDF_LOGE("HiRtcWriteTime: fail!ret[%d], value[%d]", ret, value);
        return HDF_ERR_DEVICE_BUSY;
    }

    return HDF_SUCCESS;
}

static int32_t HiReadAlarm(struct RtcHost *host, enum RtcAlarmIndex alarmIndex, struct RtcTime *time)
{
    struct RtcConfigInfo *rtcInfo = NULL;
    struct RtcTimeReg regAddr;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiReadAlarm: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    if (alarmIndex != rtcInfo->alarmIndex) {
        HDF_LOGE("HiReadAlarm: alarmIndex para error!");
        return HDF_FAILURE;
    }

    regAddr.millisecondAddr = RTC_MR_10MS;
    regAddr.secondAddr = RTC_MR_S;
    regAddr.minuteAddr = RTC_MR_M;
    regAddr.hourAddr = RTC_MR_H;
    regAddr.dayLowAddr = RTC_MR_D_L;
    regAddr.dayHighAddr = RTC_MR_D_H;

    return HiRtcReadTimeData(rtcInfo, &regAddr, time);
}

static int32_t HiWriteAlarm(struct RtcHost *host, enum RtcAlarmIndex alarmIndex, const struct RtcTime *time)
{
    struct RtcConfigInfo *rtcInfo = NULL;
    struct RtcTimeReg regAddr;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("WriteAlarm: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    if (alarmIndex != rtcInfo->alarmIndex) {
        HDF_LOGE("WriteAlarm: alarmIndex para error!");
        return HDF_ERR_INVALID_PARAM;
    }

    regAddr.millisecondAddr = RTC_MR_10MS;
    regAddr.secondAddr = RTC_MR_S;
    regAddr.minuteAddr = RTC_MR_M;
    regAddr.hourAddr = RTC_MR_H;
    regAddr.dayLowAddr = RTC_MR_D_L;
    regAddr.dayHighAddr = RTC_MR_D_H;

    return HiRtcWriteTimeData(rtcInfo, &regAddr, time);
}

static int32_t HiRegisterAlarmCallback(struct RtcHost *host, enum RtcAlarmIndex alarmIndex, RtcAlarmCallback cb)
{
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL || cb == NULL) {
        HDF_LOGE("HiRegisterAlarmCallback: pointer is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    if (alarmIndex != rtcInfo->alarmIndex) {
        HDF_LOGE("HiRegisterAlarmCallback: alarmIndex para error!");
        return HDF_ERR_INVALID_PARAM;
    }
    rtcInfo->cb = cb;
    return HDF_SUCCESS;
}

static int32_t HiAlarmInterruptEnable(struct RtcHost *host, enum RtcAlarmIndex alarmIndex, uint8_t enable)
{
    uint32_t ret;
    uint8_t value = 0;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL || alarmIndex != RTC_ALARM_INDEX_A) {
        HDF_LOGE("HiAlarmInterruptEnable: para invalid!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    if ((enable != RTC_TRUE) && (enable != RTC_FALSE)) {
        HDF_LOGE("HiAlarmInterruptEnable: enable para error!");
        return HDF_ERR_INVALID_PARAM;
    }

    ret = HiRtcSpiRead(rtcInfo, RTC_MSC, &value);
    if (ret != HDF_SUCCESS) {
        return ret;
    }

    if (enable == RTC_TRUE) {
        ret = HiRtcSpiWrite(rtcInfo, RTC_MSC, (value | RTC_MSC_TIME_MASK));
    } else {
        ret = HiRtcSpiWrite(rtcInfo, RTC_MSC, (value & (~RTC_MSC_TIME_MASK)));
    }
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t HiGetFreq(struct RtcHost *host, uint32_t *freq)
{
    uint32_t ret;
    uint16_t value;
    uint8_t lowFreq = 0;
    uint8_t highFreq = 0;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL || freq == NULL) {
        HDF_LOGE("HiReadFreq: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    ret = HiRtcSpiRead(rtcInfo, RTC_FREQ_H, &highFreq);
    ret |= HiRtcSpiRead(rtcInfo, RTC_FREQ_L, &lowFreq);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    value = ((highFreq & 0x0f) << SHIFT_BYTE) + lowFreq; /* 8:[12:8] for freq_h bit */
    *freq = FREQ_DIFF + (value * FREQ_UNIT) / FREQ_COEFFICIENT; /* freq convert: 3270000+(freq*10000)/3052 */
    return HDF_SUCCESS;
}

static int32_t HiSetFreq(struct RtcHost *host, uint32_t freq)
{
    uint32_t ret;
    uint8_t lowFreq;
    uint8_t highFreq;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiWriteFreq: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    freq *= FREQ_ROUND_OFF_NUMBER;
    if (freq > FREQ_MAX_VAL || freq < FREQ_MIN_VAL) {
        HDF_LOGE("HiWriteFreq: para error!");
        return HDF_ERR_INVALID_PARAM;
    }

    freq = (freq - FREQ_DIFF) * FREQ_COEFFICIENT / FREQ_UNIT; /* freq convert: (freq-3270000)*3052/10000 */
    lowFreq = (freq & 0xff); /* 8:[7:0] for freq low */
    highFreq = ((freq >> SHIFT_BYTE) & 0xf); /* 8:[12:8] for freq high */

    rtcInfo = (struct RtcConfigInfo *)host->data;
    ret = HiRtcSpiWrite(rtcInfo, RTC_FREQ_H, highFreq);
    ret |= HiRtcSpiWrite(rtcInfo, RTC_FREQ_L, lowFreq);
    if (ret != HDF_SUCCESS) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t HiReset(struct RtcHost *host)
{
    uint32_t ret;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiReset: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    ret = HiRtcSpiWrite(rtcInfo, RTC_POR_N, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiReset: write por fail!");
        return HDF_FAILURE;
    }

    ret = HiRtcSpiWrite(rtcInfo, RTC_CLK, RTC_CLK_OUT_SEL);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiReset: write clk fail!");
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t HiReadReg(struct RtcHost *host, uint8_t usrDefIndex, uint8_t *value)
{
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL || value == NULL) {
        HDF_LOGE("HiReadReg: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (usrDefIndex >= REG_INDEX_MAX_VAL) {
        HDF_LOGE("HiReadReg: index error!");
        return HDF_ERR_INVALID_PARAM;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    return HiRtcSpiRead(rtcInfo, g_usrRegAddr[usrDefIndex], value);
}

static int32_t HiWriteReg(struct RtcHost *host, uint8_t usrDefIndex, uint8_t value)
{
    struct RtcConfigInfo *rtcInfo = NULL;

    if (host == NULL || host->data == NULL) {
        HDF_LOGE("HiWriteReg: host is null!");
        return HDF_ERR_INVALID_OBJECT;
    }

    if (usrDefIndex >= REG_INDEX_MAX_VAL) {
        HDF_LOGE("HiWriteReg: index error!");
        return HDF_ERR_INVALID_PARAM;
    }

    rtcInfo = (struct RtcConfigInfo *)host->data;
    return HiRtcSpiWrite(rtcInfo, g_usrRegAddr[usrDefIndex], value);
}

static struct RtcMethod g_method = {
    .ReadTime = HiRtcReadTime,
    .WriteTime = HiRtcWriteTime,
    .ReadAlarm = HiReadAlarm,
    .WriteAlarm = HiWriteAlarm,
    .RegisterAlarmCallback = HiRegisterAlarmCallback,
    .AlarmInterruptEnable = HiAlarmInterruptEnable,
    .GetFreq = HiGetFreq,
    .SetFreq = HiSetFreq,
    .Reset = HiReset,
    .ReadReg = HiReadReg,
    .WriteReg = HiWriteReg,
};

static int32_t HiRtcAttachConfigData(struct RtcConfigInfo *rtcInfo, const struct DeviceResourceNode *node)
{
    int32_t ret;
    uint32_t value;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (node == NULL || drsOps == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: node null!", __func__);
        return HDF_FAILURE;
    }

    if (rtcInfo->supportAnaCtrl == RTC_FEATURE_SUPPORT) {
        ret = drsOps->GetUint32(node, "anaCtrlAddr", &value, 0);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%s: read anaCtrlAddr fail!", __func__);
            return ret;
        }
        rtcInfo->anaCtrlAddr = (uint8_t)value;
    }

    if (rtcInfo->supportLock == RTC_FEATURE_SUPPORT) {
        ret = drsOps->GetUint32(node, "lock0Addr", &value, 0);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        rtcInfo->lockAddr.lock0Addr = (uint8_t)value;
        ret = drsOps->GetUint32(node, "lock1Addr", &value, 0);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        rtcInfo->lockAddr.lock1Addr = (uint8_t)value;
        ret = drsOps->GetUint32(node, "lock2Addr", &value, 0);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        rtcInfo->lockAddr.lock2Addr = (uint8_t)value;
        ret = drsOps->GetUint32(node, "lock3Addr", &value, 0);
        if (ret != HDF_SUCCESS) {
            return ret;
        }
        rtcInfo->lockAddr.lock3Addr = (uint8_t)value;
    }
    return HDF_SUCCESS;
}

static int32_t HiRtcConfigData(struct RtcConfigInfo *rtcInfo, const struct DeviceResourceNode *node)
{
    int32_t ret;
    uint32_t value;
    struct DeviceResourceIface *drsOps = NULL;

    drsOps = DeviceResourceGetIfaceInstance(HDF_CONFIG_SOURCE);
    if (drsOps == NULL || drsOps->GetBool == NULL || drsOps->GetUint32 == NULL) {
        HDF_LOGE("%s: invalid drs ops fail!", __func__);
        return HDF_FAILURE;
    }

    rtcInfo->supportAnaCtrl = drsOps->GetBool(node, "supportAnaCtrl");
    rtcInfo->supportLock = drsOps->GetBool(node, "supportLock");

    ret = drsOps->GetUint32(node, "rtcSpiBaseAddr", &rtcInfo->spiBaseAddr, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read regBase fail!", __func__);
        return ret;
    }

    ret = drsOps->GetUint32(node, "regAddrLength", &value, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read Length fail!", __func__);
        return ret;
    }
    rtcInfo->regAddrLength = (uint16_t)value;

    ret = drsOps->GetUint32(node, "irq", &value, 0);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read irq fail!", __func__);
        return ret;
    }
    rtcInfo->irq = (uint8_t)value;
    ret = HiRtcAttachConfigData(rtcInfo, node);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: read config data fail!", __func__);
        return ret;
    }

    rtcInfo->alarmIndex = RTC_ALARM_INDEX_A;
    rtcInfo->remapBaseAddr = NULL;
    return HDF_SUCCESS;
}

static uint32_t HiRtcIrqHandle(uint32_t irqId, void *data)
{
    uint8_t value = 0;
    uint32_t ret;
    struct RtcConfigInfo *rtcInfo = NULL;

    UNUSED(irqId);
    rtcInfo = (struct RtcConfigInfo *)data;
    if (rtcInfo == NULL) {
        return RTC_ERROR_NULL;
    }

    ret = HiSpiRead(rtcInfo, RTC_INT, &value);
    if (ret != HDF_SUCCESS) {
        return RTC_ERROR_READ_FAIL;
    }

    ret = HiSpiWrite(rtcInfo, RTC_INT_CLR, RTC_INT_CLR_MASK);
    if (ret != HDF_SUCCESS) {
        return RTC_ERROR_WRITE_FAIL;
    }
    if (rtcInfo->cb == NULL) {
        return RTC_ERROR_NULL;
    }

    if (value & RTC_INT_MASK) {
        return rtcInfo->cb(rtcInfo->alarmIndex);
    }

    if (value & RTC_INT_UV_MASK) {
        HiSpiRead(rtcInfo, RTC_MSC, &value);
        HiSpiWrite(rtcInfo, RTC_MSC, value & (~RTC_INT_UV_MASK)); /* close low voltage int */
    }
    return HDF_SUCCESS;
}

static int32_t HiRtcSwInit(struct RtcConfigInfo *rtcInfo)
{
    bool ret = false;

    if (rtcInfo->spiBaseAddr == 0 || (rtcInfo->regAddrLength == 0)) {
        HDF_LOGE("HiRtcSwInit: para invalid!");
        return HDF_ERR_INVALID_PARAM;
    }

    if (OsalMutexInit(&rtcInfo->mutex) != HDF_SUCCESS) {
        HDF_LOGE("HiRtcSwInit: create mutex fail!");
        return HDF_FAILURE;
    }

    if (rtcInfo->remapBaseAddr == NULL) {
        rtcInfo->remapBaseAddr = (volatile void *)OsalIoRemap((uintptr_t)rtcInfo->spiBaseAddr,
            rtcInfo->regAddrLength);
    }

    ret = OsalRegisterIrq(rtcInfo->irq, 0, HiRtcIrqHandle, "rtc_alarm", (void*)rtcInfo);
    if (ret != 0) {
        HDF_LOGE("HiRtcSwInit: register irq(%d) fail!", rtcInfo->irq);
        (void)OsalMutexDestroy(&rtcInfo->mutex);
        OsalIoUnmap((void*)rtcInfo->remapBaseAddr);
        rtcInfo->remapBaseAddr = NULL;
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void HiRtcSwExit(struct RtcConfigInfo *rtcInfo)
{
    (void)OsalUnregisterIrq(rtcInfo->irq, (void *)rtcInfo);
    (void)OsalMutexDestroy(&rtcInfo->mutex);

    if (rtcInfo->remapBaseAddr != NULL) {
        OsalIoUnmap((void*)rtcInfo->remapBaseAddr);
        rtcInfo->remapBaseAddr = NULL;
    }
}

static uint32_t HiRtcHwAttachInit(struct RtcConfigInfo *rtcInfo)
{
    uint32_t ret = 0;

    if (rtcInfo->supportAnaCtrl == RTC_FEATURE_SUPPORT) {
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->anaCtrlAddr, RTC_ANA_CTRL_ENABLE);
    }

    /* Unlock First, Then modify to the default driver capability */
    if (rtcInfo->supportLock == RTC_FEATURE_SUPPORT) {
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->lockAddr.lock3Addr, RTC_LOCK_ORDER2);
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->lockAddr.lock2Addr, RTC_LOCK_ORDER2);
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->lockAddr.lock1Addr, RTC_LOCK_ORDER1);
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->lockAddr.lock0Addr, RTC_LOCK_ORDER0);
        ret |= HiRtcSpiWrite(rtcInfo, rtcInfo->anaCtrlAddr, RTC_ANA_CTRL_ORDER);
    }

    if (ret != HDF_SUCCESS) {
        HDF_LOGE("HiRtcHwAttachInit: write config fail");
        return ret;
    }
    return ret;
}

static int32_t HiRtcHwInit(struct RtcConfigInfo *rtcInfo)
{
    uint32_t ret = 0;
    uint8_t value = 0;

    /* clk div value is (apb_clk/spi_clk)/2-1, for asic, apb clk(100MHz), spi_clk(10MHz), so value is 0x4 */
    OSAL_WRITEL(RTC_CLK_DIV_VALUE, RTC_SPI_CLK_DIV((uintptr_t)rtcInfo->remapBaseAddr));
    ret |= HiRtcSpiWrite(rtcInfo, RTC_MSC, RTC_MSC_ENABLE);
    ret |= HiRtcSpiWrite(rtcInfo, RTC_SAR_CTRL, RTC_UV_CTRL_ENABLE);
    ret |= HiRtcHwAttachInit(rtcInfo);
    ret |= HiRtcSpiWrite(rtcInfo, RTC_FREQ_H, FREQ_H_DEFAULT);
    ret |= HiRtcSpiWrite(rtcInfo, RTC_FREQ_L, FREQ_L_DEFAULT);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("RtcHwInit: init fail");
        return HDF_FAILURE;
    }

    if (HiRtcSpiRead(rtcInfo, RTC_INT_RAW, &value) != 0) {
        HDF_LOGW("RtcHwInit: spi read fail");
        return HDF_FAILURE;
    }

    if (value & RTC_INT_RAW_MASK) {
        HDF_LOGW("HiRtcHwInit: low voltage detected, date/time is not reliable");
    }
    return HDF_SUCCESS;
}

static int32_t HiRtcBind(struct HdfDeviceObject *device)
{
    struct RtcHost *host = NULL;

    host = RtcHostCreate(device);
    if (host == NULL) {
        HDF_LOGE("HiRtcBind: create host fail!");
        return HDF_ERR_INVALID_OBJECT;
    }

    host->device = device;
    device->service = &host->service;
    return HDF_SUCCESS;
}

static int32_t HiRtcInit(struct HdfDeviceObject *device)
{
    struct RtcHost *host = NULL;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (device == NULL || device->property == NULL) {
        return HDF_ERR_INVALID_OBJECT;
    }
    host = RtcHostFromDevice(device);
    rtcInfo = OsalMemCalloc(sizeof(*rtcInfo));
    if (rtcInfo == NULL) {
        HDF_LOGE("HiRtcInit: malloc info fail!");
        return HDF_ERR_MALLOC_FAIL;
    }

    if (HiRtcConfigData(rtcInfo, device->property) != 0) {
        HDF_LOGE("HiRtcInit: hcs config fail!");
        OsalMemFree(rtcInfo);
        return HDF_ERR_INVALID_OBJECT;
    }

    if (HiRtcSwInit(rtcInfo) != 0) {
        HDF_LOGE("HiRtcInit: sw init fail!");
        OsalMemFree(rtcInfo);
        return HDF_DEV_ERR_DEV_INIT_FAIL;
    }

    if (HiRtcHwInit(rtcInfo) != 0) {
        HDF_LOGE("HiRtcInit: hw init fail!");
        HiRtcSwExit(rtcInfo);
        OsalMemFree(rtcInfo);
        return HDF_DEV_ERR_DEV_INIT_FAIL;
    }

    host->method = &g_method;
    host->data = rtcInfo;
    HDF_LOGI("Hdf dev service:%s init success!", HdfDeviceGetServiceName(device));
    return HDF_SUCCESS;
}

static void HiRtcRelease(struct HdfDeviceObject *device)
{
    struct RtcHost *host = NULL;
    struct RtcConfigInfo *rtcInfo = NULL;

    if (device == NULL) {
        return;
    }

    host = RtcHostFromDevice(device);
    rtcInfo = (struct RtcConfigInfo *)host->data;
    if (rtcInfo != NULL) {
        HiRtcSwExit(rtcInfo);
        OsalMemFree(rtcInfo);
        host->data = NULL;
    }
    RtcHostDestroy(host);
}

struct HdfDriverEntry g_rtcDriverEntry = {
    .moduleVersion = 1,
    .Bind = HiRtcBind,
    .Init = HiRtcInit,
    .Release = HiRtcRelease,
    .moduleName = "HDF_PLATFORM_RTC",
};

HDF_INIT(g_rtcDriverEntry);
