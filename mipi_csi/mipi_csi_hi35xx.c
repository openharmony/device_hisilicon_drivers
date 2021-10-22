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

#include "mipi_csi_hi35xx.h"
#include "hdf_log.h"
#include "mipi_rx_hi2121.h"
#include "securec.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define HDF_LOG_TAG mipi_csi_hi35xx
/* macro definition */
#define MIPI_RX_DEV_NAME      "mipi_csi_dev"
#define MIPI_RX_PROC_NAME     "mipi_rx"

#define HIMEDIA_DYNAMIC_MINOR 255

#define COMBO_MAX_LANE_NUM    4

#define COMBO_MIN_WIDTH       32
#define COMBO_MIN_HEIGHT      32

#define MIPI_HEIGHT_ALIGN     2
#define MIPI_WIDTH_ALIGN      2

#define ENABLE_INT_MASK

#define SYNC_CODE_OFFSET8     (1 << 8) /* 8 --N+1 Frame sync */
#define SYNC_CODE_OFFSET10    (1 << 10) /* 10 --N+1 Frame sync */

/* function definition */
static bool MipiIsHsModeCfged(struct MipiCsiCntlr *cntlr)
{
    bool hsModeCfged;

    OsalSpinLock(&cntlr->ctxLock);
    hsModeCfged = cntlr->ctx.hsModeCfged;
    OsalSpinUnlock(&cntlr->ctxLock);

    return hsModeCfged;
}

static bool MipiIsDevValid(struct MipiCsiCntlr *cntlr, uint8_t devno)
{
    bool devValid;

    OsalSpinLock(&cntlr->ctxLock);
    devValid = cntlr->ctx.devValid[devno];
    OsalSpinUnlock(&cntlr->ctxLock);

    return devValid;
}

static bool MipiIsDevCfged(struct MipiCsiCntlr *cntlr, uint8_t devno)
{
    bool devCfged;

    OsalSpinLock(&cntlr->ctxLock);
    devCfged = cntlr->ctx.devCfged[devno];
    OsalSpinUnlock(&cntlr->ctxLock);

    return devCfged;
}

