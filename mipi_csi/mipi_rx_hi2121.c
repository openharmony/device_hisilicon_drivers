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

#include "mipi_rx_hi2121.h"
#include "hdf_log.h"
#include "mipi_rx_reg.h"
#include "osal_io.h"
#include "osal_irq.h"
#include "osal_time.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

static unsigned long g_mipiRxCoreResetAddr;

typedef struct {
    unsigned int phy_rg_en_2121;
    unsigned int phy_rg_clk0_en;
    unsigned int phy_rg_clk1_en;
    unsigned int phy_rg_lp0_mode_en;
    unsigned int phy_rg_lp1_mode_en;
} PhyModeLinkTag;

typedef enum {
    MIPI_ESC_D0   = 0x1 << 0,
    MIPI_ESC_D1   = 0x1 << 1,
    MIPI_ESC_D2   = 0x1 << 2,
    MIPI_ESC_D3   = 0x1 << 3,
    MIPI_ESC_CLK0 = 0x1 << 4,
    MIPI_ESC_CLK1 = 0x1 << 5,

    MIPI_TIMEOUT_D0   = 0x1 << 8,
    MIPI_TIMEOUT_D1   = 0x1 << 9,
    MIPI_TIMEOUT_D2   = 0x1 << 10,
    MIPI_TIMEOUT_D3   = 0x1 << 11,
    MIPI_TIMEOUT_CLK0 = 0x1 << 12,
    MIPI_TIMEOUT_CLK1 = 0x1 << 13,
} PhyErrIntState;

typedef enum {
    MIPI_VC0_PKT_DATA_CRC = 0x1 << 0,     /* VC0'S data CRC error */
    MIPI_VC1_PKT_DATA_CRC = 0x1 << 1,
    MIPI_VC2_PKT_DATA_CRC = 0x1 << 2,
    MIPI_VC3_PKT_DATA_CRC = 0x1 << 3,

    MIPI_PKT_HEADER_ERR   = 0x1 << 16,    /* Header has two error at least ,and ECC error correction is invalid */
} MipiPktIntState;

typedef enum {
    MIPI_VC0_PKT_INVALID_DT = 0x1 << 0,   /* do not support VC0'S data type */
    MIPI_VC1_PKT_INVALID_DT = 0x1 << 1,   /* do not support VC1'S data type */
    MIPI_VC2_PKT_INVALID_DT = 0x1 << 2,   /* do not support VC2'S data type */
    MIPI_VC3_PKT_INVALID_DT = 0x1 << 3,   /* do not support VC3'S data type */

    MIPI_VC0_PKT_HEADER_ECC = 0x1 << 16,  /* VC0'S header has errors,and ECC error correction is ok */
    MIPI_VC1_PKT_HEADER_ECC = 0x1 << 17,
    MIPI_VC2_PKT_HEADER_ECC = 0x1 << 18,
    MIPI_VC3_PKT_HEADER_ECC = 0x1 << 19,
} MipiPkt2IntState;

typedef enum {
    MIPI_VC0_NO_MATCH  = 0x1 << 0,        /* VC0,frame's start and frame's end do not match */
    MIPI_VC1_NO_MATCH  = 0x1 << 1,        /* VC1,frame's start and frame's end do not match */
    MIPI_VC2_NO_MATCH  = 0x1 << 2,        /* VC2,frame's start and frame's end do not match */
    MIPI_VC3_NO_MATCH  = 0x1 << 3,        /* VC3,frame's start and frame's end do not match */

    MIPI_VC0_ORDER_ERR = 0x1 << 8,        /* VC0'S frame order error */
    MIPI_VC1_ORDER_ERR = 0x1 << 9,        /* VC1'S frame order error */
    MIPI_VC2_ORDER_ERR = 0x1 << 10,       /* VC2'S frame order error */
    MIPI_VC3_ORDER_ERR = 0x1 << 11,       /* VC3'S frame order error */

    MIPI_VC0_FRAME_CRC = 0x1 << 16,       /* in the last frame, VC0'S data has a CRC ERROR at least */
    MIPI_VC1_FRAME_CRC = 0x1 << 17,       /* in the last frame, VC1'S data has a CRC ERROR at least */
    MIPI_VC2_FRAME_CRC = 0x1 << 18,       /* in the last frame, VC2'S data has a CRC ERROR at least */
    MIPI_VC3_FRAME_CRC = 0x1 << 19,       /* in the last frame, VC3'S data has a CRC ERROR at least */
} MipiFrameIntState;

typedef enum {
    CMD_FIFO_WRITE_ERR  = 0x1 << 0,       /* MIPI_CTRL write command FIFO error */
    DATA_FIFO_WRITE_ERR = 0x1 << 1,
    CMD_FIFO_READ_ERR   = 0x1 << 16,
    DATA_FIFO_READ_ERR  = 0x1 << 17,
} MipiCtrlIntState;

typedef enum {
    LINK0_WRITE_ERR = 0x1 << 16,
    LINK0_READ_ERR  = 0x1 << 20,
    LVDS_STAT_ERR   = 0x1 << 24,
    LVDS_POP_ERR    = 0x1 << 25,
    CMD_WR_ERR      = 0x1 << 26,
    CMD_RD_ERR      = 0x1 << 27,
} LvdsIntState;

typedef enum {
    ALIGN_FIFO_FULL_ERR = 0x1 << 0,
    ALIGN_LANE0_ERR     = 0x1 << 1,
    ALIGN_LANE1_ERR     = 0x1 << 2,
    ALIGN_LANE2_ERR     = 0x1 << 3,
    ALIGN_LANE3_ERR     = 0x1 << 4,
} AlignIntState;

enum UserDefData {
    DEF0_DATA = 0,
    DEF1_DATA,
    DEF2_DATA,
    DEF3_DATA
};

#define HDF_LOG_TAG mipi_rx_hi2121
/* macro definition */
#define MIPI_RX_REGS_ADDR      0x113A0000
#define MIPI_RX_REGS_SIZE      0x10000

#define SNS_CRG_ADDR           0x120100F0
#define MIPI_RX_CRG_ADDR       0x120100F8

#define MIPI_RX_WORK_MODE_ADDR 0x12030018

#define MIPI_RX_IRQ            89

static unsigned int g_regMapFlag = 0;

#define SKEW_LINK              0x0
#define MIPI_FSMO_VALUE        0x000d1d0c
#define DOL_ID_CODE_OFFSET     (1 << 4)

/* global variables definition */
MipiRxRegsTypeTag *g_mipiRxRegsVa = NULL;

unsigned int g_mipiRxIrqNum = MIPI_RX_IRQ;

static const PhyModeLinkTag g_phyMode[][MIPI_RX_MAX_PHY_NUM] = {
    {{ 0, 1, 1, 1, 1 }},
    {{ 1, 1, 1, 1, 1 }},
};

PhyErrIntCnt g_phyErrIntCnt[MIPI_RX_MAX_PHY_NUM];
MipiErrIntCnt g_mipiErrIntCnt[MIPI_RX_MAX_DEV_NUM];
LvdsErrIntCnt g_lvdsErrIntCnt[MIPI_RX_MAX_DEV_NUM];
AlignErrIntCnt g_alignErrIntCnt[MIPI_RX_MAX_DEV_NUM];

PhyErrIntCnt *MipiRxHalGetPhyErrIntCnt(unsigned int phyId)
{
    return &g_phyErrIntCnt[phyId];
}

MipiErrIntCnt *MipiRxHalGetMipiErrInt(unsigned int phyId)
{
    return &g_mipiErrIntCnt[phyId];
}

LvdsErrIntCnt *MipiRxHalGetLvdsErrIntCnt(unsigned int phyId)
{
    return &g_lvdsErrIntCnt[phyId];
}

AlignErrIntCnt *MipiRxHalGetAlignErrIntCnt(unsigned int phyId)
{
    return &g_alignErrIntCnt[phyId];
}

/* function definition */
static void SetBit(unsigned long value, unsigned long offset, unsigned long *addr)
{
    unsigned long t, mask;

    mask = 1 << offset;
    t = OSAL_READL(addr);
    t &= ~mask;
    t |= (value << offset) & mask;
    OSAL_WRITEL(t, addr);
}

static void WriteReg32(unsigned long *addr, unsigned int value, unsigned int mask)
{
    unsigned int t;

    t = OSAL_READL(addr);
    t &= ~mask;
    t |= value & mask;

    OSAL_WRITEL(t, addr);
}

static MipiRxPhyCfgTag *GetMipiRxPhyRegs(unsigned int phyId)
{
    return &g_mipiRxRegsVa->mipiRxPhyCfg[phyId];
}

static MipiRxSysRegsTag *GetMipiRxSysRegs(void)
{
    return &g_mipiRxRegsVa->mipiRxSysRegs;
}

static  MipiCtrlRegsTag *GetMipiCtrlRegs(uint8_t devno)
{
    return &g_mipiRxRegsVa->mipiRxCtrlRegs[devno].mipiCtrlRegs;
}

static LvdsCtrlRegsTag *GetLvdsCtrlRegs(uint8_t devno)
{
    return &g_mipiRxRegsVa->mipiRxCtrlRegs[devno].lvdsCtrlRegs;
}

void MipiRxSetCilIntMask(unsigned int phyId, unsigned int mask)
{
    U_MIPI_INT_MSK mipiIntMsk;
    volatile MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    volatile MipiRxSysRegsTag *mipiRxSysRegs = GetMipiRxSysRegs();

    mipiIntMsk.u32 = mipiRxSysRegs->MIPI_INT_MSK.u32;

    if (phyId == 0) {
        mipiIntMsk.bits.int_phycil0_mask = 0x0;
    }

    mipiRxSysRegs->MIPI_INT_MSK.u32 = mipiIntMsk.u32;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    mipiRxPhyCfg->MIPI_CIL_INT_MSK_LINK.u32 = mask;
}

static void MipiRxSetPhySkewLink(unsigned int phyId, unsigned int value)
{
    volatile U_PHY_SKEW_LINK phySkewLink;
    volatile MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phySkewLink.u32 = value;
    mipiRxPhyCfg->PHY_SKEW_LINK.u32 = phySkewLink.u32;
}

void MipiRxSetPhyFsmoLink(unsigned int phyId, unsigned int value)
{
    volatile MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    mipiRxPhyCfg->CIL_FSM0_LINK.u32 = value;
}

