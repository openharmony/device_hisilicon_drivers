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

#ifndef MIPI_RX_HI2121_H
#define MIPI_RX_HI2121_H

#include "mipi_csi_core.h"

#define MIPI_RX_MAX_PHY_NUM                    1
#define MIPI_RX_MAX_EXT_DATA_TYPE_BIT_WIDTH    16
#define MIPI_RX_MIN_EXT_DATA_TYPE_BIT_WIDTH    8

#define MIPI_CIL_INT_MASK   0x00003f3f
#define MIPI_CTRL_INT_MASK  0x00030003
#define LVDS_CTRL_INT_MASK  0x0f110000 /* lvds_vsync_msk and lane0~3_sync_err_msk ignore, not err int */
#define MIPI_FRAME_INT_MASK 0x000f0000
#define MIPI_PKT_INT1_MASK  0x0001000f
#define MIPI_PKT_INT2_MASK  0x000f000f
#define ALIGN0_INT_MASK     0x0000001f

PhyErrIntCnt *MipiRxHalGetPhyErrIntCnt(unsigned int phyId);
MipiErrIntCnt *MipiRxHalGetMipiErrInt(unsigned int phyId);
LvdsErrIntCnt *MipiRxHalGetLvdsErrIntCnt(unsigned int phyId);
AlignErrIntCnt *MipiRxHalGetAlignErrIntCnt(unsigned int phyId);

/* sensor function */
void SensorDrvEnableClock(uint8_t snsClkSource);
void SensorDrvDisableClock(uint8_t snsClkSource);

void SensorDrvReset(uint8_t snsResetSource);
void SensorDrvUnreset(uint8_t snsResetSource);

/* MipiRx function */
void MipiRxDrvSetWorkMode(uint8_t devno, InputMode inputMode);
void MipiRxDrvSetMipiImageRect(uint8_t devno, const ImgRect *pImgRect);
void MipiRxDrvSetMipiCropEn(uint8_t devno, int enable);
void MipiRxDrvSetDiDt(uint8_t devno, DataType inputDataType);
void MipiRxDrvSetMipiYuvDt(uint8_t devno, DataType inputDataType);
void MipiRxDrvSetMipiWdrUserDt(uint8_t devno, DataType inputDataType,
    const short dataType[WDR_VC_NUM]);
void MipiRxDrvSetMipiDolId(uint8_t devno, DataType inputDataType, const short dolId[]);
void MipiRxDrvSetMipiWdrMode(uint8_t devno, MipiWdrMode wdrMode);
unsigned int MipiRxDrvGetPhyData(int phyId, int laneId);
unsigned int MipiRxDrvGetPhyMipiLinkData(int phyId, int laneId);
unsigned int MipiRxDrvGetPhyLvdsLinkData(int phyId, int laneId);

void MipiRxDrvSetDataRate(uint8_t devno, MipiDataRate dataRate);
void MipiRxDrvSetLinkLaneId(uint8_t devno, InputMode inputMode, const short *pLaneId,
    unsigned int laneBitmap, LaneDivideMode mode);
void MipiRxDrvSetMemCken(uint8_t devno, int enable);
void MipiRxDrvSetClrCken(uint8_t devno, int enable);
void MipiRxDrvSetLaneNum(uint8_t devno, unsigned int laneNum);
void MipiRxDrvSetPhyConfig(InputMode inputMode, unsigned int laneBitmap);
void MipiRxDrvSetPhyCmvmode(InputMode inputMode, PhyCmvMode cmvMode, unsigned int laneBitmap);

void MipiRxDrvSetPhyEn(unsigned int laneBitmap);
void MipiRxDrvSetCmosEn(unsigned int phyId, int enable);
void MipiRxDrvSetLaneEn(unsigned int laneBitmap);
void MipiRxDrvSetPhyCilEn(unsigned int laneBitmap, int enable);
void MipiRxDrvSetPhyCfgMode(InputMode inputMode, unsigned int laneBitmap);
void MipiRxDrvSetPhyCfgEn(unsigned int laneBitmap, int enable);
void MipiRxSetPhyRgLp0ModeEn(unsigned int phyId, int enable);
void MipiRxSetPhyRgLp1ModeEn(unsigned int phyId, int enable);
void MipiRxDrvSetExtDataType(const ExtDataType* dataType, DataType inputDataType);

void MipiRxDrvSetLvdsImageRect(uint8_t devno, const ImgRect *pImgRect, short totalLaneNum);
void MipiRxDrvSetLvdsCropEn(uint8_t devno, int enable);

int MipiRxDrvSetLvdsWdrMode(uint8_t devno, WdrMode wdrMode,
    const LvdsVsyncAttr *vsyncAttr, const LvdsFidAttr *fidAttr);
void MipiRxDrvSetLvdsCtrlMode(uint8_t devno, LvdsSyncMode syncMode,
    DataType inputDataType, LvdsBitEndian dataEndian, LvdsBitEndian syncCodeEndian);

void MipiRxDrvSetLvdsDataRate(uint8_t devno, MipiDataRate dataRate);

void MipiRxDrvSetDolLineInformation(uint8_t devno, WdrMode wdrMode);
void MipiRxDrvSetLvdsSyncCode(uint8_t devno, unsigned int laneCnt,
    const short laneId[LVDS_LANE_NUM], const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM]);

void MipiRxDrvSetLvdsNxtSyncCode(uint8_t devno, unsigned int laneCnt,
    const short laneId[LVDS_LANE_NUM], const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM]);

void MipiRxDrvSetPhySyncConfig(const LvdsDevAttr *pAttr, unsigned int laneBitmap,
    const unsigned short nxtSyncCode[][WDR_VC_NUM][SYNC_CODE_NUM]);

int MipiRxDrvIsLaneValid(uint8_t devno, short laneId, LaneDivideMode mode);
void MipiRxDrvSetHsMode(LaneDivideMode laneDivideMode);

void MipiRxDrvGetMipiImgsizeStatis(uint8_t devno, short vc, ImgSize *pSize);
void MipiRxDrvGetLvdsImgsizeStatis(uint8_t devno, short vc, ImgSize *pSize);
void MipiRxDrvGetLvdsLaneImgsizeStatis(uint8_t devno, short lane, ImgSize *pSize);

void MipiRxDrvSetMipiIntMask(uint8_t devno);
void MipiRxDrvSetLvdsCtrlIntMask(uint8_t devno, unsigned int mask);
void MipiRxDrvSetMipiCtrlIntMask(uint8_t devno, unsigned int mask);
void MipiRxDrvSetMipiPkt1IntMask(uint8_t devno, unsigned int mask);
void MipiRxDrvSetMipiPkt2IntMask(uint8_t devno, unsigned int mask);
void MipiRxDrvSetMipiFrameIntMask(uint8_t devno, unsigned int mask);
void MipiRxDrvSetAlignIntMask(uint8_t devno, unsigned int mask);

void MipiRxDrvEnableClock(uint8_t comboDev);
void MipiRxDrvDisableClock(uint8_t comboDev);

void MipiRxDrvCoreReset(uint8_t comboDev);
void MipiRxDrvCoreUnreset(uint8_t comboDev);

int MipiRxDrvInit(void);
void MipiRxDrvExit(void);

#endif