static int CheckLane(uint8_t devno, int laneNum, LaneDivideMode curLaneDivideMode,
    int *laneSum, const short pLaneId[])
{
    int i;
    int j;
    int allLaneIdInvalidFlag = 1;
    for (i = 0; i < laneNum; i++) {
        int tempId = pLaneId[i];
        int laneValid;

        if (tempId < -1 || tempId >= COMBO_MAX_LANE_NUM) {
            HDF_LOGE("%s: laneId[%d] is invalid value %d.", __func__, i, tempId);
            return HDF_ERR_INVALID_PARAM;
        }

        if (tempId == -1) {
            continue;
        }
        *laneSum = *laneSum + 1;
        allLaneIdInvalidFlag = 0;

        for (j = i + 1; j < laneNum; j++) {
            if (tempId == pLaneId[j]) {
                HDF_LOGE("%s: laneId[%d] can't be same value %d as laneId[%d]", __func__, i, tempId, j);
                return HDF_ERR_INVALID_PARAM;
            }
        }

        laneValid = MipiRxDrvIsLaneValid(devno, tempId, curLaneDivideMode);
        if (laneValid == 0) {
            HDF_LOGE("%s: laneId[%d] %d is invalid in hs_mode %d", __func__, i, tempId, curLaneDivideMode);
            return HDF_ERR_INVALID_PARAM;
        }
    }
    if (allLaneIdInvalidFlag != 0) {
        HDF_LOGE("%s: all laneId is invalid!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    return HDF_SUCCESS;
}

static int CheckLaneId(struct MipiCsiCntlr *cntlr, uint8_t devno, InputMode inputMode, const short pLaneId[])
{
    int laneNum;
    int laneSum = 0;
    int  mode1LaneNum = 2;
    LaneDivideMode curLaneDivideMode;

    if (inputMode == INPUT_MODE_MIPI) {
        laneNum = MIPI_LANE_NUM;
    } else if (inputMode == INPUT_MODE_LVDS) {
        laneNum = LVDS_LANE_NUM;
    } else {
        return HDF_SUCCESS;
    }

    OsalSpinLock(&cntlr->ctxLock);
    curLaneDivideMode = cntlr->ctx.laneDivideMode;
    OsalSpinUnlock(&cntlr->ctxLock);
    if (CheckLane(devno, laneNum, curLaneDivideMode, &laneSum, pLaneId) != HDF_SUCCESS) {
        HDF_LOGE("%s: laneId is invalid!", __func__);
        return HDF_FAILURE;
    }
    if ((curLaneDivideMode == LANE_DIVIDE_MODE_1) && (laneSum > mode1LaneNum)) {
        HDF_LOGE("%s: When divide mode is LANE_DIVIDE_MODE_1, valid lane number cannot be greater than 2", __func__);
        return HDF_ERR_INVALID_PARAM;
    }
    return HDF_SUCCESS;
}

static unsigned int MipiGetLaneBitmap(InputMode inputMode, const short *pLaneId, unsigned int *pTotalLaneNum)
{
    unsigned int laneBitmap = 0;
    int laneNum;
    int totalLaneNum;
    int i;

    if (inputMode == INPUT_MODE_MIPI) {
        laneNum = MIPI_LANE_NUM;
    } else {
        laneNum = LVDS_LANE_NUM;
    }

    totalLaneNum = 0;

    for (i = 0; i < laneNum; i++) {
        short tmpLaneId;
        tmpLaneId = pLaneId[i];

        if (tmpLaneId != -1) {
            laneBitmap = laneBitmap | (1 << (unsigned short)(tmpLaneId));
            totalLaneNum++;
        }
    }

    *pTotalLaneNum = totalLaneNum;

    return laneBitmap;
}

static int MipiCheckCombDevAttr(const ComboDevAttr *pAttr)
{
    if (pAttr->devno >= COMBO_DEV_MAX_NUM) {
        HDF_LOGE("%s: invalid comboDev number(%d).", __func__, pAttr->devno);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->inputMode < INPUT_MODE_MIPI || pAttr->inputMode >= INPUT_MODE_BUTT) {
        HDF_LOGE("%s: invalid inputMode(%d).", __func__, pAttr->inputMode);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->dataRate != MIPI_DATA_RATE_X1) {
        HDF_LOGE("%s: invalid dataRate(%d).", __func__, pAttr->dataRate);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->imgRect.x < 0 || pAttr->imgRect.y < 0) {
        HDF_LOGE("%s: crop x and y (%d, %d) must be great than 0", __func__, pAttr->imgRect.x, pAttr->imgRect.y);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->imgRect.width < COMBO_MIN_WIDTH || pAttr->imgRect.height < COMBO_MIN_HEIGHT) {
        HDF_LOGE("%s: invalid imgSize(%d, %d), can't be smaller than (%d, %d)", __func__,
            pAttr->imgRect.width, pAttr->imgRect.height, COMBO_MIN_WIDTH, COMBO_MIN_HEIGHT);
        return HDF_ERR_INVALID_PARAM;
    }

    /* width and height align 2 */
    if ((pAttr->imgRect.width & (MIPI_WIDTH_ALIGN - 1)) != 0) {
        HDF_LOGE("%s: imgWidth should be %d bytes align which is %d!", __func__,
            MIPI_WIDTH_ALIGN, pAttr->imgRect.width);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->imgRect.height & (MIPI_HEIGHT_ALIGN - 1)) != 0) {
        HDF_LOGE("%s: imgHeight should be %d bytes align which is %d!", __func__,
            MIPI_WIDTH_ALIGN, pAttr->imgRect.height);
        return HDF_ERR_INVALID_PARAM;
    }

    return HDF_SUCCESS;
}

static int CheckLvdsWdrMode(const LvdsDevAttr *pAttr)
{
    int ret = HDF_SUCCESS;

    switch (pAttr->wdrMode) {
        case HI_WDR_MODE_2F:
        case HI_WDR_MODE_3F:
        case HI_WDR_MODE_4F: {
            if (pAttr->vsyncAttr.syncType != LVDS_VSYNC_NORMAL &&
                pAttr->vsyncAttr.syncType != LVDS_VSYNC_SHARE) {
                HDF_LOGE("%s: invalid syncType, must be LVDS_VSYNC_NORMAL or LVDS_VSYNC_SHARE", __func__);
                ret = HDF_FAILURE;
            }
            break;
        }

        case HI_WDR_MODE_DOL_2F:
        case HI_WDR_MODE_DOL_3F:
        case HI_WDR_MODE_DOL_4F: {
            if (pAttr->vsyncAttr.syncType == LVDS_VSYNC_NORMAL) {
                if (pAttr->fidAttr.fidType != LVDS_FID_IN_SAV &&
                    pAttr->fidAttr.fidType != LVDS_FID_IN_DATA) {
                    HDF_LOGE("%s: invalid fidType, must be LVDS_FID_IN_SAV or LVDS_FID_IN_DATA", __func__);
                    ret = HDF_FAILURE;
                }
            } else if (pAttr->vsyncAttr.syncType == LVDS_VSYNC_HCONNECT) {
                if (pAttr->fidAttr.fidType != LVDS_FID_NONE &&
                    pAttr->fidAttr.fidType != LVDS_FID_IN_DATA) {
                    HDF_LOGE("%s: invalid fidType, must be LVDS_FID_NONE or LVDS_FID_IN_DATA", __func__);
                    ret = HDF_FAILURE;
                }
            } else {
                HDF_LOGE("%s: invalid syncType, must be LVDS_VSYNC_NORMAL or LVDS_VSYNC_HCONNECT", __func__);
                ret = HDF_FAILURE;
            }
            break;
        }

        default:
            break;
    }

    return ret;
}

static int CheckLvdsDevAttr(struct MipiCsiCntlr *cntlr, uint8_t devno, const LvdsDevAttr *pAttr)
{
    int ret;

    if ((pAttr->inputDataType < DATA_TYPE_RAW_8BIT) ||
        (pAttr->inputDataType > DATA_TYPE_RAW_16BIT)) {
        HDF_LOGE("%s: invalid dataType, must be in [%d, %d]", __func__, DATA_TYPE_RAW_8BIT, DATA_TYPE_RAW_16BIT);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->wdrMode < HI_WDR_MODE_NONE) || (pAttr->wdrMode >= HI_WDR_MODE_BUTT)) {
        HDF_LOGE("%s: invalid wdrMode, must be in [%d, %d)", __func__, HI_WDR_MODE_NONE, HI_WDR_MODE_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->syncMode < LVDS_SYNC_MODE_SOF) || (pAttr->syncMode >= LVDS_SYNC_MODE_BUTT)) {
        HDF_LOGE("%s: invalid syncMode, must be in [%d, %d)", __func__, LVDS_SYNC_MODE_SOF, LVDS_SYNC_MODE_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->vsyncAttr.syncType < LVDS_VSYNC_NORMAL ||
        pAttr->vsyncAttr.syncType >= LVDS_VSYNC_BUTT) {
        HDF_LOGE("%s: invalid vsync_code, must be in [%d, %d)", __func__, LVDS_VSYNC_NORMAL, LVDS_VSYNC_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->fidAttr.fidType < LVDS_FID_NONE || pAttr->fidAttr.fidType >= LVDS_FID_BUTT) {
        HDF_LOGE("%s: invalid fidType, must be in [%d, %d)", __func__, LVDS_FID_NONE, LVDS_FID_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->fidAttr.outputFil != TRUE && pAttr->fidAttr.outputFil != FALSE) {
        HDF_LOGE("%s: invalid outputFil, must be HI_TURE or FALSE", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->dataEndian < LVDS_ENDIAN_LITTLE) || (pAttr->dataEndian >= LVDS_ENDIAN_BUTT)) {
        HDF_LOGE("%s: invalid lvds_bit_endian, must be in [%d, %d)", __func__, LVDS_ENDIAN_LITTLE, LVDS_ENDIAN_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->syncCodeEndian < LVDS_ENDIAN_LITTLE) ||
        (pAttr->syncCodeEndian >= LVDS_ENDIAN_BUTT)) {
        HDF_LOGE("%s: invalid lvds_bit_endian, must be in [%d, %d)", __func__, LVDS_ENDIAN_LITTLE, LVDS_ENDIAN_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    ret = CheckLvdsWdrMode(pAttr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: CheckLvdsWdrMode failed!", __func__);
        return HDF_FAILURE;
    }

    ret = CheckLaneId(cntlr, devno, INPUT_MODE_LVDS, pAttr->laneId);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: CheckLaneId failed!", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void GetSyncCodes(const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM], unsigned short offset,
    unsigned short dst[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    int i;
    int j;
    int k;

    /*
     * SONY DOL N frame and N+1 Frame has the different syncCode
     * N+1 Frame FSET is 1, FSET is the 10th bit
     */
    for (i = 0; i < LVDS_LANE_NUM; i++) {
        for (j = 0; j < WDR_VC_NUM; j++) {
            for (k = 0; k < SYNC_CODE_NUM; k++) {
                dst[i][j][k] = syncCode[i][j][k] + offset;
            }
        }
    }
}

static void MipiSetLvdsSyncCodes(uint8_t devno, const LvdsDevAttr *pAttr,
    unsigned int totalLaneNum, unsigned int laneBitmap)
{
    int ret;
    unsigned short offset;
    /* Sony DOL Mode, vmalloc and OsalMemAlloc are used in different os. */
    unsigned short pNxtSyncCode[LVDS_LANE_NUM][WDR_VC_NUM][SYNC_CODE_NUM] = { 0 };

    if (pAttr->wdrMode >= HI_WDR_MODE_DOL_2F && pAttr->wdrMode <= HI_WDR_MODE_DOL_4F) {
        /* DATA_TYPE_RAW_10BIT or DATA_TYPE_RAW_8BIT needs to be confirmed. */
        offset = (pAttr->inputDataType == DATA_TYPE_RAW_10BIT) ? SYNC_CODE_OFFSET8 : SYNC_CODE_OFFSET10;
        /*
         * SONY DOL N frame and N+1 Frame has the different syncCode
         * N+1 Frame FSET is 1, FSET is the 10th bit
         */
        GetSyncCodes(pAttr->syncCode, offset, pNxtSyncCode);

        /* Set Dony DOL Line Information */
        if (pAttr->fidAttr.fidType == LVDS_FID_IN_DATA) {
            MipiRxDrvSetDolLineInformation(devno, pAttr->wdrMode);
        }
    } else {
        ret = memcpy_s(pNxtSyncCode, sizeof(pNxtSyncCode), &pAttr->syncCode, sizeof(pNxtSyncCode));
        if (ret != EOK) {
            HDF_LOGE("%s: [memcpy_s] failed", __func__);
            return;
        }
    }

    /* LVDS_CTRL Sync code */
    MipiRxDrvSetLvdsSyncCode(devno, totalLaneNum, pAttr->laneId, pAttr->syncCode);
    MipiRxDrvSetLvdsNxtSyncCode(devno, totalLaneNum, pAttr->laneId, pNxtSyncCode);

    /* PHY Sync code detect setting */
    MipiRxDrvSetPhySyncConfig(pAttr, laneBitmap, pNxtSyncCode);
}

static void MipiSetLvdsIntMask(uint8_t devno)
{
    (void)memset_s(MipiRxHalGetLvdsErrIntCnt(devno), sizeof(LvdsErrIntCnt), 0, sizeof(LvdsErrIntCnt));
    (void)memset_s(MipiRxHalGetAlignErrIntCnt(devno), sizeof(AlignErrIntCnt), 0, sizeof(AlignErrIntCnt));

#ifdef ENABLE_INT_MASK
    MipiRxDrvSetMipiIntMask(devno);
    MipiRxDrvSetLvdsCtrlIntMask(devno, LVDS_CTRL_INT_MASK);
    MipiRxDrvSetAlignIntMask(devno, ALIGN0_INT_MASK);
#endif
}

static void MipiSetLvdsPhySyncCfg(const struct MipiCsiCntlr *cntlr, uint8_t devno,
    const LvdsDevAttr *pLvdsAttr, unsigned int laneBitmap, unsigned int laneNum)
{
    /* phy lane config */
    MipiRxDrvSetLinkLaneId(devno, INPUT_MODE_LVDS, pLvdsAttr->laneId, laneBitmap, cntlr->ctx.laneDivideMode);
    MipiRxDrvSetMemCken(devno, TRUE);
    MipiRxDrvSetClrCken(devno, TRUE);

    MipiRxDrvSetPhyConfig(INPUT_MODE_LVDS, laneBitmap);

    /* sync codes */
    MipiSetLvdsSyncCodes(devno, pLvdsAttr, laneNum, laneBitmap);

    MipiRxSetPhyRgLp0ModeEn(devno, 0);
    MipiRxSetPhyRgLp1ModeEn(devno, 0);
}

static int MipiSetLvdsDevAttr(struct MipiCsiCntlr *cntlr, const ComboDevAttr *pComboDevAttr)
{
    uint8_t devno;
    const LvdsDevAttr *pLvdsAttr = NULL;
    int ret;
    unsigned int laneBitmap;
    unsigned int laneNum;

    devno = pComboDevAttr->devno;
    pLvdsAttr = &pComboDevAttr->lvdsAttr;

    if (!MipiIsHsModeCfged(cntlr)) {
        HDF_LOGE("%s: mipi must set hs mode before set lvds attr!", __func__);
        return HDF_FAILURE;
    }

    if (!MipiIsDevValid(cntlr, devno)) {
        HDF_LOGE("%s: invalid combo dev num after set hs mode!", __func__);
        return HDF_FAILURE;
    }

    ret = CheckLvdsDevAttr(cntlr, devno, pLvdsAttr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: CheckLvdsDevAttr failed!", __func__);
        return HDF_FAILURE;
    }

    laneBitmap = MipiGetLaneBitmap(INPUT_MODE_LVDS, pLvdsAttr->laneId, &laneNum);

    /* work mode */
    MipiRxDrvSetWorkMode(devno, INPUT_MODE_LVDS);

    /* image crop */
    MipiRxDrvSetLvdsImageRect(devno, &pComboDevAttr->imgRect, laneNum);
    MipiRxDrvSetLvdsCropEn(devno, TRUE);

    /* data type & mode */
    ret = MipiRxDrvSetLvdsWdrMode(devno, pLvdsAttr->wdrMode, &pLvdsAttr->vsyncAttr, &pLvdsAttr->fidAttr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: set lvds wdr mode failed!", __func__);
        return HDF_FAILURE;
    }

    MipiRxDrvSetLvdsCtrlMode(devno, pLvdsAttr->syncMode, pLvdsAttr->inputDataType,
        pLvdsAttr->dataEndian, pLvdsAttr->syncCodeEndian);

    /* data rate */
    MipiRxDrvSetLvdsDataRate(devno, pComboDevAttr->dataRate);
    MipiSetLvdsPhySyncCfg(cntlr, devno, pLvdsAttr, laneBitmap, laneNum);
    MipiSetLvdsIntMask(devno);

    return HDF_SUCCESS;
}

static int CheckMipiDevAttr(struct MipiCsiCntlr *cntlr, uint8_t devno, const MipiDevAttr *pAttr)
{
    int ret;
    int i;

    if ((pAttr->inputDataType < DATA_TYPE_RAW_8BIT) || (pAttr->inputDataType >= DATA_TYPE_BUTT)) {
        HDF_LOGE("%s: invalid inputDataType, must be in [%d, %d)", __func__, DATA_TYPE_RAW_8BIT, DATA_TYPE_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->wdrMode < HI_MIPI_WDR_MODE_NONE) || (pAttr->wdrMode >= HI_MIPI_WDR_MODE_BUTT)) {
        HDF_LOGE("%s: invalid wdrMode, must be in [%d, %d)", __func__, HI_MIPI_WDR_MODE_NONE, HI_MIPI_WDR_MODE_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((pAttr->wdrMode != HI_MIPI_WDR_MODE_NONE) &&
        (pAttr->inputDataType >= DATA_TYPE_YUV420_8BIT_NORMAL)) {
        HDF_LOGE("%s: It do not support wdr mode when inputDataType is yuv format!", __func__);
        return HDF_ERR_INVALID_PARAM;
    }

    if (pAttr->wdrMode == HI_MIPI_WDR_MODE_DT) {
        for (i = 0; i < WDR_VC_NUM; i++) {
            /* dataType must be the CSI-2 reserve Type [0x38, 0x3f] */
            if (pAttr->dataType[i] < 0x38 || pAttr->dataType[i] > 0x3f) {
                HDF_LOGE("%s: invalid dataType[%d]: %d, must be in [0x38, 0x3f]", __func__, i, pAttr->dataType[i]);
                return HDF_ERR_INVALID_PARAM;
            }
        }
    }

    ret = CheckLaneId(cntlr, devno, INPUT_MODE_MIPI, pAttr->laneId);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: CheckLaneId failed!", __func__);
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static void MipiSetDtAndMode(uint8_t devno, const MipiDevAttr *pAttr)
{
    DataType inputDataType;

    inputDataType = pAttr->inputDataType;

    MipiRxDrvSetDiDt(devno, inputDataType);
    MipiRxDrvSetMipiYuvDt(devno, inputDataType);

    if (pAttr->wdrMode == HI_MIPI_WDR_MODE_DT) {
        MipiRxDrvSetMipiWdrUserDt(devno, inputDataType, pAttr->dataType);
    } else if (pAttr->wdrMode == HI_MIPI_WDR_MODE_DOL) {
        MipiRxDrvSetMipiDolId(devno, inputDataType, NULL);
    }

    MipiRxDrvSetMipiWdrMode(devno, pAttr->wdrMode);
}

static void MipiSetIntMask(uint8_t devno)
{
    /* interrupt mask */
    (void)memset_s(MipiRxHalGetMipiErrInt(devno), sizeof(MipiErrIntCnt), 0, sizeof(MipiErrIntCnt));
    (void)memset_s(MipiRxHalGetAlignErrIntCnt(devno), sizeof(AlignErrIntCnt), 0, sizeof(AlignErrIntCnt));

#ifdef ENABLE_INT_MASK
    MipiRxDrvSetMipiIntMask(devno);
    MipiRxDrvSetMipiCtrlIntMask(devno, MIPI_CTRL_INT_MASK); /* 0x12f8 */
    MipiRxDrvSetMipiPkt1IntMask(devno, MIPI_PKT_INT1_MASK); /* 0x1064 */
    MipiRxDrvSetMipiPkt2IntMask(devno, MIPI_PKT_INT2_MASK); /* 0x1074 */
    MipiRxDrvSetMipiFrameIntMask(devno, MIPI_FRAME_INT_MASK); /* 0x1084 */
    MipiRxDrvSetAlignIntMask(devno, ALIGN0_INT_MASK); /* 0x18f8 */
#endif
}

static void MipiSetPhyCfg(struct MipiCsiCntlr *cntlr, const ComboDevAttr *pComboDevAttr)
{
    uint8_t devno;
    unsigned int laneBitmap;
    unsigned int laneNum;
    const MipiDevAttr *pMipiAttr = NULL;

    devno = pComboDevAttr->devno;
    pMipiAttr = &pComboDevAttr->mipiAttr;

    laneBitmap = MipiGetLaneBitmap(INPUT_MODE_MIPI, pMipiAttr->laneId, &laneNum);
    MipiRxDrvSetLinkLaneId(devno, INPUT_MODE_MIPI, pMipiAttr->laneId, laneBitmap, cntlr->ctx.laneDivideMode);

    if (cntlr->ctx.laneDivideMode == 0) {
        MipiRxDrvSetLaneNum(devno, laneNum);
    } else {
        MipiRxDrvSetLaneNum(devno, laneNum);

        if (devno == 1) {
            if (laneBitmap == 0x3) {
                laneBitmap = 0xa;
            } else if (laneBitmap == 0x1) {
                laneBitmap = 0x2;
            } else if (laneBitmap == 0x2) {
                laneBitmap = 0x8;
            } else {
                laneBitmap = 0xa;
            }
        }
    }

    cntlr->ctx.laneBitmap[devno] = laneBitmap;
    MipiRxDrvSetPhyConfig(INPUT_MODE_MIPI, laneBitmap);
}

static int MipiSetMipiDevAttr(struct MipiCsiCntlr *cntlr, const ComboDevAttr *pComboDevAttr)
{
    uint8_t devno;
    const MipiDevAttr *pMipiAttr = NULL;
    int ret;

    devno = pComboDevAttr->devno;
    pMipiAttr = &pComboDevAttr->mipiAttr;

    if (!MipiIsHsModeCfged(cntlr)) {
        HDF_LOGE("%s: mipi must set hs mode before set mipi attr!", __func__);
        return HDF_FAILURE;
    }

    if (!MipiIsDevValid(cntlr, devno)) {
        HDF_LOGE("%s: invalid combo dev num after set hs mode!", __func__);
        return HDF_FAILURE;
    }

    ret = CheckMipiDevAttr(cntlr, devno, pMipiAttr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: CheckMipiDevAttr failed!", __func__);
        return HDF_FAILURE;
    }

    /* work mode */
    MipiRxDrvSetWorkMode(devno, INPUT_MODE_MIPI);

    /* image crop */
    MipiRxDrvSetMipiImageRect(devno, &pComboDevAttr->imgRect);
    MipiRxDrvSetMipiCropEn(devno, TRUE);

    /* data type & mode */
    MipiSetDtAndMode(devno, pMipiAttr);

    /* data rate */
    MipiRxDrvSetDataRate(devno, pComboDevAttr->dataRate);

    /* phy lane config */
    MipiSetPhyCfg(cntlr, pComboDevAttr);

    MipiRxDrvSetMemCken(devno, TRUE);

    MipiSetIntMask(devno);

    return HDF_SUCCESS;
}

static int MipiSetCmosDevAttr(const ComboDevAttr *pComboDevAttr)
{
    uint8_t devno;
    unsigned int laneBitmap = 0xf;
    unsigned int phyId = 0;

    devno = pComboDevAttr->devno;

    if ((devno > CMOS_MAX_DEV_NUM) || (devno == 0)) {
        HDF_LOGE("%s: invalid cmos devno(%d)!", __func__, devno);
        return HDF_ERR_INVALID_PARAM;
    }

    if (devno == 1) {
        phyId = 0;
        laneBitmap = 0xf;
    }

    if (pComboDevAttr->inputMode != INPUT_MODE_BT656) {
        if (devno == 1) {
            MipiRxDrvSetCmosEn(phyId, 1);
        }
        MipiRxDrvSetPhyEn(laneBitmap);
        MipiRxDrvSetPhyCfgEn(laneBitmap, 1);
        MipiRxDrvSetLaneEn(laneBitmap);
        MipiRxDrvSetPhyCilEn(laneBitmap, 1);
        MipiRxDrvSetPhyCfgMode(INPUT_MODE_CMOS, laneBitmap); /* BT1120 may not need 698~700 */

        MipiRxSetPhyRgLp0ModeEn(phyId, 0);
        MipiRxSetPhyRgLp1ModeEn(phyId, 0);
    }

    return HDF_SUCCESS;
}

static int32_t MipiSetComboDevAttr(struct MipiCsiCntlr *cntlr, const ComboDevAttr *pAttr)
{
    int32_t ret;

    ret = MipiCheckCombDevAttr(pAttr);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: mipi check comboDev attr failed!", __func__);
        return HDF_FAILURE;
    }

    switch (pAttr->inputMode) {
        case INPUT_MODE_LVDS:
        case INPUT_MODE_SUBLVDS:
        case INPUT_MODE_HISPI: {
            ret = MipiSetLvdsDevAttr(cntlr, pAttr);
            if (ret < 0) {
                HDF_LOGE("%s: mipi set lvds attr failed!", __func__);
                ret = HDF_FAILURE;
            }
            break;
        }

        case INPUT_MODE_MIPI: {
            ret = MipiSetMipiDevAttr(cntlr, pAttr);
            if (ret != HDF_SUCCESS) {
                HDF_LOGE("%s: mipi set mipi attr failed!", __func__);
                ret = HDF_FAILURE;
            }
            break;
        }

        case INPUT_MODE_CMOS:
        case INPUT_MODE_BT601:
        case INPUT_MODE_BT656:
        case INPUT_MODE_BT1120: {
            ret = MipiSetCmosDevAttr(pAttr);
            if (ret != HDF_SUCCESS) {
                HDF_LOGE("%s: mipi set cmos attr failed!", __func__);
                ret = HDF_FAILURE;
            }
            break;
        }

        default: {
            HDF_LOGE("%s: invalid input mode", __func__);
            ret = HDF_FAILURE;
            break;
        }
    }

    return ret;
}

static int32_t SetComboDevAttr(struct MipiCsiCntlr *cntlr, const ComboDevAttr *argp)
{
    int32_t ret;
    errno_t err;
    uint8_t devno;

    if (argp == NULL) {
        HDF_LOGE("%s: NULL pointer \r", __func__);
        return HDF_ERR_INVALID_OBJECT;
    }

    devno = argp->devno;
    OsalSpinLock(&cntlr->ctxLock);
    cntlr->ctx.devCfged[devno] = true;
    err = memcpy_s(&cntlr->ctx.comboDevAttr[devno], sizeof(ComboDevAttr), argp, sizeof(ComboDevAttr));
    if (err != EOK) {
        cntlr->ctx.devCfged[devno] = false;
        HDF_LOGE("%s: [memcpy_s] failed.", __func__);
        ret = HDF_FAILURE;
    } else {
        ret = HDF_SUCCESS;
    }
    OsalSpinUnlock(&cntlr->ctxLock);

    return ret;
}

static int32_t Hi35xxSetComboDevAttr(struct MipiCsiCntlr *cntlr, ComboDevAttr *pAttr)
{
    int32_t ret;

    ret = MipiSetComboDevAttr(cntlr, pAttr);
    if (ret == HDF_SUCCESS) {
        ret = SetComboDevAttr(cntlr, pAttr);
    } else {
        HDF_LOGE("%s: [MipiSetComboDevAttr] failed.", __func__);
    }

    return ret;
}

static int32_t Hi35xxSetExtDataType(struct MipiCsiCntlr *cntlr, ExtDataType *dataType)
{
    unsigned int i;
    uint8_t devno;
    InputMode inputMode;
    DataType inputDataType;

    devno = dataType->devno;

    if (devno >= COMBO_DEV_MAX_NUM) {
        HDF_LOGE("%s: invalid mipi dev number(%d).", __func__, devno);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!MipiIsDevCfged(cntlr, devno)) {
        HDF_LOGE("%s: MIPI device %d has not beed configured", __func__, devno);
        return HDF_FAILURE;
    }

    OsalSpinLock(&cntlr->ctxLock);
    inputMode = cntlr->ctx.comboDevAttr[devno].inputMode;
    inputDataType = cntlr->ctx.comboDevAttr[devno].mipiAttr.inputDataType;
    OsalSpinUnlock(&cntlr->ctxLock);

    if (inputMode != INPUT_MODE_MIPI) {
        HDF_LOGE("%s: devno: %d, input mode: %d, not support set data type", __func__, devno, inputMode);
        return HDF_ERR_INVALID_PARAM;
    }

    if (dataType->num > MAX_EXT_DATA_TYPE_NUM) {
        HDF_LOGE("%s: invalid ext data type num(%d)", __func__, dataType->num);
        return HDF_ERR_INVALID_PARAM;
    }

    for (i = 0; i < dataType->num; i++) {
        if (dataType->extDataBitWidth[i] < MIPI_RX_MIN_EXT_DATA_TYPE_BIT_WIDTH ||
            dataType->extDataBitWidth[i] > MIPI_RX_MAX_EXT_DATA_TYPE_BIT_WIDTH) {
            HDF_LOGE("%s: invalid ext data bit width(%d)", __func__, dataType->extDataBitWidth[i]);
            return HDF_ERR_INVALID_PARAM;
        }

        if (dataType->extDataBitWidth[i] % 2 != 0) { /* 2:even check */
            HDF_LOGE("%s: invalid ext data bit width(%d),must be even value", __func__, dataType->extDataBitWidth[i]);
            return HDF_ERR_INVALID_PARAM;
        }
    }

    MipiRxDrvSetExtDataType(dataType, inputDataType);

    return HDF_SUCCESS;
}

static int32_t Hi35xxSetPhyCmvmode(struct MipiCsiCntlr *cntlr, uint8_t devno, PhyCmvMode cmvMode)
{
    InputMode inputMode;
    unsigned int laneBitMap;

    if (devno >= COMBO_DEV_MAX_NUM) {
        HDF_LOGE("%s: invalid mipi dev number(%d).", __func__, devno);
        return HDF_ERR_INVALID_PARAM;
    }

    if ((cmvMode < PHY_CMV_GE1200MV) || (cmvMode >= PHY_CMV_BUTT)) {
        HDF_LOGE("%s: invalid common mode voltage mode: %d, must be int [%d, %d)", __func__,
            cmvMode, PHY_CMV_GE1200MV, PHY_CMV_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    if (!MipiIsDevCfged(cntlr, devno)) {
        HDF_LOGE("%s: MIPI device %d has not beed configured", __func__, devno);
        return HDF_ERR_INVALID_PARAM;
    }

    OsalSpinLock(&cntlr->ctxLock);
    inputMode = cntlr->ctx.comboDevAttr[devno].inputMode;
    laneBitMap = cntlr->ctx.laneBitmap[devno];
    OsalSpinUnlock(&cntlr->ctxLock);

    if (inputMode != INPUT_MODE_MIPI &&
        inputMode != INPUT_MODE_SUBLVDS &&
        inputMode != INPUT_MODE_LVDS &&
        inputMode != INPUT_MODE_HISPI) {
        HDF_LOGE("%s: devno: %d, input mode: %d, not support set common voltage mode", __func__, devno, inputMode);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvSetPhyCmvmode(inputMode, cmvMode, laneBitMap);

    return HDF_SUCCESS;
}

static int32_t Hi35xxResetSensor(struct MipiCsiCntlr *cntlr, uint8_t snsResetSource)
{
    (void)cntlr;
    if (snsResetSource >= SNS_MAX_RST_SOURCE_NUM) {
        HDF_LOGE("%s: invalid snsResetSource(%d).", __func__, snsResetSource);
        return HDF_ERR_INVALID_PARAM;
    }

    SensorDrvReset(snsResetSource);

    return HDF_SUCCESS;
}

static int32_t Hi35xxUnresetSensor(struct MipiCsiCntlr *cntlr, uint8_t snsResetSource)
{
    (void)cntlr;
    if (snsResetSource >= SNS_MAX_RST_SOURCE_NUM) {
        HDF_LOGE("%s: invalid snsResetSource(%d).", __func__, snsResetSource);
        return HDF_ERR_INVALID_PARAM;
    }

    SensorDrvUnreset(snsResetSource);

    return HDF_SUCCESS;
}

static int32_t Hi35xxResetRx(struct MipiCsiCntlr *cntlr, uint8_t comboDev)
{
    (void)cntlr;
    if (comboDev >= MIPI_RX_MAX_DEV_NUM) {
        HDF_LOGE("%s: invalid comboDev num(%d).", __func__, comboDev);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvCoreReset(comboDev);

    return HDF_SUCCESS;
}

static int32_t Hi35xxUnresetRx(struct MipiCsiCntlr *cntlr, uint8_t comboDev)
{
    (void)cntlr;
    if (comboDev >= MIPI_RX_MAX_DEV_NUM) {
        HDF_LOGE("%s: invalid comboDev num(%d).", __func__, comboDev);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvCoreUnreset(comboDev);

    return HDF_SUCCESS;
}

static void MipiSetDevValid(struct MipiCsiCntlr *cntlr, LaneDivideMode mode)
{
    switch (mode) {
        case LANE_DIVIDE_MODE_0:
            cntlr->ctx.devValid[0] = true;
            break;
        case LANE_DIVIDE_MODE_1:
            cntlr->ctx.devValid[0] = true;
            cntlr->ctx.devValid[1] = true;
            break;
        default:
            break;
    }
}

static int32_t Hi35xxSetHsMode(struct MipiCsiCntlr *cntlr, LaneDivideMode laneDivideMode)
{
    if ((laneDivideMode < LANE_DIVIDE_MODE_0) ||
        (laneDivideMode >= LANE_DIVIDE_MODE_BUTT)) {
        HDF_LOGE("%s: invalid laneDivideMode(%d), must be in [%d, %d)", __func__,
            laneDivideMode, LANE_DIVIDE_MODE_0, LANE_DIVIDE_MODE_BUTT);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvSetHsMode(laneDivideMode);

    OsalSpinLock(&cntlr->ctxLock);
    cntlr->ctx.laneDivideMode = laneDivideMode;
    cntlr->ctx.hsModeCfged = true;
    (void)memset_s(cntlr->ctx.devValid, sizeof(cntlr->ctx.devValid), 0, sizeof(cntlr->ctx.devValid));
    MipiSetDevValid(cntlr, laneDivideMode);
    OsalSpinUnlock(&cntlr->ctxLock);

    return HDF_SUCCESS;
}

static int32_t Hi35xxEnableClock(struct MipiCsiCntlr *cntlr, uint8_t comboDev)
{
    (void)cntlr;
    if (comboDev >= MIPI_RX_MAX_DEV_NUM) {
        HDF_LOGE("%s: invalid comboDev num(%d).", __func__, comboDev);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvEnableClock(comboDev);

    return HDF_SUCCESS;
}

static int32_t Hi35xxDisableClock(struct MipiCsiCntlr *cntlr, uint8_t comboDev)
{
    (void)cntlr;
    if (comboDev >= MIPI_RX_MAX_DEV_NUM) {
        HDF_LOGE("%s: invalid comboDev num(%d).", __func__, comboDev);
        return HDF_ERR_INVALID_PARAM;
    }

    MipiRxDrvDisableClock(comboDev);

    return HDF_SUCCESS;
}

static int32_t Hi35xxEnableSensorClock(struct MipiCsiCntlr *cntlr, uint8_t snsClkSource)
{
    (void)cntlr;
    if (snsClkSource >= SNS_MAX_CLK_SOURCE_NUM) {
        HDF_LOGE("%s: invalid snsClkSource(%d).", __func__, snsClkSource);
        return HDF_ERR_INVALID_PARAM;
    }

    SensorDrvEnableClock(snsClkSource);

    return HDF_SUCCESS;
}

static int32_t Hi35xxDisableSensorClock(struct MipiCsiCntlr *cntlr, uint8_t snsClkSource)
{
    (void)cntlr;
    if (snsClkSource >= SNS_MAX_CLK_SOURCE_NUM) {
        HDF_LOGE("%s: invalid snsClkSource(%d).", __func__, snsClkSource);
        return HDF_ERR_INVALID_PARAM;
    }

    SensorDrvDisableClock(snsClkSource);

    return HDF_SUCCESS;
}

#ifdef CONFIG_HI_PROC_SHOW_SUPPORT
static void Hi35xxGetMipiDevCtx(struct MipiCsiCntlr *cntlr, MipiDevCtx *ctx)
{
    OsalSpinLock(&cntlr->ctxLock);
    *ctx = cntlr->ctx;
    OsalSpinUnlock(&cntlr->ctxLock);
}

static void Hi35xxGetPhyErrIntCnt(struct MipiCsiCntlr *cntlr, unsigned int phyId, PhyErrIntCnt *errInfo)
{
    int32_t ret;
    PhyErrIntCnt *err = NULL;

    (void)cntlr;
    err = MipiRxHalGetPhyErrIntCnt(phyId);
    if (err == NULL) {
        HDF_LOGE("%s: [MipiRxHalGetPhyErrIntCnt] failed.", __func__);
        return;
    }
    ret = memcpy_s(errInfo, sizeof(PhyErrIntCnt), err, sizeof(PhyErrIntCnt));
    if (ret != EOK) {
        HDF_LOGE("%s: [memcpy_s] failed.", __func__);
    } else {
        HDF_LOGI("%s: success.", __func__);
    }
}

static void Hi35xxGetMipiErrInt(struct MipiCsiCntlr *cntlr, unsigned int phyId, MipiErrIntCnt *errInfo)
{
    int32_t ret;
    MipiErrIntCnt *err = NULL;

    (void)cntlr;
    err = MipiRxHalGetMipiErrInt(phyId);
    if (err == NULL) {
        HDF_LOGE("%s: [MipiRxHalGetMipiErrInt] failed.", __func__);
        return;
    }
    ret = memcpy_s(errInfo, sizeof(MipiErrIntCnt), err, sizeof(MipiErrIntCnt));
    if (ret != EOK) {
        HDF_LOGE("%s: [memcpy_s] failed.", __func__);
    } else {
        HDF_LOGI("%s: success.", __func__);
    }
}

static void Hi35xxGetLvdsErrIntCnt(struct MipiCsiCntlr *cntlr, unsigned int phyId, LvdsErrIntCnt *errInfo)
{
    int32_t ret;
    LvdsErrIntCnt *err = NULL;

    (void)cntlr;
    err = MipiRxHalGetLvdsErrIntCnt(phyId);
    if (err == NULL) {
        HDF_LOGE("%s: [MipiRxHalGetLvdsErrIntCnt] failed.", __func__);
        return;
    }
    ret = memcpy_s(errInfo, sizeof(LvdsErrIntCnt), err, sizeof(LvdsErrIntCnt));
    if (ret != EOK) {
        HDF_LOGE("%s: [memcpy_s] failed.", __func__);
    } else {
        HDF_LOGI("%s: success.", __func__);
    }
}

static void Hi35xxGetAlignErrIntCnt(struct MipiCsiCntlr *cntlr, unsigned int phyId, AlignErrIntCnt *errInfo)
{
    int32_t ret;
    AlignErrIntCnt *err = NULL;

    (void)cntlr;
    err = MipiRxHalGetAlignErrIntCnt(phyId);
    if (err == NULL) {
        HDF_LOGE("%s: [MipiRxHalGetAlignErrIntCnt] failed.", __func__);
        return;
    }
    ret = memcpy_s(errInfo, sizeof(AlignErrIntCnt), err, sizeof(AlignErrIntCnt));
    if (ret != EOK) {
        HDF_LOGE("%s: [memcpy_s] failed.", __func__);
    } else {
        HDF_LOGI("%s: success.", __func__);
    }
}

static void Hi35xxGetPhyData(struct MipiCsiCntlr *cntlr, int phyId, int laneId, unsigned int *laneData)
{
    (void)cntlr;
    *laneData = MipiRxDrvGetPhyData(phyId, laneId);
}

static void Hi35xxGetPhyMipiLinkData(struct MipiCsiCntlr *cntlr, int phyId, int laneId, unsigned int *laneData)
{
    (void)cntlr;
    *laneData = MipiRxDrvGetPhyMipiLinkData(phyId, laneId);
}

static void Hi35xxGetPhyLvdsLinkData(struct MipiCsiCntlr *cntlr, int phyId, int laneId, unsigned int *laneData)
{
    (void)cntlr;
    *laneData = MipiRxDrvGetPhyLvdsLinkData(phyId, laneId);
}

static void Hi35xxGetMipiImgsizeStatis(struct MipiCsiCntlr *cntlr, uint8_t devno, short vc, ImgSize *pSize)
{
    (void)cntlr;
    MipiRxDrvGetMipiImgsizeStatis(devno, vc, pSize);
}

static void Hi35xxGetLvdsImgsizeStatis(struct MipiCsiCntlr *cntlr, uint8_t devno, short vc, ImgSize *pSize)
{
    (void)cntlr;
    MipiRxDrvGetLvdsImgsizeStatis(devno, vc, pSize);
}

static void Hi35xxGetLvdsLaneImgsizeStatis(struct MipiCsiCntlr *cntlr, uint8_t devno, short lane, ImgSize *pSize)
{
    (void)cntlr;
    MipiRxDrvGetLvdsLaneImgsizeStatis(devno, lane, pSize);
}

static struct MipiCsiCntlrDebugMethod g_debugMethod = {
    .getMipiDevCtx = Hi35xxGetMipiDevCtx,
    .getPhyErrIntCnt = Hi35xxGetPhyErrIntCnt,
    .getMipiErrInt = Hi35xxGetMipiErrInt,
    .getLvdsErrIntCnt = Hi35xxGetLvdsErrIntCnt,
    .getAlignErrIntCnt = Hi35xxGetAlignErrIntCnt,
    .getPhyData = Hi35xxGetPhyData,
    .getPhyMipiLinkData = Hi35xxGetPhyMipiLinkData,
    .getPhyLvdsLinkData = Hi35xxGetPhyLvdsLinkData,
    .getMipiImgsizeStatis = Hi35xxGetMipiImgsizeStatis,
    .getLvdsImgsizeStatis = Hi35xxGetLvdsImgsizeStatis,
    .getLvdsLaneImgsizeStatis = Hi35xxGetLvdsLaneImgsizeStatis
};
#endif

static struct MipiCsiCntlr g_mipiCsi = {
    .devNo = 0
};

static struct MipiCsiCntlrMethod g_method = {
    .setComboDevAttr = Hi35xxSetComboDevAttr,
    .setPhyCmvmode = Hi35xxSetPhyCmvmode,
    .setExtDataType = Hi35xxSetExtDataType,
    .setHsMode = Hi35xxSetHsMode,
    .enableClock = Hi35xxEnableClock,
    .disableClock = Hi35xxDisableClock,
    .resetRx = Hi35xxResetRx,
    .unresetRx = Hi35xxUnresetRx,
    .enableSensorClock = Hi35xxEnableSensorClock,
    .disableSensorClock = Hi35xxDisableSensorClock,
    .resetSensor = Hi35xxResetSensor,
    .unresetSensor = Hi35xxUnresetSensor
};

static int32_t Hi35xxMipiCsiInit(struct HdfDeviceObject *device)
{
    int32_t ret;

    HDF_LOGI("%s: enter!", __func__);
    g_mipiCsi.priv = NULL;
    g_mipiCsi.ops = &g_method;
#ifdef CONFIG_HI_PROC_SHOW_SUPPORT
    g_mipiCsi.debugs = &g_debugMethod;
#endif
    ret = MipiCsiRegisterCntlr(&g_mipiCsi, device);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: [MipiCsiRegisterCntlr] failed!", __func__);
        return ret;
    }

    ret = MipiRxDrvInit();
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: [MipiRxDrvInit] failed.", __func__);
        return ret;
    }
#ifdef MIPICSI_VFS_SUPPORT
    ret = MipiCsiDevModuleInit(g_mipiCsi.devNo);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: [MipiCsiDevModuleInit] failed!", __func__);
        return ret;
    }
#endif

    OsalSpinInit(&g_mipiCsi.ctxLock);
    HDF_LOGI("%s: load mipi csi driver success!", __func__);

    return ret;
}

static void Hi35xxMipiCsiRelease(struct HdfDeviceObject *device)
{
    struct MipiCsiCntlr *cntlr = NULL;

    HDF_LOGI("%s: enter!", __func__);
    if (device == NULL) {
        HDF_LOGE("%s: device is NULL.", __func__);
        return;
    }
    cntlr = MipiCsiCntlrFromDevice(device);
    if (cntlr == NULL) {
        HDF_LOGE("%s: cntlr is NULL.", __func__);
        return;
    }

    OsalSpinDestroy(&cntlr->ctxLock);
#ifdef MIPICSI_VFS_SUPPORT
    MipiCsiDevModuleExit(cntlr->devNo);
#endif
    MipiRxDrvExit();
    MipiCsiUnregisterCntlr(&g_mipiCsi);
    g_mipiCsi.priv = NULL;

    HDF_LOGI("%s: unload mipi csi driver success!", __func__);
}

struct HdfDriverEntry g_mipiCsiDriverEntry = {
    .moduleVersion = 1,
    .Init = Hi35xxMipiCsiInit,
    .Release = Hi35xxMipiCsiRelease,
    .moduleName = "HDF_MIPI_RX",
};
HDF_INIT(g_mipiCsiDriverEntry);

#ifdef __cplusplus
#if __cplusplus
}

#endif
#endif /* End of #ifdef __cplusplus */