void MipiRxSetPhyRg2121En(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_en_2l2l = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxSetPhyRgClk0En(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_faclk0_en = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxSetPhyRgClk1En(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_faclk1_en = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxSetPhyRgLp0ModeEn(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_en_lp0 = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxSetPhyRgLp1ModeEn(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_en_lp1 = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxDrvSetWorkMode(uint8_t devno, InputMode inputMode)
{
    unsigned long *mipiRxWorkModeAddr = (unsigned long *)OsalIoRemap(MIPI_RX_WORK_MODE_ADDR, (unsigned long)0x4);

    if (mipiRxWorkModeAddr == 0) {
        HDF_LOGE("%s: MipiRx work mode reg ioremap failed!", __func__);
        return;
    }

    if (inputMode == INPUT_MODE_MIPI) {
        WriteReg32(mipiRxWorkModeAddr, 0x0 << (2 * devno), 0x1 << (2 * devno)); /* 2 bit, [1:0] */
    } else if ((inputMode == INPUT_MODE_SUBLVDS) ||
        (inputMode == INPUT_MODE_LVDS) || (inputMode == INPUT_MODE_HISPI)) {
        WriteReg32(mipiRxWorkModeAddr, 0x1 << (2 * devno), 0x1 << (2 * devno)); /* 2 bit, [1:0] */
    } else {
    }

    OsalIoUnmap((void *)mipiRxWorkModeAddr);
}

void MipiRxDrvSetMipiImageRect(uint8_t devno, const ImgRect *pImgRect)
{
    U_MIPI_CROP_START_CHN0 cropStartChn0;
    U_MIPI_CROP_START_CHN1 cropStartChn1;
    U_MIPI_CROP_START_CHN2 cropStartChn2;
    U_MIPI_CROP_START_CHN3 cropStartChn3;
    U_MIPI_IMGSIZE mipiImgsize;

    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    cropStartChn0.u32 = mipiCtrlRegs->MIPI_CROP_START_CHN0.u32;
    cropStartChn1.u32 = mipiCtrlRegs->MIPI_CROP_START_CHN1.u32;
    cropStartChn2.u32 = mipiCtrlRegs->MIPI_CROP_START_CHN2.u32;
    cropStartChn3.u32 = mipiCtrlRegs->MIPI_CROP_START_CHN3.u32;
    mipiImgsize.u32 = mipiCtrlRegs->MIPI_IMGSIZE.u32;

    mipiImgsize.bits.mipi_imgwidth = pImgRect->width - 1;
    mipiImgsize.bits.mipi_imgheight = pImgRect->height - 1;

    cropStartChn0.bits.mipi_start_x_chn0 = pImgRect->x;
    cropStartChn0.bits.mipi_start_y_chn0 = pImgRect->y;

    cropStartChn1.bits.mipi_start_x_chn1 = pImgRect->x;
    cropStartChn1.bits.mipi_start_y_chn1 = pImgRect->y;

    cropStartChn2.bits.mipi_start_x_chn2 = pImgRect->x;
    cropStartChn2.bits.mipi_start_y_chn2 = pImgRect->y;

    cropStartChn3.bits.mipi_start_x_chn3 = pImgRect->x;
    cropStartChn3.bits.mipi_start_y_chn3 = pImgRect->y;

    mipiCtrlRegs->MIPI_CROP_START_CHN0.u32 = cropStartChn0.u32;
    mipiCtrlRegs->MIPI_CROP_START_CHN1.u32 = cropStartChn1.u32;
    mipiCtrlRegs->MIPI_CROP_START_CHN2.u32 = cropStartChn2.u32;
    mipiCtrlRegs->MIPI_CROP_START_CHN3.u32 = cropStartChn3.u32;
    mipiCtrlRegs->MIPI_IMGSIZE.u32 = mipiImgsize.u32;
}

void MipiRxDrvSetMipiCropEn(uint8_t devno, int enable)
{
    U_MIPI_CTRL_MODE_PIXEL mipiCtrlModePixel;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlModePixel.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32;
    mipiCtrlModePixel.bits.crop_en = enable;
    mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32 = mipiCtrlModePixel.u32;
}

short MipiRxDrvGetDataType(DataType inputDataType)
{
    if (inputDataType == DATA_TYPE_RAW_8BIT) {
        return 0x2a;
    } else if (inputDataType == DATA_TYPE_RAW_10BIT) {
        return 0x2b;
    } else if (inputDataType == DATA_TYPE_RAW_12BIT) {
        return 0x2c;
    } else if (inputDataType == DATA_TYPE_RAW_14BIT) {
        return 0x2d;
    } else if (inputDataType == DATA_TYPE_RAW_16BIT) {
        return 0x2e;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_NORMAL) {
        return 0x18;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_LEGACY) {
        return 0x1a;
    } else if (inputDataType == DATA_TYPE_YUV422_8BIT) {
        return 0x1e;
    } else if (inputDataType == DATA_TYPE_YUV422_PACKED) {
        return 0x1e;
    } else {
        return 0x0;
    }
}

short MipiRxDrvGetDataBitWidth(DataType inputDataType)
{
    if (inputDataType == DATA_TYPE_RAW_8BIT) {
        return 0x0;
    } else if (inputDataType == DATA_TYPE_RAW_10BIT) {
        return 0x1;
    } else if (inputDataType == DATA_TYPE_RAW_12BIT) {
        return 0x2;
    } else if (inputDataType == DATA_TYPE_RAW_14BIT) {
        return 0x3;
    } else if (inputDataType == DATA_TYPE_RAW_16BIT) {
        return 0x4;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_NORMAL) {
        return 0x0;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_LEGACY) {
        return 0x0;
    } else if (inputDataType == DATA_TYPE_YUV422_8BIT) {
        return 0x0;
    } else if (inputDataType == DATA_TYPE_YUV422_PACKED) {
        return 0x4;
    } else {
        return 0x0;
    }
}

/* magic num mean bit width, convert to register condfig */
short MipiRxDrvGetExtDataBitWidth(unsigned int extDataBitWidth)
{
    if (extDataBitWidth == 8) { /* 8:magic bit width */
        return 0x0;
    } else if (extDataBitWidth == 10) { /* 10:magic bit width */
        return 0x1;
    } else if (extDataBitWidth == 12) { /* 12:magic bit width */
        return 0x2;
    } else if (extDataBitWidth == 14) { /* 14:magic bit width */
        return 0x3;
    } else if (extDataBitWidth == 16) { /* 16:magic bit width */
        return 0x4;
    } else {
        return 0x0;
    }
}

void MipiRxDrvSetDiDt(uint8_t devno, DataType inputDataType)
{
    U_MIPI_DI_1 mipiDi1;
    U_MIPI_DI_2 mipiDi2;
    unsigned int tempDataType;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    tempDataType = MipiRxDrvGetDataType(inputDataType);

    mipiDi1.u32 = mipiCtrlRegs->MIPI_DI_1.u32;
    mipiDi1.bits.di0_dt = tempDataType;
    mipiDi1.bits.di1_dt = tempDataType;
    mipiDi1.bits.di2_dt = tempDataType;
    mipiDi1.bits.di3_dt = tempDataType;
    mipiCtrlRegs->MIPI_DI_1.u32 = mipiDi1.u32;

    mipiDi2.u32 = mipiCtrlRegs->MIPI_DI_2.u32;
    mipiDi2.bits.di4_dt = tempDataType;
    mipiDi2.bits.di5_dt = tempDataType;
    mipiDi2.bits.di6_dt = tempDataType;
    mipiDi2.bits.di7_dt = tempDataType;
    mipiCtrlRegs->MIPI_DI_2.u32 = mipiDi2.u32;
}

static void MipiRxDrvSetModePixel(uint8_t devno, DataType inputDataType)
{
    U_MIPI_CTRL_MODE_PIXEL mipiCtrlModePixel;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlModePixel.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32;

    if (inputDataType == DATA_TYPE_YUV420_8BIT_NORMAL) {
        mipiCtrlModePixel.bits.mipi_yuv420_even_detect = 1;
        mipiCtrlModePixel.bits.mipi_yuv_420_nolegacy_en = 1;
        mipiCtrlModePixel.bits.mipi_yuv_420_legacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_422_en = 0;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_LEGACY) {
        mipiCtrlModePixel.bits.mipi_yuv_420_nolegacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_420_legacy_en = 1;
        mipiCtrlModePixel.bits.mipi_yuv_422_en = 0;
    } else if (inputDataType == DATA_TYPE_YUV422_8BIT) {
        mipiCtrlModePixel.bits.mipi_yuv_420_nolegacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_420_legacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_422_en = 1;
    } else {
        mipiCtrlModePixel.bits.mipi_yuv_420_nolegacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_420_legacy_en = 0;
        mipiCtrlModePixel.bits.mipi_yuv_422_en = 0;
    }

    mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32 = mipiCtrlModePixel.u32;
}

static void MipiRxDrvSetUserDef(uint8_t devno, DataType inputDataType)
{
    unsigned char bitWidth;
    unsigned int tempDataType;
    U_MIPI_USERDEF_DT userDefDt;
    U_MIPI_USER_DEF userDef;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if (inputDataType == DATA_TYPE_YUV420_8BIT_NORMAL) {
        bitWidth = 0;
        tempDataType = 0x18;
    } else if (inputDataType == DATA_TYPE_YUV420_8BIT_LEGACY) {
        bitWidth = 0;
        tempDataType = 0x1a;
    } else if (inputDataType == DATA_TYPE_YUV422_8BIT) {
        bitWidth = 0; /* 0 -- 8bit */
        tempDataType = 0x1e;
    } else if (inputDataType == DATA_TYPE_YUV422_PACKED) {
        bitWidth = 4; /* 4 -- 16bit */
        tempDataType = 0x1e;
    } else {
        return;
    }

    userDefDt.u32 = mipiCtrlRegs->MIPI_USERDEF_DT.u32;
    userDef.u32 = mipiCtrlRegs->MIPI_USER_DEF.u32;

    userDefDt.bits.user_def0_dt = bitWidth;
    userDefDt.bits.user_def1_dt = bitWidth;
    userDefDt.bits.user_def2_dt = bitWidth;
    userDefDt.bits.user_def3_dt = bitWidth;

    userDef.bits.user_def0 = tempDataType;
    userDef.bits.user_def1 = tempDataType;
    userDef.bits.user_def2 = tempDataType;
    userDef.bits.user_def3 = tempDataType;

    mipiCtrlRegs->MIPI_USERDEF_DT.u32 = userDefDt.u32;
    mipiCtrlRegs->MIPI_USER_DEF.u32 = userDef.u32;

    return;
}

static void MipiRxDrvCtrlModeHs(uint8_t devno, DataType inputDataType)
{
    unsigned char userDefEn;
    U_MIPI_CTRL_MODE_HS mipiCtrlModeHs;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if ((inputDataType == DATA_TYPE_YUV420_8BIT_NORMAL) ||
        (inputDataType == DATA_TYPE_YUV420_8BIT_LEGACY) ||
        (inputDataType == DATA_TYPE_YUV422_8BIT) ||
        (inputDataType == DATA_TYPE_YUV422_PACKED)) {
        userDefEn = 1;
    } else {
        userDefEn = 0;
    }

    mipiCtrlModeHs.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32;

    mipiCtrlModeHs.bits.user_def_en = userDefEn;

    mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32 = mipiCtrlModeHs.u32;
}

void MipiRxDrvSetMipiUserDt(uint8_t devno, int index, short dataType, short bitWidth)
{
    U_MIPI_USERDEF_DT userDefDt;
    U_MIPI_USER_DEF userDef;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if (mipiCtrlRegs != NULL) {
        userDefDt.u32 = mipiCtrlRegs->MIPI_USERDEF_DT.u32;
        userDef.u32 = mipiCtrlRegs->MIPI_USER_DEF.u32;

        if (index == DEF0_DATA) {
            userDefDt.bits.user_def0_dt = bitWidth;
            userDef.bits.user_def0 = dataType;
        } else if (index == DEF1_DATA) {
            userDefDt.bits.user_def1_dt = bitWidth;
            userDef.bits.user_def1 = dataType;
        } else if (index == DEF2_DATA) {
            userDefDt.bits.user_def2_dt = bitWidth;
            userDef.bits.user_def2 = dataType;
        } else if (index == DEF3_DATA) {
            userDefDt.bits.user_def3_dt = bitWidth;
            userDef.bits.user_def3 = dataType;
        }

        mipiCtrlRegs->MIPI_USERDEF_DT.u32 = userDefDt.u32;
        mipiCtrlRegs->MIPI_USER_DEF.u32 = userDef.u32;
    }
}

void MipiRxDrvSetMipiYuvDt(uint8_t devno, DataType inputDataType)
{
    MipiRxDrvCtrlModeHs(devno, inputDataType);
    MipiRxDrvSetUserDef(devno, inputDataType);
    MipiRxDrvSetModePixel(devno, inputDataType);
}

void MipiRxDrvSetMipiWdrUserDt(uint8_t devno, DataType inputDataType, const short dataType[WDR_VC_NUM])
{
    U_MIPI_USERDEF_DT userDefDt;
    U_MIPI_USER_DEF   userDef;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    userDefDt.u32 = mipiCtrlRegs->MIPI_USERDEF_DT.u32;
    userDef.u32 = mipiCtrlRegs->MIPI_USER_DEF.u32;

    userDefDt.bits.user_def0_dt = inputDataType;
    userDefDt.bits.user_def1_dt = inputDataType;

    userDef.bits.user_def0 = dataType[0];
    userDef.bits.user_def1 = dataType[1];

    mipiCtrlRegs->MIPI_USERDEF_DT.u32 = userDefDt.u32;
    mipiCtrlRegs->MIPI_USER_DEF.u32 = userDef.u32;
}

void MipiRxDrvSetMipiDolId(uint8_t devno, DataType inputDataType, const short dolId[])
{
    U_MIPI_DOL_ID_CODE0 dolId0;
    U_MIPI_DOL_ID_CODE1 dolId1;
    U_MIPI_DOL_ID_CODE2 dolId2;
    short lef, sef1, sef2;
    short nxtLef, nxtSef1, nxtSef2;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    (void)inputDataType;
    (void)dolId;
    dolId0.u32 = mipiCtrlRegs->MIPI_DOL_ID_CODE0.u32;
    dolId1.u32 = mipiCtrlRegs->MIPI_DOL_ID_CODE1.u32;
    dolId2.u32 = mipiCtrlRegs->MIPI_DOL_ID_CODE2.u32;

    lef = 0x241;
    sef1 = 0x242;
    sef2 = 0x244;

    nxtLef = lef  + DOL_ID_CODE_OFFSET; /* 4:The LEF frame ID code of the N+1 frame is larger than the LEF frame
                                         * ID code of the Nth frame(1 << 4)
                                         */
    nxtSef1 = sef1 + DOL_ID_CODE_OFFSET; /* 4:The SEF1 frame ID code of the N+1 frame is larger than the LEF frame
                                          * ID code of the Nth frame(1 << 4)
                                          */
    nxtSef2 = sef2 + DOL_ID_CODE_OFFSET; /* 4:The SEF2 frame ID code of the N+1 frame is larger than the LEF frame
                                          * ID code of the Nth frame(1 << 4)
                                          */

    dolId0.bits.id_code_reg0 = lef;
    dolId0.bits.id_code_reg1 = sef1;
    dolId1.bits.id_code_reg2 = sef2;

    dolId1.bits.id_code_reg3 = nxtLef;
    dolId2.bits.id_code_reg4 = nxtSef1;
    dolId2.bits.id_code_reg5 = nxtSef2;

    mipiCtrlRegs->MIPI_DOL_ID_CODE0.u32 = dolId0.u32;
    mipiCtrlRegs->MIPI_DOL_ID_CODE1.u32 = dolId1.u32;
    mipiCtrlRegs->MIPI_DOL_ID_CODE2.u32 = dolId2.u32;
}

void MipiRxDrvSetMipiWdrMode(uint8_t devno, MipiWdrMode wdrMode)
{
    U_MIPI_CTRL_MODE_HS modeHs;
    U_MIPI_CTRL_MODE_PIXEL modePixel;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    modeHs.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32;
    modePixel.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32;

    if (wdrMode == HI_MIPI_WDR_MODE_NONE) {
        modePixel.bits.mipi_dol_mode = 0;
    }

    if (wdrMode == HI_MIPI_WDR_MODE_VC) {
        modePixel.bits.mipi_dol_mode = 0;
    } else if (wdrMode == HI_MIPI_WDR_MODE_DT) {
        modeHs.bits.user_def_en = 1;
    } else if (wdrMode == HI_MIPI_WDR_MODE_DOL) {
        modePixel.bits.mipi_dol_mode = 1;
    }

    mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32 = modeHs.u32;
    mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32 = modePixel.u32;
}

void MipiRxDrvEnableUserDefineDt(uint8_t devno, int enable)
{
    U_MIPI_CTRL_MODE_HS modeHs;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if (mipiCtrlRegs != NULL) {
        modeHs.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32;
        modeHs.bits.user_def_en = enable;

        mipiCtrlRegs->MIPI_CTRL_MODE_HS.u32 = modeHs.u32;
    }
}

void MipiRxDrvSetExtDataType(const ExtDataType* dataType, DataType inputDataType)
{
    unsigned int i;
    uint8_t devno;
    short inputDt;
    short bitWidth;
    short extBitWidth;

    devno = dataType->devno;
    inputDt = MipiRxDrvGetDataType(inputDataType);
    bitWidth = MipiRxDrvGetDataBitWidth(inputDataType);

    MipiRxDrvSetMipiUserDt(devno, 0, inputDt, bitWidth);

    for (i = 0; i < dataType->num; i++) {
        extBitWidth = MipiRxDrvGetExtDataBitWidth(dataType->extDataBitWidth[i]);
        MipiRxDrvSetMipiUserDt(devno, i + 1, dataType->extDataType[i], extBitWidth);
    }

    MipiRxDrvEnableUserDefineDt(devno, 1);
}

unsigned int MipiRxDrvGetPhyData(int phyId, int laneId)
{
    volatile U_PHY_DATA_LINK phyDataLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    unsigned int laneData = 0x0;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyDataLink.u32 = mipiRxPhyCfg->PHY_DATA_LINK.u32;

    if (laneId == 0) { /* 0 -- laneId 0 */
        laneData = phyDataLink.bits.phy_data0_mipi;
    } else if (laneId == 1) { /* 1 -- laneId 1 */
        laneData = phyDataLink.bits.phy_data1_mipi;
    } else if (laneId == 2) { /* 2 -- laneId 2 */
        laneData = phyDataLink.bits.phy_data2_mipi;
    } else if (laneId == 3) { /* 3 -- laneId 3 */
        laneData = phyDataLink.bits.phy_data3_mipi;
    }

    return laneData;
}

unsigned int MipiRxDrvGetPhyMipiLinkData(int phyId, int laneId)
{
    volatile U_PHY_DATA_MIPI_LINK phyDataMipiLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    unsigned int laneData = 0x0;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyDataMipiLink.u32 = mipiRxPhyCfg->PHY_DATA_MIPI_LINK.u32;

    if (laneId == 0) { /* 0 -- laneId 0 */
        laneData = phyDataMipiLink.bits.phy_data0_mipi_hs;
    } else if (laneId == 1) { /* 1 -- laneId 1 */
        laneData = phyDataMipiLink.bits.phy_data1_mipi_hs;
    } else if (laneId == 2) { /* 2 -- laneId 2 */
        laneData = phyDataMipiLink.bits.phy_data2_mipi_hs;
    } else if (laneId == 3) { /* 3 -- laneId 3 */
        laneData = phyDataMipiLink.bits.phy_data3_mipi_hs;
    }

    return laneData;
}


unsigned int MipiRxDrvGetPhyLvdsLinkData(int phyId, int laneId)
{
    volatile U_PHY_DATA_LVDS_LINK phyDataLvdsLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    unsigned int laneData = 0x0;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyDataLvdsLink.u32 = mipiRxPhyCfg->PHY_DATA_LVDS_LINK.u32;

    if (laneId == 0) { /* 0 -- laneId 0 */
        laneData = phyDataLvdsLink.bits.phy_data0_lvds_hs;
    } else if (laneId == 1) { /* 1 -- laneId 1 */
        laneData = phyDataLvdsLink.bits.phy_data1_lvds_hs;
    } else if (laneId == 2) { /* 2 -- laneId 2 */
        laneData = phyDataLvdsLink.bits.phy_data2_lvds_hs;
    } else if (laneId == 3) { /* 3 -- laneId 3 */
        laneData = phyDataLvdsLink.bits.phy_data3_lvds_hs;
    }

    return laneData;
}

void MipiRxDrvSetDataRate(uint8_t devno, MipiDataRate dataRate)
{
    U_MIPI_CTRL_MODE_PIXEL mipiCtrlModePixel;
    unsigned int mipiDoublePixEn = 0;
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if (dataRate == MIPI_DATA_RATE_X1) {
        mipiDoublePixEn = 0;
    } else if (dataRate == MIPI_DATA_RATE_X2) {
        mipiDoublePixEn = 1;
    } else {
        HDF_LOGE("%s: unsupported  dataRate:%d  devno %d", __func__, dataRate, devno);
        return;
    }

    mipiCtrlModePixel.u32 = mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32;
    mipiCtrlModePixel.bits.mipi_double_pix_en = mipiDoublePixEn;
    mipiCtrlModePixel.bits.sync_clear_en = 0x1;
    mipiCtrlRegs->MIPI_CTRL_MODE_PIXEL.u32 = mipiCtrlModePixel.u32;
}

void MipiRxSetLaneId(uint8_t devno, int laneIdx, short laneId,
    unsigned int laneBitmap, LaneDivideMode mode)
{
    U_LANE_ID0_CHN laneId0Ch0;

    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    laneId0Ch0.u32 = lvdsCtrlRegs->LANE_ID0_CHN0.u32;

    switch (laneId) {
        case 0: /* 0 -- laneId 0 */
            laneId0Ch0.bits.lane0_id = laneIdx;
            break;

        case 1: /* 1 -- laneId 1 */
            laneId0Ch0.bits.lane1_id = laneIdx;
            break;

        case 2: /* 2 -- laneId 2 */
            laneId0Ch0.bits.lane2_id = laneIdx;
            break;

        case 3: /* 3 -- laneId 3 */
            laneId0Ch0.bits.lane3_id = laneIdx;
            break;

        default:
            break;
    }

    if (laneBitmap == 0xa && mode == 1 && devno == 1) {
        laneId0Ch0.u32 = 0x3210;
    }

    lvdsCtrlRegs->LANE_ID0_CHN0.u32 = laneId0Ch0.u32;
}

void MipiRxDrvSetLinkLaneId(uint8_t devno, InputMode inputMode, const short *pLaneId,
    unsigned int laneBitmap, LaneDivideMode mode)
{
    int i;
    int laneNum;

    if (inputMode == INPUT_MODE_MIPI) {
        laneNum = MIPI_LANE_NUM;
    } else {
        laneNum = LVDS_LANE_NUM;
    }

    for (i = 0; i < laneNum; i++) {
        if (pLaneId[i] != -1) {
            MipiRxSetLaneId(devno, i, pLaneId[i], laneBitmap, mode);
        }
    }
}

void MipiRxDrvSetMemCken(uint8_t devno, int enable)
{
    U_CHN0_MEM_CTRL chn0MemCtrl;
    U_CHN1_MEM_CTRL chn1MemCtrl;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();

    switch (devno) {
        case 0: /* 0 -- mipi dev 0 */
            chn0MemCtrl.u32 = mipiRxSysRegs->CHN0_MEM_CTRL.u32;
            chn0MemCtrl.bits.chn0_mem_ck_gt = enable;
            mipiRxSysRegs->CHN0_MEM_CTRL.u32 = chn0MemCtrl.u32;
            break;
        case 1: /* 1 -- mipi dev 1 */
            chn1MemCtrl.u32 = mipiRxSysRegs->CHN1_MEM_CTRL.u32;
            chn1MemCtrl.bits.chn1_mem_ck_gt = enable;
            mipiRxSysRegs->CHN1_MEM_CTRL.u32 = chn1MemCtrl.u32;
            break;
        default:
            break;
    }
}

void MipiRxDrvSetClrCken(uint8_t devno, int enable)
{
    U_CHN0_CLR_EN chn0ClrEn;
    U_CHN1_CLR_EN chn1ClrEn;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();

    switch (devno) {
        case 0: /* 0 -- mipi dev 0 */
            chn0ClrEn.u32 = mipiRxSysRegs->CHN0_CLR_EN.u32;
            chn0ClrEn.bits.chn0_clr_en_lvds = enable;
            chn0ClrEn.bits.chn0_clr_en_align = enable;
            mipiRxSysRegs->CHN0_CLR_EN.u32 = chn0ClrEn.u32;
            break;
        case 1: /* 1 -- mipi dev 1 */
            chn1ClrEn.u32 = mipiRxSysRegs->CHN1_CLR_EN.u32;
            chn1ClrEn.bits.chn1_clr_en_lvds = enable;
            chn1ClrEn.bits.chn1_clr_en_align = enable;
            mipiRxSysRegs->CHN1_CLR_EN.u32 = chn1ClrEn.u32;
            break;

        default:
            break;
    }
}

void MipiRxDrvSetLaneNum(uint8_t devno, unsigned int laneNum)
{
    U_MIPI_LANES_NUM mipiLanesNum;
    MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiLanesNum.u32 = mipiCtrlRegs->MIPI_LANES_NUM.u32;
    mipiLanesNum.bits.lane_num = laneNum - 1;
    mipiCtrlRegs->MIPI_LANES_NUM.u32 = mipiLanesNum.u32;
}

void MipiRxDrvSetPhyEnLink(unsigned int phyId, unsigned int laneBitmap)
{
    U_PHY_EN_LINK phyEnLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyEnLink.u32 = mipiRxPhyCfg->PHY_EN_LINK.u32;

    if (laneBitmap & 0x5) {
        phyEnLink.bits.phy_da_d0_valid = laneBitmap & 0x1; /* 0 -- lane0 */
        phyEnLink.bits.phy_da_d2_valid = (laneBitmap & 0x4) >> 2; /* 2 -- lane2 */
        phyEnLink.bits.phy_d0_term_en = laneBitmap & 0x1;
        phyEnLink.bits.phy_d2_term_en = (laneBitmap & 0x4) >> 2; /* 2 -- lane2 */
        phyEnLink.bits.phy_clk0_term_en = 1;
    }

    if (laneBitmap & 0xa) {
        phyEnLink.bits.phy_da_d1_valid = (laneBitmap & 0x2) >> 1; /* 1 -- lane1 */
        phyEnLink.bits.phy_da_d3_valid = (laneBitmap & 0x8) >> 3; /* 3 -- lane3 */
        phyEnLink.bits.phy_d1_term_en = (laneBitmap & 0x2) >> 1; /* 1 -- lane1 */
        phyEnLink.bits.phy_d3_term_en = (laneBitmap & 0x8) >> 3; /* 3 -- lane3 */
        phyEnLink.bits.phy_clk1_term_en = 1;
    }

    mipiRxPhyCfg->PHY_EN_LINK.u32 = phyEnLink.u32;
}

void MipiRxDrvSetPhyMode(unsigned int phyId, InputMode inputMode, unsigned int laneBitmap)
{
    U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    int cmosEn = 0;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;

    if (inputMode == INPUT_MODE_CMOS ||
        inputMode == INPUT_MODE_BT1120 ||
        inputMode == INPUT_MODE_BYPASS) {
        cmosEn = 1;
    }

    phyModeLink.bits.phy0_rg_en_d = phyModeLink.bits.phy0_rg_en_d | (laneBitmap & 0xf);
    phyModeLink.bits.phy0_rg_en_cmos = cmosEn;
    phyModeLink.bits.phy0_rg_en_clk0 = 1;
    phyModeLink.bits.phy0_rg_mipi_mode0 = 1;

    if (laneBitmap & 0xa) {
        phyModeLink.bits.phy0_rg_en_clk1 = 1;
        phyModeLink.bits.phy0_rg_mipi_mode1 = 1;
    }

    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxDrvSetCmosEn(unsigned int phyId, int enable)
{
    volatile U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;
    phyModeLink.bits.phy0_rg_en_cmos = enable;
    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxDrvSetPhyEn(unsigned int laneBitmap)
{
    U_PHY_EN phyEn;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();
    phyEn.u32 = mipiRxSysRegs->PHY_EN.u32;

    if (laneBitmap & 0xf) {
        phyEn.bits.phy0_en = 1;
    }

    mipiRxSysRegs->PHY_EN.u32 = phyEn.u32;
}

void MipiRxDrvSetLaneEn(unsigned int laneBitmap)
{
    U_LANE_EN laneEn;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();
    laneEn.u32 = mipiRxSysRegs->LANE_EN.u32;
    laneEn.u32 = laneEn.u32 | (laneBitmap & 0xf);
    mipiRxSysRegs->LANE_EN.u32 = laneEn.u32;
}

void MipiRxDrvSetPhyCilEn(unsigned int laneBitmap, int enable)
{
    U_PHY_CIL_CTRL phyCilCtrl;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();
    phyCilCtrl.u32 = mipiRxSysRegs->PHY_CIL_CTRL.u32;

    if (laneBitmap & 0xf) {
        phyCilCtrl.bits.phycil0_cken = enable;
    }

    mipiRxSysRegs->PHY_CIL_CTRL.u32 = phyCilCtrl.u32;
}

void MipiRxDrvSetPhyCfgMode(InputMode inputMode, unsigned int laneBitmap)
{
    U_PHYCFG_MODE phycfgMode;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;
    unsigned int cfgMode;
    unsigned int cfgModeSel;

    mipiRxSysRegs = GetMipiRxSysRegs();
    phycfgMode.u32 = mipiRxSysRegs->PHYCFG_MODE.u32;

    if (inputMode == INPUT_MODE_MIPI) {
        cfgMode = 0;
        cfgModeSel = 0;
    } else if (inputMode == INPUT_MODE_SUBLVDS ||
        inputMode == INPUT_MODE_LVDS ||
        inputMode == INPUT_MODE_HISPI) {
        cfgMode = 1;
        cfgModeSel = 0; /* RAW */
    } else {
        cfgMode = 2; /* 2 -- PHY cfg is controlled by register value */
        cfgModeSel = 1; /* CMOS */
    }

    if (laneBitmap & 0xf) {
        phycfgMode.bits.phycil0_0_cfg_mode = cfgMode;
        phycfgMode.bits.phycil0_1_cfg_mode = cfgMode;
        phycfgMode.bits.phycil0_cfg_mode_sel = cfgModeSel;
    } else if (laneBitmap & 0x5) {
        phycfgMode.bits.phycil0_0_cfg_mode = cfgMode;
        phycfgMode.bits.phycil0_cfg_mode_sel = cfgModeSel;
    } else if (laneBitmap & 0xa) {
        phycfgMode.bits.phycil0_1_cfg_mode = cfgMode;
        phycfgMode.bits.phycil0_cfg_mode_sel = cfgModeSel;
    }

    mipiRxSysRegs->PHYCFG_MODE.u32 = phycfgMode.u32;
}

void MipiRxDrvSetPhyCfgEn(unsigned int laneBitmap, int enable)
{
    U_PHYCFG_EN phycfgEn;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;

    mipiRxSysRegs = GetMipiRxSysRegs();
    phycfgEn.u32 = mipiRxSysRegs->PHYCFG_EN.u32;

    if (laneBitmap & 0xf) {
        phycfgEn.bits.phycil0_cfg_en = enable;
    }

    mipiRxSysRegs->PHYCFG_EN.u32 = phycfgEn.u32;
}

void MipiRxDrvSetPhyConfig(InputMode inputMode, unsigned int laneBitmap)
{
    unsigned int i;
    unsigned int mask;
    unsigned int phyLaneBitmap;

    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; i++) {
        mask = 0xf << (4 * i); /* 4 -- 4bit */
        if (laneBitmap & mask) {
            phyLaneBitmap = (laneBitmap & mask) >> (4 * i); /* 4 -- 4bit */
            MipiRxDrvSetPhyEnLink(i, phyLaneBitmap);
            MipiRxDrvSetPhyMode(i, inputMode, phyLaneBitmap);
        }
    }

    MipiRxDrvSetPhyEn(laneBitmap);
    MipiRxDrvSetLaneEn(laneBitmap);
    MipiRxDrvSetPhyCilEn(laneBitmap, 1);
    MipiRxDrvSetPhyCfgMode(inputMode, laneBitmap);
    MipiRxDrvSetPhyCfgEn(laneBitmap, 1);
}

static void MipiRxDrvSetPhyCmv(unsigned int phyId, PhyCmvMode cmvMode, unsigned int laneBitmap)
{
    int mipiCmvMode = 0;
    U_PHY_MODE_LINK phyModeLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    if (cmvMode == PHY_CMV_GE1200MV) {
        mipiCmvMode = 0;
    } else if (cmvMode == PHY_CMV_LT1200MV) {
        mipiCmvMode = 1;
    }

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyModeLink.u32 = mipiRxPhyCfg->PHY_MODE_LINK.u32;

    if (laneBitmap & 0xa) {
        phyModeLink.bits.phy0_rg_mipi_mode1 = mipiCmvMode;
    }

    if (laneBitmap & 0x5) {
        phyModeLink.bits.phy0_rg_mipi_mode0 = mipiCmvMode;
    }

    mipiRxPhyCfg->PHY_MODE_LINK.u32 = phyModeLink.u32;
}

void MipiRxDrvSetPhyCmvmode(InputMode inputMode, PhyCmvMode cmvMode, unsigned int laneBitmap)
{
    unsigned int i;
    unsigned int mask;
    unsigned int phyLaneBitmap;

    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; i++) {
        mask = 0xf << (4 * i); /* 4 -- 4bit */
        if (laneBitmap & mask) {
            phyLaneBitmap = (laneBitmap & mask) >> (4 * i); /* 4 -- 4bit */
            MipiRxDrvSetPhyCmv(i, cmvMode, phyLaneBitmap);
        }
    }

    MipiRxDrvSetPhyCfgMode(inputMode, laneBitmap);
    MipiRxDrvSetPhyCfgEn(laneBitmap, 1);
}

void MipiRxDrvSetLvdsImageRect(uint8_t devno, const ImgRect *pImgRect, short totalLaneNum)
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    U_LVDS_IMGSIZE lvdsImgSize;
    U_LVDS_CROP_START0 cropStart0;
    U_LVDS_CROP_START1 cropStart1;
    U_LVDS_CROP_START2 cropStart2;
    U_LVDS_CROP_START3 cropStart3;
    unsigned int widthPerLane, xPerLane;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (totalLaneNum == 0) {
        return;
    }

    widthPerLane = (pImgRect->width / totalLaneNum);
    xPerLane = (pImgRect->x / totalLaneNum);

    lvdsImgSize.u32 = ctrlReg->LVDS_IMGSIZE.u32;
    cropStart0.u32 = ctrlReg->LVDS_CROP_START0.u32;
    cropStart1.u32 = ctrlReg->LVDS_CROP_START1.u32;
    cropStart2.u32 = ctrlReg->LVDS_CROP_START2.u32;
    cropStart3.u32 = ctrlReg->LVDS_CROP_START3.u32;

    lvdsImgSize.bits.lvds_imgwidth_lane = widthPerLane - 1;
    lvdsImgSize.bits.lvds_imgheight = pImgRect->height - 1;

    cropStart0.bits.lvds_start_x0_lane = xPerLane;
    cropStart0.bits.lvds_start_y0 = pImgRect->y;

    cropStart1.bits.lvds_start_x1_lane = xPerLane;
    cropStart1.bits.lvds_start_y1 = pImgRect->y;

    cropStart2.bits.lvds_start_x2_lane = xPerLane;
    cropStart2.bits.lvds_start_y2 = pImgRect->y;

    cropStart3.bits.lvds_start_x3_lane = xPerLane;
    cropStart3.bits.lvds_start_y3 = pImgRect->y;

    ctrlReg->LVDS_IMGSIZE.u32 = lvdsImgSize.u32;
    ctrlReg->LVDS_CROP_START0.u32 = cropStart0.u32;
    ctrlReg->LVDS_CROP_START1.u32 = cropStart1.u32;
    ctrlReg->LVDS_CROP_START2.u32 = cropStart2.u32;
    ctrlReg->LVDS_CROP_START3.u32 = cropStart3.u32;
}

void MipiRxDrvSetLvdsCropEn(uint8_t devno, int enable)
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    U_LVDS_CTRL lvdsCtrl;

    ctrlReg = GetLvdsCtrlRegs(devno);
    if (ctrlReg == NULL) {
        return;
    }

    lvdsCtrl.u32 = ctrlReg->LVDS_CTRL.u32;
    lvdsCtrl.bits.lvds_crop_en = enable;
    ctrlReg->LVDS_CTRL.u32 = lvdsCtrl.u32;
}

static int MipiRxHalSetLvdsWdrEn(uint8_t devno, WdrMode wdrMode)
{
    int ret = HDF_SUCCESS;
    U_LVDS_WDR lvdsWdr;
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);
    lvdsWdr.u32 = ctrlReg->LVDS_WDR.u32;

    if (wdrMode == HI_WDR_MODE_NONE) {
        lvdsWdr.bits.lvds_wdr_en = 0;
        lvdsWdr.bits.lvds_wdr_num = 0;
    } else {
        lvdsWdr.bits.lvds_wdr_en = 1;

        switch (wdrMode) {
            case HI_WDR_MODE_2F:
            case HI_WDR_MODE_DOL_2F:
                lvdsWdr.bits.lvds_wdr_num = 1; /* 1 -- 2_wdr */
                break;

            case HI_WDR_MODE_3F:
            case HI_WDR_MODE_DOL_3F:
                lvdsWdr.bits.lvds_wdr_num = 2; /* 2 -- 3_wdr */
                break;

            case HI_WDR_MODE_4F:
            case HI_WDR_MODE_DOL_4F:
                lvdsWdr.bits.lvds_wdr_num = 3; /* 3 -- 4_wdr */
                break;

            default:
                ret = HDF_FAILURE;
                HDF_LOGE("%s: not support WDR_MODE: %d", __func__, wdrMode);
                break;
        }
    }

    ctrlReg->LVDS_WDR.u32 = lvdsWdr.u32;

    return ret;
}

static int MipiRxHalSetLvdsSofWdr(uint8_t devno, WdrMode wdrMode,
    const LvdsVsyncAttr *vsyncAttr)
{
    U_LVDS_WDR lvdsWdr;
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;

    (void)wdrMode;
    ctrlReg = GetLvdsCtrlRegs(devno);
    lvdsWdr.u32 = ctrlReg->LVDS_WDR.u32;

    if (vsyncAttr->syncType == LVDS_VSYNC_NORMAL) {
        /* SOF-EOF WDR, long exposure frame and short exposure frame has independent sync code */
        lvdsWdr.bits.lvds_wdr_mode = 0x0;
    } else if (vsyncAttr->syncType == LVDS_VSYNC_SHARE) {
        /* SOF-EOF WDR, long exposure frame and short exposure frame share the SOF and EOF */
        lvdsWdr.bits.lvds_wdr_mode = 0x2;
    } else {
        HDF_LOGE("%s: not support vsync type: %d", __func__, vsyncAttr->syncType);
        return HDF_ERR_INVALID_PARAM;
    }

    ctrlReg->LVDS_WDR.u32 = lvdsWdr.u32;

    return HDF_SUCCESS;
}

static int MipiRxHalSetLvdsDolWdr(uint8_t devno, WdrMode wdrMode,
    const LvdsVsyncAttr *vsyncAttr, const LvdsFidAttr *fidAttr)
{
    U_LVDS_WDR lvdsWdr;
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;

    (void)wdrMode;
    ctrlReg = GetLvdsCtrlRegs(devno);
    lvdsWdr.u32 = ctrlReg->LVDS_WDR.u32;

    /* Sony DOL WDR */
    if (vsyncAttr->syncType == LVDS_VSYNC_NORMAL) {
        /*
         * SAV-EAV WDR, 4 sync code, fid embedded in 4th sync code
         * long exposure fame and short exposure frame has independent sync code
         */
        if (fidAttr->fidType == LVDS_FID_IN_SAV) {
            lvdsWdr.bits.lvds_wdr_mode = 0x4;
        } else if (fidAttr->fidType == LVDS_FID_IN_DATA) {
            /*
             * SAV-EAV WDR, 5 sync code(Line Information), fid in the fist DATA,
             * fid in data, line information
             */
            if (fidAttr->outputFil) {
                /* Frame Information Line is included in the image data */
                lvdsWdr.bits.lvds_wdr_mode = 0xd;
            } else {
                /* Frame Information Line is not included in the image data */
                lvdsWdr.bits.lvds_wdr_mode = 0x6;
            }
        } else {
            HDF_LOGE("%s: not support fid type: %d", __func__, fidAttr->fidType);
            return HDF_ERR_INVALID_PARAM;
        }
    } else if (vsyncAttr->syncType == LVDS_VSYNC_HCONNECT) {
        /*
         * SAV-EAV H-Connection DOL, long exposure frame and short exposure frame
         * share the same SAV EAV, the H-Blank is assigned by the dol_hblank1 and dol_hblank2
         */
        if (fidAttr->fidType == LVDS_FID_NONE) {
            lvdsWdr.bits.lvds_wdr_mode = 0x5;
        } else {
            HDF_LOGE("%s: not support fid type: %d", __func__, fidAttr->fidType);
            return HDF_ERR_INVALID_PARAM;
        }
    } else {
        HDF_LOGE("%s: not support vsync type: %d", __func__, vsyncAttr->syncType);
        return HDF_ERR_INVALID_PARAM;
    }

    ctrlReg->LVDS_WDR.u32 = lvdsWdr.u32;
    return HDF_SUCCESS;
}

static int MipiRxHalSetLvdsWdrType(uint8_t devno, WdrMode wdrMode,
    const LvdsVsyncAttr *vsyncAttr, const LvdsFidAttr *fidAttr)
{
    int ret = HDF_SUCCESS;

    if (wdrMode >= HI_WDR_MODE_2F && wdrMode <= HI_WDR_MODE_4F) {
        ret = MipiRxHalSetLvdsSofWdr(devno, wdrMode, vsyncAttr);
    } else if (wdrMode >= HI_WDR_MODE_DOL_2F && wdrMode <= HI_WDR_MODE_DOL_4F) {
        ret = MipiRxHalSetLvdsDolWdr(devno, wdrMode, vsyncAttr, fidAttr);
    } else {
    }

    return ret;
}

static void MipiRxHalSetScdHblk(uint8_t devno, WdrMode wdrMode, const LvdsVsyncAttr *vsyncAttr)
{
    U_LVDS_DOLSCD_HBLK scdHblk;
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);
    scdHblk.u32 = ctrlReg->LVDS_DOLSCD_HBLK.u32;

    if ((wdrMode >= HI_WDR_MODE_DOL_2F && wdrMode <= HI_WDR_MODE_DOL_4F) &&
        (vsyncAttr->syncType == LVDS_VSYNC_HCONNECT)) {
        scdHblk.bits.dol_hblank1 = vsyncAttr->hblank1;
        scdHblk.bits.dol_hblank2 = vsyncAttr->hblank2;
    }

    ctrlReg->LVDS_DOLSCD_HBLK.u32 = scdHblk.u32;
}

int MipiRxDrvSetLvdsWdrMode(uint8_t devno, WdrMode wdrMode, const LvdsVsyncAttr *vsyncAttr,
    const LvdsFidAttr *fidAttr)
{
    int ret;

    if (wdrMode == HI_WDR_MODE_BUTT) {
        HDF_LOGE("%s: not support WDR_MODE: %d", __func__, wdrMode);
        return HDF_ERR_NOT_SUPPORT;
    }

    ret = MipiRxHalSetLvdsWdrEn(devno, wdrMode);
    if (ret != HDF_SUCCESS) {
        HDF_LOGE("%s: [MipiRxHalSetLvdsWdrEn] failed.", __func__);
        return ret;
    }

    if (wdrMode != HI_WDR_MODE_NONE) {
        ret = MipiRxHalSetLvdsWdrType(devno, wdrMode, vsyncAttr, fidAttr);
        if (ret != HDF_SUCCESS) {
            HDF_LOGE("%s: [MipiRxHalSetLvdsWdrType] failed.", __func__);
            return ret;
        }

        MipiRxHalSetScdHblk(devno, wdrMode, vsyncAttr);
    }

    return ret;
}

void MipiRxDrvSetLvdsCtrlMode(uint8_t devno, LvdsSyncMode syncMode, DataType inputDataType,
    LvdsBitEndian dataEndian, LvdsBitEndian syncCodeEndian)
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    U_LVDS_CTRL lvdsCtrl;
    unsigned short rawType;

    ctrlReg = GetLvdsCtrlRegs(devno);

    lvdsCtrl.u32 = ctrlReg->LVDS_CTRL.u32;

    switch (inputDataType) {
        case DATA_TYPE_RAW_8BIT:
            rawType = 0x1;
            break;

        case DATA_TYPE_RAW_10BIT:
            rawType = 0x2;
            break;

        case DATA_TYPE_RAW_12BIT:
            rawType = 0x3;
            break;

        case DATA_TYPE_RAW_14BIT:
            rawType = 0x4;
            break;

        case DATA_TYPE_RAW_16BIT:
            rawType = 0x5;
            break;

        default:
            return;
    }

    lvdsCtrl.bits.lvds_sync_mode = syncMode;
    lvdsCtrl.bits.lvds_raw_type = rawType;
    lvdsCtrl.bits.lvds_pix_big_endian = dataEndian;
    lvdsCtrl.bits.lvds_code_big_endian = syncCodeEndian;

    ctrlReg->LVDS_CTRL.u32 = lvdsCtrl.u32;
}

void MipiRxDrvSetLvdsDataRate(uint8_t devno, MipiDataRate dataRate)
{
    U_LVDS_OUTPUT_PIX_NUM lvdsOutputPixelNum;
    unsigned int lvdsDoublePixEn = 0;
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    if (dataRate == MIPI_DATA_RATE_X1) {
        lvdsDoublePixEn = 0;
    } else if (dataRate == MIPI_DATA_RATE_X2) {
        lvdsDoublePixEn = 0x1;
    } else {
        HDF_LOGE("%s: unsupported  dataRate:%d  devno %d", __func__, dataRate, devno);
        return;
    }

    lvdsOutputPixelNum.u32 = lvdsCtrlRegs->LVDS_OUTPUT_PIX_NUM.u32;
    lvdsOutputPixelNum.bits.lvds_double_pix_en = lvdsDoublePixEn;
    lvdsCtrlRegs->LVDS_OUTPUT_PIX_NUM.u32 = lvdsOutputPixelNum.u32;
}

void MipiRxDrvSetDolLineInformation(uint8_t devno, WdrMode wdrMode)
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (wdrMode >= HI_WDR_MODE_DOL_2F) {
        ctrlReg->LVDS_LI_WORD0.bits.li_word0_0 = 0x0201;
        ctrlReg->LVDS_LI_WORD0.bits.li_word0_1 = 0x0211;
        ctrlReg->LVDS_LI_WORD1.bits.li_word1_0 = 0x0202;
        ctrlReg->LVDS_LI_WORD1.bits.li_word1_1 = 0x0212;
    }

    if (wdrMode >= HI_WDR_MODE_DOL_3F) {
        ctrlReg->LVDS_LI_WORD2.bits.li_word2_0 = 0x0204;
        ctrlReg->LVDS_LI_WORD2.bits.li_word2_1 = 0x0214;
    }

    if (wdrMode >= HI_WDR_MODE_DOL_4F) {
        ctrlReg->LVDS_LI_WORD3.bits.li_word3_0 = 0x0208;
        ctrlReg->LVDS_LI_WORD3.bits.li_word3_1 = 0x0218;
    }
}

static void SetLvdsLaneSof(uint8_t devno, int nFrame, int i,
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    volatile LvdsSyncCodeCfgTag *pSyncCode = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (nFrame == TRUE) {
        pSyncCode = &ctrlReg->lvds_this_frame_sync_code[i];
    } else {
        pSyncCode = &ctrlReg->lvds_next_frame_sync_code[i];
    }

    {
        U_LVDS_LANE_SOF_01 lvdsSof01;
        lvdsSof01.u32 = pSyncCode->LVDS_LANE_SOF_01.u32;
        lvdsSof01.bits.lane_sof_0 = syncCode[i][0][0]; /* 0 -- frame0 sof */
        lvdsSof01.bits.lane_sof_1 = syncCode[i][1][0]; /* 1 -- frame1 sof */
        pSyncCode->LVDS_LANE_SOF_01.u32 = lvdsSof01.u32;
    }
    {
        U_LVDS_LANE_SOF_23 lvdsSof23;
        lvdsSof23.u32 = pSyncCode->LVDS_LANE_SOF_23.u32;
        lvdsSof23.bits.lane_sof_2 = syncCode[i][2][0]; /* 2 -- frame2 sof */
        lvdsSof23.bits.lane_sof_3 = syncCode[i][3][0]; /* 3 -- frame3 sof */
        pSyncCode->LVDS_LANE_SOF_23.u32 = lvdsSof23.u32;
    }
}

static void SetLvdsLaneEof(uint8_t devno, int nFrame, int i,
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    volatile LvdsSyncCodeCfgTag *pSyncCode = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (nFrame == TRUE) {
        pSyncCode = &ctrlReg->lvds_this_frame_sync_code[i];
    } else {
        pSyncCode = &ctrlReg->lvds_next_frame_sync_code[i];
    }

    {
        U_LVDS_LANE_EOF_01 lvdsEof01;
        lvdsEof01.u32 = pSyncCode->LVDS_LANE_EOF_01.u32;
        lvdsEof01.bits.lane_eof_0 = syncCode[i][0][1]; /* 0 -- frame0 eof */
        lvdsEof01.bits.lane_eof_1 = syncCode[i][1][1]; /* 1 -- frame1 eof */
        pSyncCode->LVDS_LANE_EOF_01.u32 = lvdsEof01.u32;
    }
    {
        U_LVDS_LANE_EOF_23 lvdsEof23;
        lvdsEof23.u32 = pSyncCode->LVDS_LANE_EOF_23.u32;
        lvdsEof23.bits.lane_eof_2 = syncCode[i][2][1]; /* 2 -- frame2 eof */
        lvdsEof23.bits.lane_eof_3 = syncCode[i][3][1]; /* 3 -- frame3 eof */
        pSyncCode->LVDS_LANE_EOF_23.u32 = lvdsEof23.u32;
    }
}

static void SetLvdsLaneSol(uint8_t devno, int nFrame, int i,
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    volatile LvdsSyncCodeCfgTag *pSyncCode = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (nFrame == TRUE) {
        pSyncCode = &ctrlReg->lvds_this_frame_sync_code[i];
    } else {
        pSyncCode = &ctrlReg->lvds_next_frame_sync_code[i];
    }

    {
        U_LVDS_LANE_SOL_01 lvdsSol01;
        lvdsSol01.u32 = pSyncCode->LVDS_LANE_SOL_01.u32;
        lvdsSol01.bits.lane_sol_0 = syncCode[i][0][2]; /* [0][2] -- frame0 sol bit */
        lvdsSol01.bits.lane_sol_1 = syncCode[i][1][2]; /* [1][2] -- frame1 sol bit */
        pSyncCode->LVDS_LANE_SOL_01.u32 = lvdsSol01.u32;
    }
    {
        U_LVDS_LANE_SOL_23 lvdsSol23;
        lvdsSol23.u32 = pSyncCode->LVDS_LANE_SOL_23.u32;
        lvdsSol23.bits.lane_sol_2 = syncCode[i][2][2]; /* [2][2] -- frame2 sol bit */
        lvdsSol23.bits.lane_sol_3 = syncCode[i][3][2]; /* [3][2] -- frame3 sol bit */
        pSyncCode->LVDS_LANE_SOL_23.u32 = lvdsSol23.u32;
    }
}

static void SetLvdsLaneEol(uint8_t devno, int nFrame, int i,
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    volatile LvdsCtrlRegsTag *ctrlReg = NULL;
    volatile LvdsSyncCodeCfgTag *pSyncCode = NULL;

    ctrlReg = GetLvdsCtrlRegs(devno);

    if (nFrame == TRUE) {
        pSyncCode = &ctrlReg->lvds_this_frame_sync_code[i];
    } else {
        pSyncCode = &ctrlReg->lvds_next_frame_sync_code[i];
    }

    {
        U_LVDS_LANE_EOL_01 lvdsEol01;
        lvdsEol01.u32 = pSyncCode->LVDS_LANE_EOL_01.u32;
        lvdsEol01.bits.lane_eol_0 = syncCode[i][0][3]; /* [0][3] -- frame0 sol */
        lvdsEol01.bits.lane_eol_1 = syncCode[i][1][3]; /* [1][3] -- frame1 sol */
        pSyncCode->LVDS_LANE_EOL_01.u32 = lvdsEol01.u32;
    }
    {
        U_LVDS_LANE_EOL_23 lvdsEol23;
        lvdsEol23.u32 = pSyncCode->LVDS_LANE_EOL_23.u32;
        lvdsEol23.bits.lane_eol_2 = syncCode[i][2][3]; /* [2][3] -- frame2 sol */
        lvdsEol23.bits.lane_eol_3 = syncCode[i][3][3]; /* [3][3] -- frame3 sol */
        pSyncCode->LVDS_LANE_EOL_23.u32 = lvdsEol23.u32;
    }
}

void SetLvdsSyncCode(uint8_t devno, int nFrame, unsigned int laneCnt,
    const short laneId[LVDS_LANE_NUM], const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    int i;

    (void)laneCnt;
    for (i = 0; i < LVDS_LANE_NUM; i++) {
        if (laneId[i] == -1) {
            continue;
        }

        SetLvdsLaneSof(devno, nFrame, i, syncCode);
        SetLvdsLaneEof(devno, nFrame, i, syncCode);
        SetLvdsLaneSol(devno, nFrame, i, syncCode);
        SetLvdsLaneEol(devno, nFrame, i, syncCode);
    }
}

void MipiRxDrvSetLvdsSyncCode(uint8_t devno, unsigned int laneCnt, const short laneId[LVDS_LANE_NUM],
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    SetLvdsSyncCode(devno, TRUE, laneCnt, laneId, syncCode);
}

void MipiRxDrvSetLvdsNxtSyncCode(uint8_t devno, unsigned int laneCnt, const short laneId[LVDS_LANE_NUM],
    const unsigned short syncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    SetLvdsSyncCode(devno, FALSE, laneCnt, laneId, syncCode);
}

void MipiRxDrvSetPhySyncDct(unsigned int phyId, int rawType, LvdsBitEndian codeEndian, unsigned int phyLaneBitmap)
{
    U_PHY_SYNC_DCT_LINK phySyncDctLink;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phySyncDctLink.u32 = mipiRxPhyCfg->PHY_SYNC_DCT_LINK.u32;

    if (phyLaneBitmap & 0x5) {
        phySyncDctLink.bits.cil_raw_type0 = rawType;
        phySyncDctLink.bits.cil_code_big_endian0 = codeEndian;
    }

    if (phyLaneBitmap & 0xa) {
        phySyncDctLink.bits.cil_raw_type1 = rawType;
        phySyncDctLink.bits.cil_code_big_endian1 = codeEndian;
    }

    mipiRxPhyCfg->PHY_SYNC_DCT_LINK.u32 = phySyncDctLink.u32;
}

short GetSensorLaneIndex(short lane, const short laneId[LVDS_LANE_NUM])
{
    int i;

    for (i = 0; i < LVDS_LANE_NUM; i++) {
        if (laneId[i] == lane) {
            break;
        }
    }

    return i;
}

void MipiRxDrvSetLvdsPhySyncCode(unsigned int phyId, const short laneId[LVDS_LANE_NUM],
    const unsigned short nSyncCode[][WDR_VC_NUM][SYNC_CODE_NUM],
    const unsigned short nxtSyncCode[][WDR_VC_NUM][SYNC_CODE_NUM], unsigned int phyLaneBitmap)
{
    U_PHY_SYNC_SOF0_LINK phySyncSof0Link;
    U_PHY_SYNC_SOF1_LINK phySyncSof1Link;
    U_PHY_SYNC_SOF2_LINK phySyncSof2Link;
    U_PHY_SYNC_SOF3_LINK phySyncSof3Link;
    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;
    short sensorLaneIdx;
    short lane;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phySyncSof0Link.u32 = mipiRxPhyCfg->PHY_SYNC_SOF0_LINK.u32;
    phySyncSof1Link.u32 = mipiRxPhyCfg->PHY_SYNC_SOF1_LINK.u32;
    phySyncSof2Link.u32 = mipiRxPhyCfg->PHY_SYNC_SOF2_LINK.u32;
    phySyncSof3Link.u32 = mipiRxPhyCfg->PHY_SYNC_SOF3_LINK.u32;

    if (phyLaneBitmap & 0x1) {
        lane = 0 + 4 * phyId; /* 4 -- 1 phy have 4 lane */
        sensorLaneIdx = GetSensorLaneIndex(lane, laneId);
        phySyncSof0Link.bits.cil_sof0_word4_0 = nSyncCode[sensorLaneIdx][0][0];
        phySyncSof0Link.bits.cil_sof1_word4_0 = nxtSyncCode[sensorLaneIdx][0][0];
    }

    if (phyLaneBitmap & 0x2) {
        lane = 1 + 4 * phyId; /* 4 -- 1 phy have 4 lane */
        sensorLaneIdx = GetSensorLaneIndex(lane, laneId);
        phySyncSof1Link.bits.cil_sof0_word4_1 = nSyncCode[sensorLaneIdx][0][0];
        phySyncSof1Link.bits.cil_sof1_word4_1 = nxtSyncCode[sensorLaneIdx][0][0];
    }

    if (phyLaneBitmap & 0x4) {
        lane = 2 + 4 * phyId; /* 4 -- 1 phy have 4 lane, 2 -- laneId 2 */
        sensorLaneIdx = GetSensorLaneIndex(lane, laneId);
        phySyncSof2Link.bits.cil_sof0_word4_2 = nSyncCode[sensorLaneIdx][0][0];
        phySyncSof2Link.bits.cil_sof1_word4_2 = nxtSyncCode[sensorLaneIdx][0][0];
    }

    if (phyLaneBitmap & 0x8) {
        lane = 3 + 4 * phyId; /* 4 -- 1 phy have 4 lane, 3 -- laneId 3 */
        sensorLaneIdx = GetSensorLaneIndex(lane, laneId);
        phySyncSof3Link.bits.cil_sof0_word4_3 = nSyncCode[sensorLaneIdx][0][0];
        phySyncSof3Link.bits.cil_sof1_word4_3 = nxtSyncCode[sensorLaneIdx][0][0];
    }

    mipiRxPhyCfg->PHY_SYNC_SOF0_LINK.u32 = phySyncSof0Link.u32;
    mipiRxPhyCfg->PHY_SYNC_SOF1_LINK.u32 = phySyncSof1Link.u32;
    mipiRxPhyCfg->PHY_SYNC_SOF2_LINK.u32 = phySyncSof2Link.u32;
    mipiRxPhyCfg->PHY_SYNC_SOF3_LINK.u32 = phySyncSof3Link.u32;
}

void MipiRxDrvSetPhySyncConfig(const LvdsDevAttr *pAttr, unsigned int laneBitmap,
    const unsigned short nxtSyncCode[][WDR_VC_NUM][SYNC_CODE_NUM])
{
    int rawType;
    unsigned int i;
    unsigned int mask;
    unsigned int phyLaneBitmap;

    switch (pAttr->inputDataType) {
        case DATA_TYPE_RAW_8BIT:
            rawType = 0x1;
            break;

        case DATA_TYPE_RAW_10BIT:
            rawType = 0x2;
            break;

        case DATA_TYPE_RAW_12BIT:
            rawType = 0x3;
            break;

        case DATA_TYPE_RAW_14BIT:
            rawType = 0x4;
            break;

        case DATA_TYPE_RAW_16BIT:
            rawType = 0x5;
            break;

        default:
            return;
    }

    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; i++) {
        mask = 0xf << (4 * i); /* 4 -- 4bit */
        if (laneBitmap & mask) {
            phyLaneBitmap = (laneBitmap & mask) >> (4 * i); /* 4 -- 4bit */
            MipiRxDrvSetPhySyncDct(i, rawType, pAttr->syncCodeEndian, phyLaneBitmap);
            MipiRxDrvSetLvdsPhySyncCode(i, pAttr->laneId, pAttr->syncCode, nxtSyncCode, phyLaneBitmap);
        }
    }
}

int MipiRxDrvIsLaneValid(uint8_t devno, short laneId, LaneDivideMode mode)
{
    int laneValid = 0;

    switch (mode) {
        case LANE_DIVIDE_MODE_0:
            if (devno == 0) {
                if (laneId >= 0 && laneId <= 3) { /* 3 -- laneId max value */
                    laneValid = 1;
                }
            }
            break;

        case LANE_DIVIDE_MODE_1:
            if ((devno == 0) || (devno == 1)) {
                if (laneId >= 0 && laneId <= 3) { /* 3 -- laneId max value */
                    laneValid = 1;
                }
            }
            break;

        default:
            break;
    }

    return laneValid;
}

static void MipiRxDrvHwInit(void);
void MipiRxDrvSetHsMode(LaneDivideMode mode)
{
    U_HS_MODE_SELECT hsModeSel;
    MipiRxSysRegsTag *mipiRxSysRegs = NULL;
    int i;

    MipiRxDrvHwInit();

    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; i++) {
        MipiRxSetPhyRg2121En(i, g_phyMode[mode][i].phy_rg_en_2121);
        MipiRxSetPhyRgClk0En(i, g_phyMode[mode][i].phy_rg_clk0_en);
        MipiRxSetPhyRgClk1En(i, g_phyMode[mode][i].phy_rg_clk1_en);
        MipiRxSetPhyRgLp0ModeEn(i, g_phyMode[mode][i].phy_rg_lp0_mode_en);
        MipiRxSetPhyRgLp1ModeEn(i, g_phyMode[mode][i].phy_rg_lp1_mode_en);
    }

    mipiRxSysRegs = GetMipiRxSysRegs();
    hsModeSel.u32 = mipiRxSysRegs->HS_MODE_SELECT.u32;
    hsModeSel.bits.hs_mode = mode;
    mipiRxSysRegs->HS_MODE_SELECT.u32 = hsModeSel.u32;
}

void MipiRxDrvSetMipiIntMask(uint8_t devno)
{
    U_MIPI_INT_MSK mipiIntMsk;
    volatile MipiRxSysRegsTag *mipiRxSysRegs = GetMipiRxSysRegs();

    mipiIntMsk.u32 = mipiRxSysRegs->MIPI_INT_MSK.u32;

    if (devno == 0) {
        mipiIntMsk.bits.int_chn0_mask = 0x1;
    } else if (devno == 1) {
        mipiIntMsk.bits.int_chn1_mask = 0x1;
    }

    mipiRxSysRegs->MIPI_INT_MSK.u32 = mipiIntMsk.u32;
}

void MipiRxDrvSetLvdsCtrlIntMask(uint8_t devno, unsigned int mask)
{
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    lvdsCtrlRegs->LVDS_CTRL_INT_MSK.u32 = mask;
}

void MipiRxDrvSetMipiCtrlIntMask(uint8_t devno, unsigned int mask)
{
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlRegs->MIPI_CTRL_INT_MSK.u32 = mask;
}

void MipiRxDrvSetMipiPkt1IntMask(uint8_t devno, unsigned int mask)
{
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlRegs->MIPI_PKT_INTR_MSK.u32 = mask;
}

void MipiRxDrvSetMipiPkt2IntMask(uint8_t devno, unsigned int mask)
{
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlRegs->MIPI_PKT_INTR2_MSK.u32 = mask;
}

void MipiRxDrvSetMipiFrameIntMask(uint8_t devno, unsigned int mask)
{
    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlRegs->MIPI_FRAME_INTR_MSK.u32 = mask;
}

void MipiRxDrvSetAlignIntMask(uint8_t devno, unsigned int mask)
{
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    lvdsCtrlRegs->ALIGN0_INT_MSK.u32 = mask;
    lvdsCtrlRegs->CHN_INT_MASK.u32 = 0xf;
}

void MipiRxEnableDisableClock(uint8_t comboDev, int enable)
{
    unsigned long *mipiRxClockAddr = (unsigned long *)OsalIoRemap(MIPI_RX_CRG_ADDR, (unsigned long)0x4);

    if (mipiRxClockAddr == 0) {
        HDF_LOGE("%s: MipiRx clock ioremap failed!", __func__);
        return;
    }
    SetBit(enable, 1 + comboDev, mipiRxClockAddr);
    OsalIoUnmap((void *)mipiRxClockAddr);
}

void MipiRxDrvEnableClock(uint8_t comboDev)
{
    MipiRxEnableDisableClock(comboDev, 1);
}

void MipiRxDrvDisableClock(uint8_t comboDev)
{
    MipiRxEnableDisableClock(comboDev, 0);
}

void SensorEnableDisableClock(uint8_t snsClkSource, int enable)
{
    unsigned long *sensorClockAddr = NULL;
    unsigned offset;

    if (snsClkSource == 0) {
        offset = 0; /* 0 -- sensor0_cken is bit[0] */
    } else if (snsClkSource == 1) {
        offset = 6; /* 6 -- sensor1_cken is bit[6] */
    } else {
        HDF_LOGE("%s: invalid sensor clock source!", __func__);
        return;
    }

    sensorClockAddr = (unsigned long *)OsalIoRemap(SNS_CRG_ADDR, (unsigned long)0x4);
    if (sensorClockAddr == 0) {
        HDF_LOGE("%s: sensor clock ioremap failed!", __func__);
        return;
    }
    SetBit(enable, offset, sensorClockAddr);
    OsalIoUnmap((void *)sensorClockAddr);
}

void SensorDrvEnableClock(uint8_t snsClkSource)
{
    SensorEnableDisableClock(snsClkSource, 1);
}

void SensorDrvDisableClock(uint8_t snsClkSource)
{
    SensorEnableDisableClock(snsClkSource, 0);
}

void MipiRxCoreResetUnreset(uint8_t comboDev, int reset)
{
    /* 4 -- mipi_pix0_core_srst_req bit[4] */
    SetBit(reset, (comboDev + 4), (unsigned long *)(uintptr_t)g_mipiRxCoreResetAddr); 
}

void MipiRxDrvCoreReset(uint8_t comboDev)
{
    MipiRxCoreResetUnreset(comboDev, 1);
}

void MipiRxDrvCoreUnreset(uint8_t comboDev)
{
    MipiRxCoreResetUnreset(comboDev, 0);
}

void SensorResetUnreset(uint8_t snsResetSource, int reset)
{
    unsigned long *sensorResetAddr = NULL;
    unsigned offset;

    if (snsResetSource == 0) {
        offset = 1; /* 1 -- sensor0_srst_req is bit[1] */
    } else if (snsResetSource == 1) {
        offset = 7; /* 7 -- sensor1_srst_req is bit[7] */
    } else {
        HDF_LOGE("%s: invalid sensor reset source!", __func__);
        return;
    }

    sensorResetAddr = (unsigned long *)OsalIoRemap(SNS_CRG_ADDR, (unsigned long)0x4);
    if (sensorResetAddr == 0) {
        HDF_LOGE("%s: sensor reset ioremap failed!", __func__);
        return;
    }
    SetBit(reset, offset, sensorResetAddr);
    OsalIoUnmap((void *)sensorResetAddr);
}

void SensorDrvReset(uint8_t snsResetSource)
{
    SensorResetUnreset(snsResetSource, 1);
}

void SensorDrvUnreset(uint8_t snsResetSource)
{
    SensorResetUnreset(snsResetSource, 0);
}

void MipiRxDrvGetMipiImgsizeStatis(uint8_t devno, short vc, ImgSize *pSize)
{
    U_MIPI_IMGSIZE0_STATIS mipiImgsize0Statis;
    U_MIPI_IMGSIZE1_STATIS mipiImgsize1Statis;
    U_MIPI_IMGSIZE2_STATIS mipiImgsize2Statis;
    U_MIPI_IMGSIZE3_STATIS mipiImgsize3Statis;

    volatile MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    if (vc == 0) { /* 0 -- vc0 */
        mipiImgsize0Statis.u32 = mipiCtrlRegs->MIPI_IMGSIZE0_STATIS.u32;
        pSize->width = mipiImgsize0Statis.bits.imgwidth_statis_vc0;
        pSize->height = mipiImgsize0Statis.bits.imgheight_statis_vc0;
    } else if (vc == 1) { /* 1 -- vc1 */
        mipiImgsize1Statis.u32 = mipiCtrlRegs->MIPI_IMGSIZE1_STATIS.u32;
        pSize->width = mipiImgsize1Statis.bits.imgwidth_statis_vc1;
        pSize->height = mipiImgsize1Statis.bits.imgheight_statis_vc1;
    } else if (vc == 2) { /* 2 -- vc2 */
        mipiImgsize2Statis.u32 = mipiCtrlRegs->MIPI_IMGSIZE2_STATIS.u32;
        pSize->width = mipiImgsize2Statis.bits.imgwidth_statis_vc2;
        pSize->height = mipiImgsize2Statis.bits.imgheight_statis_vc2;
    } else if (vc == 3) { /* 3 -- vc3 */
        mipiImgsize3Statis.u32 = mipiCtrlRegs->MIPI_IMGSIZE3_STATIS.u32;
        pSize->width = mipiImgsize3Statis.bits.imgwidth_statis_vc3;
        pSize->height = mipiImgsize3Statis.bits.imgheight_statis_vc3;
    } else {
        pSize->width = 0;
        pSize->height = 0;
    }
}

void MipiRxDrvGetLvdsImgsizeStatis(uint8_t devno, short vc, ImgSize *pSize)
{
    U_LVDS_IMGSIZE0_STATIS lvdsImgSize0Statis;
    U_LVDS_IMGSIZE1_STATIS lvdsImgSize1Statis;

    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    if (vc == 0) {
        lvdsImgSize0Statis.u32 = lvdsCtrlRegs->LVDS_IMGSIZE0_STATIS.u32;
        pSize->width = lvdsImgSize0Statis.bits.lvds_imgwidth0;
        pSize->height = lvdsImgSize0Statis.bits.lvds_imgheight0;
    } else if (vc == 1) {
        lvdsImgSize1Statis.u32 = lvdsCtrlRegs->LVDS_IMGSIZE1_STATIS.u32;
        pSize->width = lvdsImgSize1Statis.bits.lvds_imgwidth1;
        pSize->height = lvdsImgSize1Statis.bits.lvds_imgheight1;
    }
}

void MipiRxDrvGetLvdsLaneImgsizeStatis(uint8_t devno, short lane, ImgSize *pSize)
{
    U_LVDS_LANE_IMGSIZE_STATIS statis;

    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    statis.u32 = lvdsCtrlRegs->LVDS_LANE_IMGSIZE_STATIS[lane].u32;
    pSize->width = statis.bits.lane_imgwidth + 1;
    pSize->height = statis.bits.lane_imgheight;
}

static void MipiRxPhyCilIntStatis(int phyId)
{
    unsigned int phyIntStatus;

    MipiRxPhyCfgTag *mipiRxPhyCfg = NULL;

    mipiRxPhyCfg = GetMipiRxPhyRegs(phyId);
    phyIntStatus = mipiRxPhyCfg->MIPI_CIL_INT_LINK.u32;

    if (phyIntStatus) {
        mipiRxPhyCfg->MIPI_CIL_INT_RAW_LINK.u32 = 0xffffffff;

        if (phyIntStatus & MIPI_ESC_CLK1) {
            g_phyErrIntCnt[phyId].clk1FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_ESC_CLK0) {
            g_phyErrIntCnt[phyId].clk0FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_ESC_D0) {
            g_phyErrIntCnt[phyId].d0FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_ESC_D1) {
            g_phyErrIntCnt[phyId].d1FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_ESC_D2) {
            g_phyErrIntCnt[phyId].d2FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_ESC_D3) {
            g_phyErrIntCnt[phyId].d3FsmEscapeErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_CLK1) {
            g_phyErrIntCnt[phyId].clk1FsmTimeoutErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_CLK0) {
            g_phyErrIntCnt[phyId].clk0FsmTimeoutErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_D0) {
            g_phyErrIntCnt[phyId].d0FsmTimeoutErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_D1) {
            g_phyErrIntCnt[phyId].d1FsmTimeoutErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_D2) {
            g_phyErrIntCnt[phyId].d2FsmTimeoutErrCnt++;
        }

        if (phyIntStatus & MIPI_TIMEOUT_D3) {
            g_phyErrIntCnt[phyId].d3FsmTimeoutErrCnt++;
        }
    }
}

static void MipiRxPktInt1Statics(uint8_t devno)
{
    unsigned int pktInt1;
    MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    pktInt1 = mipiCtrlRegs->MIPI_PKT_INTR_ST.u32;
    if (pktInt1) {
        if (pktInt1 & MIPI_PKT_HEADER_ERR) {
            g_mipiErrIntCnt[devno].errEccDoubleCnt++;
        }

        if (pktInt1 & MIPI_VC0_PKT_DATA_CRC) {
            g_mipiErrIntCnt[devno].vc0ErrCrcCnt++;
        }

        if (pktInt1 & MIPI_VC1_PKT_DATA_CRC) {
            g_mipiErrIntCnt[devno].vc1ErrCrcCnt++;
        }

        if (pktInt1 & MIPI_VC2_PKT_DATA_CRC) {
            g_mipiErrIntCnt[devno].vc2ErrCrcCnt++;
        }

        if (pktInt1 & MIPI_VC3_PKT_DATA_CRC) {
            g_mipiErrIntCnt[devno].vc3ErrCrcCnt++;
        }
    }
}

static void MipiRxPktInt2Statics(uint8_t devno)
{
    unsigned int pktInt2;
    MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    pktInt2 = mipiCtrlRegs->MIPI_PKT_INTR2_ST.u32;
    if (pktInt2) {
        if (pktInt2 & MIPI_VC0_PKT_INVALID_DT) {
            g_mipiErrIntCnt[devno].errIdVc0Cnt++;
        }

        if (pktInt2 & MIPI_VC1_PKT_INVALID_DT) {
            g_mipiErrIntCnt[devno].errIdVc1Cnt++;
        }

        if (pktInt2 & MIPI_VC2_PKT_INVALID_DT) {
            g_mipiErrIntCnt[devno].errIdVc2Cnt++;
        }

        if (pktInt2 & MIPI_VC3_PKT_INVALID_DT) {
            g_mipiErrIntCnt[devno].errIdVc3Cnt++;
        }

        if (pktInt2 & MIPI_VC0_PKT_HEADER_ECC) {
            g_mipiErrIntCnt[devno].vc0ErrEccCorrectedCnt++;
        }

        if (pktInt2 & MIPI_VC1_PKT_HEADER_ECC) {
            g_mipiErrIntCnt[devno].vc1ErrEccCorrectedCnt++;
        }

        if (pktInt2 & MIPI_VC2_PKT_HEADER_ECC) {
            g_mipiErrIntCnt[devno].vc2ErrEccCorrectedCnt++;
        }

        if (pktInt2 & MIPI_VC3_PKT_HEADER_ECC) {
            g_mipiErrIntCnt[devno].vc3ErrEccCorrectedCnt++;
        }
    }
}

static void MipiRxFrameIntrStatics(uint8_t devno)
{
    unsigned int frameInt;
    MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    frameInt = mipiCtrlRegs->MIPI_FRAME_INTR_ST.u32;
    if (frameInt) {
        if (frameInt & MIPI_VC0_FRAME_CRC) {
            g_mipiErrIntCnt[devno].errFrameDataVc0Cnt++;
        }

        if (frameInt & MIPI_VC1_FRAME_CRC) {
            g_mipiErrIntCnt[devno].errFrameDataVc1Cnt++;
        }

        if (frameInt & MIPI_VC2_FRAME_CRC) {
            g_mipiErrIntCnt[devno].errFrameDataVc2Cnt++;
        }

        if (frameInt & MIPI_VC3_FRAME_CRC) {
            g_mipiErrIntCnt[devno].errFrameDataVc3Cnt++;
        }

        if (frameInt & MIPI_VC0_ORDER_ERR) {
            g_mipiErrIntCnt[devno].errFSeqVc0Cnt++;
        }

        if (frameInt & MIPI_VC1_ORDER_ERR) {
            g_mipiErrIntCnt[devno].errFSeqVc1Cnt++;
        }

        if (frameInt & MIPI_VC2_ORDER_ERR) {
            g_mipiErrIntCnt[devno].errFSeqVc2Cnt++;
        }

        if (frameInt & MIPI_VC3_ORDER_ERR) {
            g_mipiErrIntCnt[devno].errFSeqVc3Cnt++;
        }

        if (frameInt & MIPI_VC0_NO_MATCH) {
            g_mipiErrIntCnt[devno].errFBndryMatchVc0Cnt++;
        }

        if (frameInt & MIPI_VC1_NO_MATCH) {
            g_mipiErrIntCnt[devno].errFBndryMatchVc1Cnt++;
        }

        if (frameInt & MIPI_VC2_NO_MATCH) {
            g_mipiErrIntCnt[devno].errFBndryMatchVc2Cnt++;
        }

        if (frameInt & MIPI_VC3_NO_MATCH) {
            g_mipiErrIntCnt[devno].errFBndryMatchVc3Cnt++;
        }
    }
}

static void MipiRxMainIntStatics(uint8_t devno)
{
    unsigned int lineInt;
    unsigned int mipiMainInt;
    unsigned int mipiCtrlInt;
    MipiCtrlRegsTag *mipiCtrlRegs = GetMipiCtrlRegs(devno);

    mipiCtrlInt = mipiCtrlRegs->MIPI_CTRL_INT.u32;

    /* Need read mipiMainInt and lineInt, to clear MIPI_MAIN_INT_ST and MIPI_LINE_INTR_ST interrupt. */
    mipiMainInt = mipiCtrlRegs->MIPI_MAIN_INT_ST.u32;
    lineInt = mipiCtrlRegs->MIPI_LINE_INTR_ST.u32;

    if (mipiMainInt) {
        mipiCtrlRegs->MIPI_MAIN_INT_ST.u32 = 0xffffffff;
    }

    if (lineInt) {
        mipiCtrlRegs->MIPI_LINE_INTR_ST.u32 = 0xffffffff;
    }

    if (mipiCtrlInt) {
        mipiCtrlRegs->MIPI_CTRL_INT_RAW.u32 = 0xffffffff;
    }

    if (mipiCtrlInt) {
        if (mipiCtrlInt & CMD_FIFO_READ_ERR) {
            g_mipiErrIntCnt[devno].cmdFifoRderrCnt++;
        }

        if (mipiCtrlInt & DATA_FIFO_READ_ERR) {
            g_mipiErrIntCnt[devno].dataFifoRderrCnt++;
        }

        if (mipiCtrlInt & CMD_FIFO_WRITE_ERR) {
            g_mipiErrIntCnt[devno].cmdFifoWrerrCnt++;
        }

        if (mipiCtrlInt & DATA_FIFO_WRITE_ERR) {
            g_mipiErrIntCnt[devno].dataFifoWrerrCnt++;
        }
    }
}

static void MipiIntStatics(uint8_t devno)
{
    MipiRxMainIntStatics(devno);
    MipiRxPktInt1Statics(devno);
    MipiRxPktInt2Statics(devno);
    MipiRxFrameIntrStatics(devno);
}

static void LvdsIntStatics(uint8_t devno)
{
    unsigned int lvdsCtrlInt;
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    lvdsCtrlInt = lvdsCtrlRegs->LVDS_CTRL_INT.u32;

    if (lvdsCtrlInt) {
        lvdsCtrlRegs->LVDS_CTRL_INT_RAW.u32 = 0xffffffff;
    }

    if (lvdsCtrlInt & CMD_RD_ERR) {
        g_lvdsErrIntCnt[devno].cmdRdErrCnt++;
    }

    if (lvdsCtrlInt & CMD_WR_ERR) {
        g_lvdsErrIntCnt[devno].cmdWrErrCnt++;
    }

    if (lvdsCtrlInt & LVDS_POP_ERR) {
        g_lvdsErrIntCnt[devno].popErrCnt++;
    }

    if (lvdsCtrlInt & LVDS_STAT_ERR) {
        g_lvdsErrIntCnt[devno].lvdsStateErrCnt++;
    }

    if (lvdsCtrlInt & LINK0_READ_ERR) {
        g_lvdsErrIntCnt[devno].link0RdErrCnt++;
    }

    if (lvdsCtrlInt & LINK0_WRITE_ERR) {
        g_lvdsErrIntCnt[devno].link0WrErrCnt++;
    }
}

static void AlignIntStatis(uint8_t devno)
{
    unsigned int alignInt;
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = GetLvdsCtrlRegs(devno);

    alignInt = lvdsCtrlRegs->ALIGN0_INT.u32;

    if (alignInt) {
        lvdsCtrlRegs->ALIGN0_INT_RAW.u32 = 0xffffffff;
    }

    if (alignInt & ALIGN_FIFO_FULL_ERR) {
        g_alignErrIntCnt[devno].fifoFullErrCnt++;
    }

    if (alignInt & ALIGN_LANE0_ERR) {
        g_alignErrIntCnt[devno].lane0AlignErrCnt++;
    }

    if (alignInt & ALIGN_LANE1_ERR) {
        g_alignErrIntCnt[devno].lane1AlignErrCnt++;
    }

    if (alignInt & ALIGN_LANE2_ERR) {
        g_alignErrIntCnt[devno].lane2AlignErrCnt++;
    }

    if (alignInt & ALIGN_LANE3_ERR) {
        g_alignErrIntCnt[devno].lane3AlignErrCnt++;
    }
}

static uint32_t MipiRxInterruptRoute(uint32_t irq, void *devId)
{
    volatile MipiRxSysRegsTag *mipiRxSysRegs = GetMipiRxSysRegs();
    volatile LvdsCtrlRegsTag *lvdsCtrlRegs = NULL;
    int i;

    (void)irq;
    (void)devId;
    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; i++) {
        MipiRxPhyCilIntStatis(i);
    }

    for (i = 0; i < MIPI_RX_MAX_DEV_NUM; i++) {
        lvdsCtrlRegs = GetLvdsCtrlRegs(i);
        if (lvdsCtrlRegs->CHN_INT_RAW.u32) {
            MipiRxDrvCoreReset(i);
            MipiRxDrvCoreUnreset(i);
        } else {
            continue;
        }

        MipiIntStatics(i);
        LvdsIntStatics(i);
        AlignIntStatis(i);
        lvdsCtrlRegs->CHN_INT_RAW.u32 = 0xf;
    }

    mipiRxSysRegs->MIPI_INT_RAW.u32 = 0xff;

    return HDF_SUCCESS;
}

static int MipiRxDrvRegInit(void)
{
    if (g_mipiRxRegsVa == NULL) {
        g_mipiRxRegsVa = (MipiRxRegsTypeTag *)OsalIoRemap(MIPI_RX_REGS_ADDR, (unsigned int)MIPI_RX_REGS_SIZE);
        if (g_mipiRxRegsVa == NULL) {
            HDF_LOGE("%s: remap mipi_rx reg addr fail", __func__);
            return -1;
        }
        g_regMapFlag = 1;
    }

    return 0;
}

static void MipiRxDrvRegExit(void)
{
    if (g_regMapFlag == 1) {
        if (g_mipiRxRegsVa != NULL) {
            OsalIoUnmap((void *)g_mipiRxRegsVa);
            g_mipiRxRegsVa = NULL;
        }
        g_regMapFlag = 0;
    }
}

static int MipiRxRegisterIrq(void)
{
    int ret;

    // This function needs to be verified.
    ret = OsalRegisterIrq(g_mipiRxIrqNum, 0, MipiRxInterruptRoute, "MIPI_RX", NULL);
    if (ret < 0) {
        HDF_LOGE("%s: MipiRx: failed to register irq.", __func__);
        return -1;
    }

    return 0;
}

static void MipiRxUnregisterIrq(void)
{
    OsalUnregisterIrq(g_mipiRxIrqNum, MipiRxInterruptRoute);
}

static void MipiRxDrvHwInit(void)
{
    unsigned long *mipiRxCrgAddr = (unsigned long *)OsalIoRemap(MIPI_RX_CRG_ADDR, (unsigned long)0x4);
    int i;

    WriteReg32(mipiRxCrgAddr, 1 << 0, 0x1 << 0); /* 0 -- cil_cken bit[0] */
    WriteReg32(mipiRxCrgAddr, 1 << 3, 0x1 << 3); /* 3 -- mipi_bus_cken bit[3] */
    WriteReg32(mipiRxCrgAddr, 1 << 6, 0x1 << 6); /* 6 -- mipi_bus_srst_req bit[6] */
    OsalUDelay(10); /* 10 -- udelay 10ns */
    WriteReg32(mipiRxCrgAddr, 0, 0x1 << 6); /* 6 -- mipi_bus_srst_req bit[6] */

    OsalIoUnmap((void *)mipiRxCrgAddr);

    for (i = 0; i < MIPI_RX_MAX_PHY_NUM; ++i) {
        MipiRxSetCilIntMask(i,  MIPI_CIL_INT_MASK);
        MipiRxSetPhySkewLink(i, SKEW_LINK);
        MipiRxSetPhyFsmoLink(i, MIPI_FSMO_VALUE);
    }
}

static void MipiRxDrvHwExit(void)
{
    unsigned long *mipiRxCrgAddr = (unsigned long *)OsalIoRemap(MIPI_RX_CRG_ADDR, (unsigned long)0x4);

    WriteReg32(mipiRxCrgAddr, 1 << 6, 0x1 << 6); /* 6 -- mipi_bus_srst_req bit[6] */
    WriteReg32(mipiRxCrgAddr, 0, 0x1 << 0);      /* 0 -- cil_cken bit[0] */
    WriteReg32(mipiRxCrgAddr, 0, 0x1 << 3);      /* 3 -- mipi_bus_cken bit[3] */

    OsalIoUnmap((void *)mipiRxCrgAddr);
}

int MipiRxDrvInit(void)
{
    int ret;

    ret = MipiRxDrvRegInit();
    if (ret < 0) {
        HDF_LOGE("%s: MipiRxDrvRegInit fail!", __func__);
        goto fail0;
    }

    ret = MipiRxRegisterIrq();
    if (ret < 0) {
        HDF_LOGE("%s: MipiRxRegisterIrq fail!", __func__);
        goto fail1;
    }

    g_mipiRxCoreResetAddr = (unsigned long)(uintptr_t)OsalIoRemap(MIPI_RX_CRG_ADDR, (unsigned long)0x4);
    if (g_mipiRxCoreResetAddr == 0) {
        HDF_LOGE("%s: MipiRx reset ioremap failed!", __func__);
        goto fail2;
    }

    MipiRxDrvHwInit();

    return HDF_SUCCESS;

fail2:
    MipiRxUnregisterIrq();
fail1:
    MipiRxDrvRegExit();
fail0:
    return HDF_FAILURE;
}

void MipiRxDrvExit(void)
{
    MipiRxUnregisterIrq();
    MipiRxDrvRegExit();
    MipiRxDrvHwExit();
    OsalIoUnmap((void *)(uintptr_t)g_mipiRxCoreResetAddr);
}

#ifdef __cplusplus
#if __cplusplus
}

#endif
#endif /* End of #ifdef __cplusplus */
