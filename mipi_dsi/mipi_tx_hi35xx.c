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

#include <los_hw.h>
#include "osal_io.h"
#include "osal_mem.h"
#include "osal_time.h"
#include "hdf_log.h"
#include "mipi_dsi_core.h"
#include "mipi_tx_reg.h"
#include "mipi_tx_hi35xx.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

volatile  MipiTxRegsTypeTag *g_mipiTxRegsVa = NULL;
unsigned int g_mipiTxIrqNum = MIPI_TX_IRQ;
unsigned int g_actualPhyDataRate;
static unsigned int g_regMapFlag;
static bool g_enCfg;

static void WriteReg32(unsigned long addr, unsigned int value, unsigned int mask)
{
    unsigned int t;

    t = OSAL_READL((void *)addr);
    t &= ~mask;
    t |= value & mask;
    OSAL_WRITEL(t, (void *)addr);
}

static void OsalIsb(void)
{
    __asm__ __volatile__("isb" : : : "memory");
}

static void OsalDsb(void)
{
    __asm__ __volatile__("dsb" : : : "memory");
}

static void OsalDmb(void)
{
    __asm__ __volatile__("dmb" : : : "memory");
}

static void HdfIsbDsbDmb(void)
{
    OsalIsb();
    OsalDsb();
    OsalDmb();
}

static void SetPhyReg(unsigned int addr, unsigned char value)
{
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL1.u32 = (0x10000 + addr);
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL0.u32 = 0x2;
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL0.u32 = 0x0;
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL1.u32 = value;
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL0.u32 = 0x2;
    HdfIsbDsbDmb();
    g_mipiTxRegsVa->PHY_TST_CTRL0.u32 = 0x0;
    HdfIsbDsbDmb();
}


static unsigned char MipiTxDrvGetPhyPllSet0(unsigned int phyDataRate)
{
    unsigned char pllSet0;

    /* to get pllSet0, the parameters come from algorithm */
    if (phyDataRate > 750) {          /* 750: mipi clk */
        pllSet0 = 0x0;
    } else if (phyDataRate > 375) {   /* 375: mipi clk */
        pllSet0 = 0x8;
    } else if (phyDataRate > 188) {   /* 188: mipi clk */
        pllSet0 = 0xa;
    } else if (phyDataRate > 94) {    /* 94: mipi clk */
        pllSet0 = 0xc;
    } else {
        pllSet0 = 0xe;
    }
    return pllSet0;
}

static void MipiTxDrvGetPhyPllSet1Set5(unsigned int phyDataRate,
    unsigned char pllSet0,
    unsigned char *pllSet1,
    unsigned char *pllSet5)
{
    int dataRateClk;
    int pllRef;

    dataRateClk = (phyDataRate + MIPI_TX_REF_CLK - 1) / MIPI_TX_REF_CLK;

    /* to get pllSet1 and pllSet5, the parameters come from algorithm */
    if (pllSet0 / 2 == 4) {           /* 2: pll, 4: pll sel */
        pllRef = 2;                   /* 2: pll set */
    } else if (pllSet0 / 2 == 5) {    /* 2: pll, 5: pllSet5 */
        pllRef = 4;                   /* 4: pll set */
    } else if (pllSet0 / 2 == 6) {    /* 2: pll, 6: pll sel */
        pllRef = 8;                   /* 8: pll set */
    } else if (pllSet0 / 2 == 7) {    /* 2: pll, 7: pll sel */
        pllRef = 16;                  /* 16: pll set */
    } else {
        pllRef = 1;
    }
    if ((dataRateClk * pllRef) % 2) { /* 2: pll */
        *pllSet1 = 0x10;
        *pllSet5 = (dataRateClk * pllRef - 1) / 2; /* 2: pllRef sel */
    } else {
        *pllSet1 = 0x20;
        *pllSet5 = dataRateClk * pllRef / 2 - 1;   /* 2: pllRef sel */
    }

    return;
}

static void MipiTxDrvSetPhyPllSetX(unsigned int phyDataRate)
{
    unsigned char pllSet0;
    unsigned char pllSet1;
    unsigned char pllSet2;
#ifdef HI_FPGA
    unsigned char pllSet3;
#endif
    unsigned char pllSet4;
    unsigned char pllSet5;

    /* pllSet0 */
    pllSet0 = MipiTxDrvGetPhyPllSet0(phyDataRate);
    SetPhyReg(PLL_SET0, pllSet0);
    /* pllSet1 */
    MipiTxDrvGetPhyPllSet1Set5(phyDataRate, pllSet0, &pllSet1, &pllSet5);
    SetPhyReg(PLL_SET1, pllSet1);
    /* pllSet2 */
    pllSet2 = 0x2;
    SetPhyReg(PLL_SET2, pllSet2);
    /* pllSet4 */
    pllSet4 = 0x0;
    SetPhyReg(PLL_SET4, pllSet4);

#ifdef HI_FPGA
    pllSet3 = 0x1;
    SetPhyReg(PLL_SET3, pllSet3);
#endif
    /* pllSet5 */
    SetPhyReg(PLL_SET5, pllSet5);

#ifdef MIPI_TX_DEBUG
    HDF_LOGI("\n==========phy pll info=======\n");
    HDF_LOGI("pllSet0(0x14): 0x%x\n", pllSet0);
    HDF_LOGI("pllSet1(0x15): 0x%x\n", pllSet1);
    HDF_LOGI("pllSet2(0x16): 0x%x\n", pllSet2);
#ifdef HI_FPGA
    HDF_LOGI("pllSet3(0x17): 0x%x\n", pllSet3);
#endif
    HDF_LOGI("pllSet4(0x1e): 0x%x\n", pllSet4);
    HDF_LOGI("=========================\n");
#endif
}

static void MipiTxDrvGetPhyClkPrepare(unsigned char *clkPrepare)
{
    unsigned char temp0;
    unsigned char temp1;

    temp0 = ((g_actualPhyDataRate * TCLK_PREPARE + ROUNDUP_VALUE) / INNER_PEROID - 1 +
            ((g_actualPhyDataRate * PREPARE_COMPENSATE + ROUNDUP_VALUE) / INNER_PEROID) -
            ((((g_actualPhyDataRate * TCLK_PREPARE + ROUNDUP_VALUE) / INNER_PEROID +
            ((g_actualPhyDataRate * PREPARE_COMPENSATE + ROUNDUP_VALUE) / INNER_PEROID)) * INNER_PEROID -
            PREPARE_COMPENSATE * g_actualPhyDataRate - TCLK_PREPARE * g_actualPhyDataRate) / INNER_PEROID));
    if (temp0 > 0) { /* 0 is the minmum  */
        temp1 = temp0;
    } else {
        temp1 = 0; /* 0 is the minmum  */
    }

    if (((temp1 + 1) * INNER_PEROID - PREPARE_COMPENSATE * g_actualPhyDataRate) /* temp + 1 is next level period */
        > 94 * g_actualPhyDataRate) { /* 94 is the  maximum in mipi protocol */
        if (temp0 > 0) {
            *clkPrepare = temp0 - 1;
        } else {
            *clkPrepare = 255; /* set 255 will easy to found mistake */
            HDF_LOGE("err when calc phy timing");
        }
    } else {
        if (temp0 > 0) { /* 0 is the minmum  */
            *clkPrepare = temp0;
        } else {
            *clkPrepare = 0; /* 0 is the minmum  */
        }
    }
}

static void MipiTxDrvGetPhyDataPrepare(unsigned char *dataPrepare)
{
    unsigned char temp0;
    unsigned char temp1;

    /* DATA_THS_PREPARE */
    temp0 = ((g_actualPhyDataRate * THS_PREPARE + ROUNDUP_VALUE) / INNER_PEROID - 1 +
            ((g_actualPhyDataRate * PREPARE_COMPENSATE + ROUNDUP_VALUE) / INNER_PEROID) -
            ((((g_actualPhyDataRate * THS_PREPARE + ROUNDUP_VALUE) / INNER_PEROID +
            ((g_actualPhyDataRate * PREPARE_COMPENSATE + ROUNDUP_VALUE) / INNER_PEROID)) * INNER_PEROID -
            PREPARE_COMPENSATE * g_actualPhyDataRate - THS_PREPARE * g_actualPhyDataRate) / INNER_PEROID));
    if (temp0 > 0) {
        temp1 = temp0;
    } else {
        temp1 = 0;
    }

    if ((g_actualPhyDataRate > 105) && /* bigger than 105 */
        (((temp1 + 1) * INNER_PEROID - PREPARE_COMPENSATE * g_actualPhyDataRate) >
        (85 * g_actualPhyDataRate + 6 * 1000))) { /* 85 + 6 * 1000 is the  maximum in mipi protocol */
        if (temp0 > 0) {
            *dataPrepare = temp0 - 1;
        } else {
            *dataPrepare = 255; /* set 255 will easy to found mistake */
            HDF_LOGE("err when calc phy timing");
        }
    } else {
        if (temp0 > 0) {
            *dataPrepare = temp0;
        } else {
            *dataPrepare = 0;
        }
    }
}

/* get global operation timing parameters. */
static void MipiTxDrvGetPhyTimingParam(MipiTxPhyTimingParamTag *tp)
{
    /* DATA0~3 TPRE-DELAY */
    /* 1: compensate */
    tp->dataTpreDelay = (g_actualPhyDataRate * TPRE_DELAY + ROUNDUP_VALUE) / INNER_PEROID - 1;
    /* CLK_TLPX */
    tp->clkTlpx = (g_actualPhyDataRate * TLPX + ROUNDUP_VALUE) / INNER_PEROID - 1; /* 1 is compensate */
    /* CLK_TCLK_PREPARE */
    MipiTxDrvGetPhyClkPrepare(&tp->clkTclkPrepare);
    /* CLK_TCLK_ZERO */
    if ((g_actualPhyDataRate * TCLK_ZERO + ROUNDUP_VALUE) / INNER_PEROID > 4) {    /* 4 is compensate */
        tp->clkTclkZero = (g_actualPhyDataRate * TCLK_ZERO + ROUNDUP_VALUE) / INNER_PEROID - 4;
    } else {
        tp->clkTclkZero = 0;       /* 0 is minimum */
    }
    /* CLK_TCLK_TRAIL */
    tp->clkTclkTrail = (g_actualPhyDataRate * TCLK_TRAIL + ROUNDUP_VALUE) / INNER_PEROID;
    /* DATA_TLPX */
    tp->dataTlpx = (g_actualPhyDataRate * TLPX + ROUNDUP_VALUE) / INNER_PEROID - 1; /* 1 is compensate */
    /* DATA_THS_PREPARE */
    MipiTxDrvGetPhyDataPrepare(&tp->dataThsPrepare);
    /* DATA_THS_ZERO */
    if ((g_actualPhyDataRate * THS_ZERO + ROUNDUP_VALUE) / INNER_PEROID > 4) {      /* 4 is compensate */
        tp->dataThsZero = (g_actualPhyDataRate * THS_ZERO + ROUNDUP_VALUE) / INNER_PEROID - 4;
    } else {
        tp->dataThsZero = 0;       /* 0 is minimum */
    }
    /* DATA_THS_TRAIL */
    tp->dataThsTrail = (g_actualPhyDataRate * THS_TRAIL + ROUNDUP_VALUE) / INNER_PEROID + 1; /* 1 is compensate */
}

/* set global operation timing parameters. */
static void MipiTxDrvSetPhyTimingParam(const MipiTxPhyTimingParamTag *tp)
{
    /* DATA0~3 TPRE-DELAY */
    SetPhyReg(DATA0_TPRE_DELAY, tp->dataTpreDelay);
    SetPhyReg(DATA1_TPRE_DELAY, tp->dataTpreDelay);
    SetPhyReg(DATA2_TPRE_DELAY, tp->dataTpreDelay);
    SetPhyReg(DATA3_TPRE_DELAY, tp->dataTpreDelay);

    /* CLK_TLPX */
    SetPhyReg(CLK_TLPX, tp->clkTlpx);
    /* CLK_TCLK_PREPARE */
    SetPhyReg(CLK_TCLK_PREPARE, tp->clkTclkPrepare);
    /* CLK_TCLK_ZERO */
    SetPhyReg(CLK_TCLK_ZERO, tp->clkTclkZero);
    /* CLK_TCLK_TRAIL */
    SetPhyReg(CLK_TCLK_TRAIL, tp->clkTclkTrail);

    /*
     * DATA_TLPX
     * DATA_THS_PREPARE
     * DATA_THS_ZERO
     * DATA_THS_TRAIL
     */
    SetPhyReg(DATA0_TLPX, tp->dataTlpx);
    SetPhyReg(DATA0_THS_PREPARE, tp->dataThsPrepare);
    SetPhyReg(DATA0_THS_ZERO, tp->dataThsZero);
    SetPhyReg(DATA0_THS_TRAIL, tp->dataThsTrail);
    SetPhyReg(DATA1_TLPX, tp->dataTlpx);
    SetPhyReg(DATA1_THS_PREPARE, tp->dataThsPrepare);
    SetPhyReg(DATA1_THS_ZERO, tp->dataThsZero);
    SetPhyReg(DATA1_THS_TRAIL, tp->dataThsTrail);
    SetPhyReg(DATA2_TLPX, tp->dataTlpx);
    SetPhyReg(DATA2_THS_PREPARE, tp->dataThsPrepare);
    SetPhyReg(DATA2_THS_ZERO, tp->dataThsZero);
    SetPhyReg(DATA2_THS_TRAIL, tp->dataThsTrail);
    SetPhyReg(DATA3_TLPX, tp->dataTlpx);
    SetPhyReg(DATA3_THS_PREPARE, tp->dataThsPrepare);
    SetPhyReg(DATA3_THS_ZERO, tp->dataThsZero);
    SetPhyReg(DATA3_THS_TRAIL, tp->dataThsTrail);

#ifdef MIPI_TX_DEBUG
    HDF_LOGI("\n==========phy timing parameters=======\n");
    HDF_LOGI("data_tpre_delay(0x30/40/50/60): 0x%x\n", tp->dataTpreDelay);
    HDF_LOGI("clk_tlpx(0x22): 0x%x\n", tp->clkTlpx);
    HDF_LOGI("clk_tclk_prepare(0x23): 0x%x\n", tp->clkTclkPrepare);
    HDF_LOGI("clk_tclk_zero(0x24): 0x%x\n", tp->clkTclkZero);
    HDF_LOGI("clk_tclk_trail(0x25): 0x%x\n", tp->clkTclkTrail);
    HDF_LOGI("data_tlpx(0x32/42/52/62): 0x%x\n", tp->dataTlpx);
    HDF_LOGI("data_ths_prepare(0x33/43/53/63): 0x%x\n", tp->dataThsPrepare);
    HDF_LOGI("data_ths_zero(0x34/44/54/64): 0x%x\n", tp->dataThsZero);
    HDF_LOGI("data_ths_trail(0x35/45/55/65): 0x%x\n", tp->dataThsTrail);
    HDF_LOGI("=========================\n");
#endif
}

/*
 * set data lp2hs,hs2lp time
 * set clk lp2hs,hs2lp time
 * unit: hsclk
 */
static void MipiTxDrvSetPhyHsLpSwitchTime(const MipiTxPhyTimingParamTag *tp)
{
    /* data lp2hs,hs2lp time */
    g_mipiTxRegsVa->PHY_TMR_CFG.u32 = ((tp->dataThsTrail - 1) << 16) +  /* 16 set register */
        tp->dataTpreDelay + tp->dataTlpx + tp->dataThsPrepare + tp->dataThsZero + 7; /* 7 from algorithm */
    /* clk lp2hs,hs2lp time */
    g_mipiTxRegsVa->PHY_TMR_LPCLK_CFG.u32 = ((31 + tp->dataThsTrail) << 16) + /* 31 from algorithm, 16 set register */
        tp->clkTlpx + tp->clkTclkPrepare + tp->clkTclkZero + 6; /* 6 from algorithm */
#ifdef MIPI_TX_DEBUG
    HDF_LOGI("PHY_TMR_CFG(0x9C): 0x%x\n", g_mipiTxRegsVa->PHY_TMR_CFG.u32);
    HDF_LOGI("PHY_TMR_LPCLK_CFG(0x98): 0x%x\n", g_mipiTxRegsVa->PHY_TMR_LPCLK_CFG.u32);
#endif
}

void MipiTxDrvSetPhyCfg(const ComboDevCfgTag *cfg)
{
    if (cfg == NULL) {
        return;
    }
    MipiTxPhyTimingParamTag tp = {0};

    /* set phy pll parameters setx */
    MipiTxDrvSetPhyPllSetX(cfg->phyDataRate);
    /* get global operation timing parameters */
    MipiTxDrvGetPhyTimingParam(&tp);
    /* set global operation timing parameters */
    MipiTxDrvSetPhyTimingParam(&tp);
    /* set hs switch to lp and lp switch to hs time  */
    MipiTxDrvSetPhyHsLpSwitchTime(&tp);
    /* edpi_cmd_size */
    g_mipiTxRegsVa->EDPI_CMD_SIZE.u32 = 0xF0;
    /* phy enable */
    g_mipiTxRegsVa->PHY_RSTZ.u32 = 0xf;
    if (cfg->outputMode == OUTPUT_MODE_CSI) {
        if (cfg->outputFormat == OUT_FORMAT_YUV420_8_BIT_NORMAL) {
            g_mipiTxRegsVa->DATATYPE0.u32 = 0x10218;
            g_mipiTxRegsVa->CSI_CTRL.u32 = 0x1111;
        } else if (cfg->outputFormat == OUT_FORMAT_YUV422_8_BIT) {
            g_mipiTxRegsVa->DATATYPE0.u32 = 0x1021E;
            g_mipiTxRegsVa->CSI_CTRL.u32 = 0x1111;
        }
    } else {
        if (cfg->outputFormat == OUT_FORMAT_RGB_16_BIT) {
            g_mipiTxRegsVa->DATATYPE0.u32 = 0x111213D;
            g_mipiTxRegsVa->CSI_CTRL.u32 = 0x10100;
        } else if (cfg->outputFormat == OUT_FORMAT_RGB_18_BIT) {
            g_mipiTxRegsVa->DATATYPE0.u32 = 0x111213D;
            g_mipiTxRegsVa->CSI_CTRL.u32 = 0x10100;
        } else if (cfg->outputFormat == OUT_FORMAT_RGB_24_BIT) {
            g_mipiTxRegsVa->DATATYPE0.u32 = 0x111213D;
            g_mipiTxRegsVa->CSI_CTRL.u32 = 0x10100;
        }
    }
    g_mipiTxRegsVa->PHY_RSTZ.u32 = 0XF;
    OsalMSleep(1);
    g_mipiTxRegsVa->LPCLK_CTRL.u32 = 0x0;
}

void MipiTxDrvGetDevStatus(MipiTxDevPhyTag *phyCtx)
{
    if (phyCtx == NULL) {
        return;
    }
    phyCtx->hactDet = g_mipiTxRegsVa->HORI0_DET.bits.hact_det;
    phyCtx->hallDet = g_mipiTxRegsVa->HORI0_DET.bits.hline_det;
    phyCtx->hbpDet  = g_mipiTxRegsVa->HORI1_DET.bits.hbp_det;
    phyCtx->hsaDet  = g_mipiTxRegsVa->HORI1_DET.bits.hsa_det;
    phyCtx->vactDet = g_mipiTxRegsVa->VERT_DET.bits.vact_det;
    phyCtx->vallDet = g_mipiTxRegsVa->VERT_DET.bits.vall_det;
    phyCtx->vsaDet  = g_mipiTxRegsVa->VSA_DET.bits.vsa_det;
}

static void SetOutputFormat(const ComboDevCfgTag *cfg)
{
    int colorCoding = 0;

    if (cfg->outputMode == OUTPUT_MODE_CSI) {
        if (cfg->outputFormat == OUT_FORMAT_YUV420_8_BIT_NORMAL) {
            colorCoding = 0xd;
        } else if (cfg->outputFormat == OUT_FORMAT_YUV422_8_BIT) {
            colorCoding = 0x1;
        }
    } else {
        if (cfg->outputFormat == OUT_FORMAT_RGB_16_BIT) {
            colorCoding = 0x0;
        } else if (cfg->outputFormat == OUT_FORMAT_RGB_18_BIT) {
            colorCoding = 0x3;
        } else if (cfg->outputFormat == OUT_FORMAT_RGB_24_BIT) {
            colorCoding = 0x5;
        }
    }
    g_mipiTxRegsVa->COLOR_CODING.u32 = colorCoding;
#ifdef MIPI_TX_DEBUG
    HDF_LOGI("SetOutputFormat: 0x%x\n", colorCoding);
#endif
}

static void SetVideoModeCfg(const ComboDevCfgTag *cfg)
{
    int videoMode;

    if (cfg->videoMode == NON_BURST_MODE_SYNC_PULSES) {
        videoMode = 0;
    } else if (cfg->videoMode == NON_BURST_MODE_SYNC_EVENTS) {
        videoMode = 1;
    } else {
        videoMode = 2; /* 2 register value */
    }
    if ((cfg->outputMode == OUTPUT_MODE_CSI) || (cfg->outputMode == OUTPUT_MODE_DSI_CMD)) {
        videoMode = 2; /* 2 register value */
    }
    g_mipiTxRegsVa->VID_MODE_CFG.u32 = 0x3f00 + videoMode;
}

static void SetTimingConfig(const ComboDevCfgTag *cfg)
{
    unsigned int hsa;
    unsigned int hbp;
    unsigned int hline;

    if (cfg->pixelClk == 0) {
        HDF_LOGE("cfg->pixelClk is 0, illegal.\n");
        return;
    }
    /* 125 from algorithm */
    hsa = g_actualPhyDataRate * cfg->syncInfo.vidHsaPixels * 125 / cfg->pixelClk;
    /* 125 from algorithm */
    hbp = g_actualPhyDataRate * cfg->syncInfo.vidHbpPixels * 125 / cfg->pixelClk;
    /* 125 from algorithm */
    hline = g_actualPhyDataRate * cfg->syncInfo.vidHlinePixels * 125 / cfg->pixelClk;
    g_mipiTxRegsVa->VID_HSA_TIME.u32 = hsa;
    g_mipiTxRegsVa->VID_HBP_TIME.u32 = hbp;
    g_mipiTxRegsVa->VID_HLINE_TIME.u32 = hline;
    g_mipiTxRegsVa->VID_VSA_LINES.u32 = cfg->syncInfo.vidVsaLines;
    g_mipiTxRegsVa->VID_VBP_LINES.u32 = cfg->syncInfo.vidVbpLines;
    g_mipiTxRegsVa->VID_VFP_LINES.u32 = cfg->syncInfo.vidVfpLines;
    g_mipiTxRegsVa->VID_VACTIVE_LINES.u32 = cfg->syncInfo.vidActiveLines;
#ifdef MIPI_TX_DEBUG
    HDF_LOGI("VID_HSA_TIME(0x48): 0x%x\n", hsa);
    HDF_LOGI("VID_HBP_TIME(0x4c): 0x%x\n", hbp);
    HDF_LOGI("VID_HLINE_TIME(0x50): 0x%x\n", hline);
    HDF_LOGI("VID_VSA_LINES(0x54): 0x%x\n", cfg->syncInfo.vidVsaLines);
    HDF_LOGI("VID_VBP_LINES(0x58): 0x%x\n", cfg->syncInfo.vidVbpLines);
    HDF_LOGI("VID_VFP_LINES(0x5c): 0x%x\n", cfg->syncInfo.vidVfpLines);
    HDF_LOGI("VID_VACTIVE_LINES(0x60): 0x%x\n", cfg->syncInfo.vidActiveLines);
#endif
}

void SetLaneConfig(const short laneId[], int len)
{
    int num = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (-1 != laneId[i]) {
            num++;
        }
    }
    g_mipiTxRegsVa->PHY_IF_CFG.u32 = num - 1;
}

void MipiTxDrvSetClkMgrCfg(void)
{
    if (g_actualPhyDataRate / 160 < 2) { /* 160 cal div, should not samller than 2 */
        g_mipiTxRegsVa->CLKMGR_CFG.u32 = 0x102;
    } else {
        g_mipiTxRegsVa->CLKMGR_CFG.u32 = 0x100 + (g_actualPhyDataRate + 159) / 160; /* 159 160 cal div */
    }
}

void MipiTxDrvSetControllerCfg(const ComboDevCfgTag *cfg)
{
    if (cfg == NULL) {
        return;
    }
    /* disable input */
    g_mipiTxRegsVa->OPERATION_MODE.u32 = 0x0;
    /* vc_id */
    g_mipiTxRegsVa->VCID.u32 = 0x0;
    /* output format,color coding */
    SetOutputFormat(cfg);
    /* txescclk,timeout */
    g_actualPhyDataRate = ((cfg->phyDataRate + MIPI_TX_REF_CLK - 1) / MIPI_TX_REF_CLK) * MIPI_TX_REF_CLK;
    MipiTxDrvSetClkMgrCfg();
    /* cmd transmission mode */
    g_mipiTxRegsVa->CMD_MODE_CFG.u32 = 0xffffff00;
    /* crc,ecc,eotp tran */
    g_mipiTxRegsVa->PCKHDL_CFG.u32 = 0x1c;
    /* gen_vcid_rx */
    g_mipiTxRegsVa->GEN_VCID.u32 = 0x0;
    /* mode config */
    g_mipiTxRegsVa->MODE_CFG.u32 = 0x1;
    /* video mode cfg */
    SetVideoModeCfg(cfg);
    if ((cfg->outputMode == OUTPUT_MODE_DSI_VIDEO) || (cfg->outputMode == OUTPUT_MODE_CSI)) {
        g_mipiTxRegsVa->VID_PKT_SIZE.u32 = cfg->syncInfo.vidPktSize;
    } else {
        g_mipiTxRegsVa->EDPI_CMD_SIZE.u32 = cfg->syncInfo.edpiCmdSize;
    }
    /* num_chunks/null_size */
    g_mipiTxRegsVa->VID_NUM_CHUNKS.u32 = 0x0;
    g_mipiTxRegsVa->VID_NULL_SIZE.u32 = 0x0;
    /* timing config */
    SetTimingConfig(cfg);
    /* invact,outvact time */
    g_mipiTxRegsVa->LP_CMD_TIM.u32 = 0x0;
    g_mipiTxRegsVa->PHY_TMR_CFG.u32 = 0x9002D;
    g_mipiTxRegsVa->PHY_TMR_LPCLK_CFG.u32 = 0x29002E;
    g_mipiTxRegsVa->EDPI_CMD_SIZE.u32 = 0xF0;
    /* lp_wr_to_cnt */
    g_mipiTxRegsVa->LP_WR_TO_CNT.u32 = 0x0;
    /* bta_to_cnt */
    g_mipiTxRegsVa->BTA_TO_CNT.u32 = 0x0;
    /* lanes */
    SetLaneConfig(cfg->laneId, LANE_MAX_NUM);
    /* phy_tx requlpsclk */
    g_mipiTxRegsVa->PHY_ULPS_CTRL.u32 = 0x0;
    /* int msk0 */
    g_mipiTxRegsVa->INT_MSK0.u32 = 0x0;
    /* pwr_up unreset */
    g_mipiTxRegsVa->PWR_UP.u32 = 0x0;
    g_mipiTxRegsVa->PWR_UP.u32 = 0xf;
}

static int MipiTxWaitCmdFifoEmpty(void)
{
    U_CMD_PKT_STATUS cmdPktStatus;
    unsigned int waitCnt;

    waitCnt = 0;
    do {
        cmdPktStatus.u32 = g_mipiTxRegsVa->CMD_PKT_STATUS.u32;
        waitCnt++;
        OsalUDelay(1);
        if (waitCnt >  MIPI_TX_READ_TIMEOUT_CNT) {
            HDF_LOGW("timeout when send cmd buffer \n");
            return -1;
        }
    } while (cmdPktStatus.bits.gen_cmd_empty == 0);
    return 0;
}

static int MipiTxWaitWriteFifoEmpty(void)
{
    U_CMD_PKT_STATUS cmdPktStatus;
    unsigned int waitCnt;

    waitCnt = 0;
    do {
        cmdPktStatus.u32 = g_mipiTxRegsVa->CMD_PKT_STATUS.u32;
        waitCnt++;
        OsalUDelay(1);
        if (waitCnt >  MIPI_TX_READ_TIMEOUT_CNT) {
            HDF_LOGW("timeout when send data buffer \n");
            return -1;
        }
    } while (cmdPktStatus.bits.gen_pld_w_empty == 0);
    return 0;
}

static int MipiTxWaitWriteFifoNotFull(void)
{
    U_CMD_PKT_STATUS cmdPktStatus;
    unsigned int waitCnt;

    waitCnt = 0;
    do {
        cmdPktStatus.u32 = g_mipiTxRegsVa->CMD_PKT_STATUS.u32;
        if (waitCnt > 0) {
            OsalUDelay(1);
            HDF_LOGW("write fifo full happened wait count = %d\n", waitCnt);
        }
        if (waitCnt >  MIPI_TX_READ_TIMEOUT_CNT) {
            HDF_LOGW("timeout when wait write fifo not full buffer \n");
            return -1;
        }
        waitCnt++;
    } while (cmdPktStatus.bits.gen_pld_w_full == 1);
    return 0;
}

/*
 * set payloads data by writing register
 * each 4 bytes in cmd corresponds to one register
 */
static void MipiTxDrvSetPayloadData(const unsigned char *cmd, unsigned short cmdSize)
{
    U_GEN_PLD_DATA genPldData;
    int i, j;

    genPldData.u32 = g_mipiTxRegsVa->GEN_PLD_DATA.u32;

    for (i = 0; i < (cmdSize / 4); i++) { /* 4 cmd once */
        genPldData.bits.gen_pld_b1 = cmd[i * 4]; /* 0 in 4 */
        genPldData.bits.gen_pld_b2 = cmd[i * 4 + 1]; /* 1 in 4 */
        genPldData.bits.gen_pld_b3 = cmd[i * 4 + 2]; /* 2 in 4 */
        genPldData.bits.gen_pld_b4 = cmd[i * 4 + 3]; /* 3 in 4 */
        MipiTxWaitWriteFifoNotFull();
        g_mipiTxRegsVa->GEN_PLD_DATA.u32 = genPldData.u32;
    }
    j = cmdSize % 4; /* remainder of 4 */
    if (j != 0) {
        if (j > 0) {
            genPldData.bits.gen_pld_b1 = cmd[i * 4]; /* 0 in 4 */
        }
        if (j > 1) {
            genPldData.bits.gen_pld_b2 = cmd[i * 4 + 1]; /* 1 in 4 */
        }
        if (j > 2) { /* bigger than 2 */
            genPldData.bits.gen_pld_b3 = cmd[i * 4 + 2]; /* 2 in 4 */
        }
        MipiTxWaitWriteFifoNotFull();
        g_mipiTxRegsVa->GEN_PLD_DATA.u32 = genPldData.u32;
    }
#ifdef MIPI_TX_DEBUG
    HDF_LOGI("\n=====set cmd=======\n");
    HDF_LOGI("GEN_PLD_DATA(0x70): 0x%x\n", genPldData);
#endif
}

int MipiTxDrvSetCmdInfo(const CmdInfoTag *cmdInfo)
{
    U_GEN_HDR genHdr;
    unsigned char *cmd = NULL;

    if (cmdInfo == NULL) {
        return -1;
    }
    genHdr.u32 = g_mipiTxRegsVa->GEN_HDR.u32;
    if (cmdInfo->cmd != NULL) {
        if (cmdInfo->cmdSize > 200 || cmdInfo->cmdSize == 0) { /* 200 is max cmd size */
            HDF_LOGE("set cmd size illegal, size =%d\n", cmdInfo->cmdSize);
            return -1;
        }
        cmd = (unsigned char *)OsalMemCalloc(cmdInfo->cmdSize);
        if (cmd == NULL) {
            HDF_LOGE("kmalloc fail,please check,need %d bytes\n", cmdInfo->cmdSize);
            return -1;
        }
        if (LOS_CopyToKernel(cmd, cmdInfo->cmdSize, cmdInfo->cmd, cmdInfo->cmdSize) != 0) {
            OsalMemFree(cmd);
            cmd = NULL;
            return -1;
        }
        MipiTxDrvSetPayloadData(cmd, cmdInfo->cmdSize);
        OsalMemFree(cmd);
        cmd = NULL;
    }
    genHdr.bits.gen_dt = cmdInfo->dataType;
    genHdr.bits.gen_wc_lsbyte = cmdInfo->cmdSize & 0xff;
    genHdr.bits.gen_wc_msbyte = (cmdInfo->cmdSize & 0xff00) >> 8; /* height 8 bits */
    g_mipiTxRegsVa->GEN_HDR.u32 = genHdr.u32;
    OsalUDelay(350);  /* wait 350 us transfer end */
    MipiTxWaitCmdFifoEmpty();
    MipiTxWaitWriteFifoEmpty();
    return 0;
}

static int MipiTxWaitReadFifoNotEmpty(void)
{
    U_INT_ST0 intSt0;
    U_INT_ST1 intSt1;
    unsigned int waitCnt;
    U_CMD_PKT_STATUS cmdPktStatus;

    waitCnt = 0;
    do {
        intSt1.u32 =  g_mipiTxRegsVa->INT_ST1.u32;
        intSt0.u32 =  g_mipiTxRegsVa->INT_ST0.u32;
        if ((intSt1.u32 & 0x3e) != 0) {
            HDF_LOGE("err happened when read data, int_st1 = 0x%x,int_st0 = %x\n", intSt1.u32, intSt0.u32);
            return -1;
        }
        if (waitCnt >  MIPI_TX_READ_TIMEOUT_CNT) {
            HDF_LOGW("timeout when read data\n");
            return -1;
        }
        waitCnt++;
        OsalUDelay(1);
        cmdPktStatus.u32 = g_mipiTxRegsVa->CMD_PKT_STATUS.u32;
    } while (cmdPktStatus.bits.gen_pld_r_empty == 0x1);
    return 0;
}

static int MipiTxWaitReadFifoEmpty(void)
{
    U_GEN_PLD_DATA pldData;
    U_INT_ST1 intSt1;
    unsigned int waitCnt;

    waitCnt = 0;
    do {
        intSt1.u32 = g_mipiTxRegsVa->INT_ST1.u32;
        if ((intSt1.bits.gen_pld_rd_err) == 0x0) {
            pldData.u32 = g_mipiTxRegsVa->GEN_PLD_DATA.u32;
        }
        waitCnt++;
        OsalUDelay(1);
        if (waitCnt >  MIPI_TX_READ_TIMEOUT_CNT) {
            HDF_LOGW("timeout when clear data buffer, the last read data is 0x%x \n", pldData.u32);
            return -1;
        }
    } while ((intSt1.bits.gen_pld_rd_err) == 0x0);
    return 0;
}

static int MipiTxSendShortPacket(unsigned char virtualChannel,
    short unsigned dataType, unsigned short  dataParam)
{
    U_GEN_HDR genHdr;

    genHdr.bits.gen_vc = virtualChannel;
    genHdr.bits.gen_dt = dataType;
    genHdr.bits.gen_wc_lsbyte = (dataParam & 0xff);
    genHdr.bits.gen_wc_msbyte = (dataParam & 0xff00) >> 8; /* height 8 bits */
    g_mipiTxRegsVa->GEN_HDR.u32 = genHdr.u32;
    if (MipiTxWaitCmdFifoEmpty() != 0) {
        return -1;
    }
    return 0;
}

static int MipiTxGetReadFifoData(unsigned int getDataSize, unsigned char *dataBuf)
{
    U_GEN_PLD_DATA pldData;
    unsigned int i, j;

    for (i = 0; i < getDataSize / 4; i++) {   /* 4byte once */
        if (MipiTxWaitReadFifoNotEmpty() != 0) {
            return -1;
        }
        pldData.u32 = g_mipiTxRegsVa->GEN_PLD_DATA.u32;
        dataBuf[i * 4] = pldData.bits.gen_pld_b1;     /* 0 in 4 */
        dataBuf[i * 4 + 1] = pldData.bits.gen_pld_b2; /* 1 in 4 */
        dataBuf[i * 4 + 2] = pldData.bits.gen_pld_b3; /* 2 in 4 */
        dataBuf[i * 4 + 3] = pldData.bits.gen_pld_b4; /* 3 in 4 */
    }

    j = getDataSize % 4; /* remainder of 4 */

    if (j != 0) {
        if (MipiTxWaitReadFifoNotEmpty() != 0) {
            return -1;
        }
        pldData.u32 = g_mipiTxRegsVa->GEN_PLD_DATA.u32;
        if (j > 0) {
            dataBuf[i * 4] = pldData.bits.gen_pld_b1; /* 0 in 4 */
        }
        if (j > 1) {
            dataBuf[i * 4 + 1] = pldData.bits.gen_pld_b2; /* 1 in 4 */
        }
        if (j > 2) { /* bigger than 2 */
            dataBuf[i * 4 + 2] = pldData.bits.gen_pld_b3; /* 2 in 4 */
        }
    }
    return 0;
}

void MipiTxReset(void)
{
    g_mipiTxRegsVa->PWR_UP.u32 = 0x0;
    g_mipiTxRegsVa->PHY_RSTZ.u32 = 0xd;
    OsalUDelay(1);
    g_mipiTxRegsVa->PWR_UP.u32 = 0x1;
    g_mipiTxRegsVa->PHY_RSTZ.u32 = 0xf;
    OsalUDelay(1);
    return;
}

int MipiTxDrvGetCmdInfo(GetCmdInfoTag *getCmdInfo)
{
    unsigned char *dataBuf = NULL;

    dataBuf = (unsigned char*)OsalMemAlloc(getCmdInfo->getDataSize);
    if (dataBuf == NULL) {
        return -1;
    }
    if (MipiTxWaitReadFifoEmpty() != 0) {
        goto fail0;
    }
    if (MipiTxSendShortPacket(0, getCmdInfo->dataType, getCmdInfo->dataParam) != 0) {
        goto fail0;
    }
    if (MipiTxGetReadFifoData(getCmdInfo->getDataSize, dataBuf) != 0) {
        /* fail will block mipi data lane ,so need reset  */
        MipiTxReset();
        goto fail0;
    }
    LOS_CopyFromKernel(getCmdInfo->getData, getCmdInfo->getDataSize, dataBuf, getCmdInfo->getDataSize);
    OsalMemFree(dataBuf);
    dataBuf = NULL;
    return 0;

fail0:
    OsalMemFree(dataBuf);
    dataBuf = NULL;
    return -1;
}

void MipiTxDrvEnableInput(const OutPutModeTag outputMode)
{
    if ((outputMode == OUTPUT_MODE_DSI_VIDEO) || (outputMode == OUTPUT_MODE_CSI)) {
        g_mipiTxRegsVa->MODE_CFG.u32 = 0x0;
    }
    if (outputMode == OUTPUT_MODE_DSI_CMD) {
        g_mipiTxRegsVa->CMD_MODE_CFG.u32 = 0x0;
    }
    /* enable input */
    g_mipiTxRegsVa->OPERATION_MODE.u32 = 0x80150000;
    g_mipiTxRegsVa->LPCLK_CTRL.u32 = 0x1;
    MipiTxReset();
}

void MipiTxDrvDisableInput(void)
{
    /* disable input */
    g_mipiTxRegsVa->OPERATION_MODE.u32 = 0x0;
    g_mipiTxRegsVa->CMD_MODE_CFG.u32 = 0xffffff00;
    /* command mode */
    g_mipiTxRegsVa->MODE_CFG.u32 = 0x1;
    g_mipiTxRegsVa->LPCLK_CTRL.u32 = 0x0;
    MipiTxReset();
}

static int MipiTxDrvRegInit(void)
{
    if (!g_mipiTxRegsVa) {
        g_mipiTxRegsVa = (MipiTxRegsTypeTag *)OsalIoRemap(MIPI_TX_REGS_ADDR, (unsigned int)MIPI_TX_REGS_SIZE);
        if (g_mipiTxRegsVa == NULL) {
            HDF_LOGE("remap mipi_tx reg addr fail\n");
            return -1;
        }
        g_regMapFlag = 1;
    }

    return 0;
}

static void MipiTxDrvRegExit(void)
{
    if (g_regMapFlag == 1) {
        if (g_mipiTxRegsVa != NULL) {
            OsalIoUnmap((void *)g_mipiTxRegsVa);
            g_mipiTxRegsVa = NULL;
        }
        g_regMapFlag = 0;
    }
}

static void MipiTxDrvHwInit(int smooth)
{
    unsigned long mipiTxCrgAddr;

    mipiTxCrgAddr = (unsigned long)OsalIoRemap(MIPI_TX_CRG, (unsigned long)0x4);
    /* mipi_tx gate clk enable */
    WriteReg32(mipiTxCrgAddr, 1, 0x1);
    /* reset */
    if (smooth == FALSE) {
        WriteReg32(mipiTxCrgAddr, 1 << 1, 0x1 << 1);
    }
    /* unreset */
    WriteReg32(mipiTxCrgAddr, 0 << 1, 0x1 << 1);
    /* ref clk */
    WriteReg32(mipiTxCrgAddr, 1 << 2, 0x1 << 2); /* 2 clk bit */
    OsalIoUnmap((void *)mipiTxCrgAddr);
}

int MipiTxDrvInit(int smooth)
{
    int ret;

    ret = MipiTxDrvRegInit();
    if (ret < 0) {
        HDF_LOGE("MipiTxDrvRegInit fail!\n");
        return -1;
    }
    MipiTxDrvHwInit(smooth);
    return 0;
}

void MipiTxDrvExit(void)
{
    MipiTxDrvRegExit();
}

ComboDevCfgTag *GetDevCfg(struct MipiDsiCntlr *cntlr)
{
    static ComboDevCfgTag dev;
    int i;

    if (cntlr == NULL) {
        return NULL;
    }
    dev.devno = cntlr->devNo;
    dev.outputMode = (OutPutModeTag)cntlr->mode;
    dev.videoMode = (VideoModeTag)cntlr->burstMode;
    dev.outputFormat = (OutputFormatTag)cntlr->format;
    dev.syncInfo.vidPktSize = cntlr->timing.xPixels;
    dev.syncInfo.vidHsaPixels = cntlr->timing.hsaPixels;
    dev.syncInfo.vidHbpPixels = cntlr->timing.hbpPixels;
    dev.syncInfo.vidHlinePixels = cntlr->timing.hlinePixels;
    dev.syncInfo.vidVsaLines = cntlr->timing.vsaLines;
    dev.syncInfo.vidVbpLines = cntlr->timing.vbpLines;
    dev.syncInfo.vidVfpLines = cntlr->timing.vfpLines;
    dev.syncInfo.vidActiveLines = cntlr->timing.ylines;
    dev.syncInfo.edpiCmdSize = cntlr->timing.edpiCmdSize;
    dev.phyDataRate = cntlr->phyDataRate;
    dev.pixelClk = cntlr->pixelClk;
    for (i = 0; i < LANE_MAX_NUM; i++) {
        dev.laneId[i] = -1;   /* -1 : not use */
    }
    for (i = 0; i < cntlr->lane; i++) {
        dev.laneId[i] = i;
    }
    return &dev;
}

static int MipiTxCheckCombDevCfg(const ComboDevCfgTag *devCfg)
{
    int i;
    int validLaneId[LANE_MAX_NUM] = {0, 1, 2, 3};

    if (devCfg->devno != 0) {
        HDF_LOGE("mipi_tx dev devno err!\n");
        return -1;
    }
    for (i = 0; i < LANE_MAX_NUM; i++) {
        if ((devCfg->laneId[i] != validLaneId[i]) && (devCfg->laneId[i] != MIPI_TX_DISABLE_LANE_ID)) {
            HDF_LOGE("mipi_tx dev laneId %d err!\n", devCfg->laneId[i]);
            return -1;
        }
    }
    if ((devCfg->outputMode != OUTPUT_MODE_CSI) && (devCfg->outputMode != OUTPUT_MODE_DSI_VIDEO) &&
        (devCfg->outputMode != OUTPUT_MODE_DSI_CMD)) {
        HDF_LOGE("mipi_tx dev outputMode %d err!\n", devCfg->outputMode);
        return -1;
    }
    if ((devCfg->videoMode != BURST_MODE) && (devCfg->videoMode != NON_BURST_MODE_SYNC_PULSES) &&
        (devCfg->videoMode != NON_BURST_MODE_SYNC_EVENTS)) {
        HDF_LOGE("mipi_tx dev videoMode %d err!\n", devCfg->videoMode);
        return -1;
    }
    if ((devCfg->outputFormat != OUT_FORMAT_RGB_16_BIT) && (devCfg->outputFormat != OUT_FORMAT_RGB_18_BIT) &&
        (devCfg->outputFormat != OUT_FORMAT_RGB_24_BIT) && (devCfg->outputFormat !=
        OUT_FORMAT_YUV420_8_BIT_NORMAL) && (devCfg->outputFormat != OUT_FORMAT_YUV420_8_BIT_LEGACY) &&
        (devCfg->outputFormat != OUT_FORMAT_YUV422_8_BIT)) {
        HDF_LOGE("mipi_tx dev outputFormat %d err!\n", devCfg->outputFormat);
        return -1;
    }
    return 0;
}

static int MipiTxSetComboDevCfg(const ComboDevCfgTag *devCfg)
{
    int ret;

    ret = MipiTxCheckCombDevCfg(devCfg);
    if (ret < 0) {
        HDF_LOGE("mipi_tx check combo_dev config failed!\n");
        return ret;
    }
    /* set controler config */
    MipiTxDrvSetControllerCfg(devCfg);
    /* set phy config */
    MipiTxDrvSetPhyCfg(devCfg);
    g_enCfg = true;
    return ret;
}

int32_t Hi35xxSetCntlrCfg(struct MipiDsiCntlr *cntlr)
{
    ComboDevCfgTag *dev = GetDevCfg(cntlr);

    if (dev == NULL) {
        return -1;
    }
    return MipiTxSetComboDevCfg(dev);
}

static int MipiTxCheckSetCmdInfo(const CmdInfoTag *cmdInfo)
{
    if (!g_enCfg) {
        HDF_LOGE("mipi_tx dev has not config!\n");
        return -1;
    }
    if (cmdInfo->devno != 0) {
        HDF_LOGE("mipi_tx devno %d err!\n", cmdInfo->devno);
        return -1;
    }
    /* When cmd is not NULL, cmd_size means the length of cmd or it means cmd and addr */
    if (cmdInfo->cmd != NULL) {
        if (cmdInfo->cmdSize > MIPI_TX_SET_DATA_SIZE) {
            HDF_LOGE("mipi_tx dev cmd_size %d err!\n", cmdInfo->cmdSize);
            return -1;
        }
    }
    return 0;
}

static int MipiTxSetCmd(const CmdInfoTag *cmdInfo)
{
    int ret;

    ret = MipiTxCheckSetCmdInfo(cmdInfo);
    if (ret < 0) {
        HDF_LOGE("mipi_tx check combo_dev config failed!\n");
        return ret;
    }
    return MipiTxDrvSetCmdInfo(cmdInfo);
}

int32_t Hi35xxSetCmd(struct MipiDsiCntlr *cntlr, struct DsiCmdDesc *cmd)
{
    CmdInfoTag cmdInfo;

    (void)cntlr;
    if (cmd == NULL) {
        return HDF_FAILURE;
    }
    cmdInfo.devno = 0;
    if (cmd->dataLen > 2) {                     /* 2: use long data type */
        cmdInfo.cmdSize = cmd->dataLen;
        cmdInfo.dataType = cmd->dataType;       /* 0x29: long data type */
        cmdInfo.cmd = cmd->payload;
    } else if (cmd->dataLen == 2) {             /* 2: use short data type */
        uint16_t tmp = cmd->payload[1];      /* 3: payload */
        tmp = (tmp & 0x00ff) << 8;           /* 0x00ff , 8: payload to high */
        tmp = 0xff00 & tmp;
        tmp = tmp | cmd->payload[0];         /* 2: reg addr */
        cmdInfo.cmdSize = tmp;
        cmdInfo.dataType = cmd->dataType;       /* 0x23: short data type */
        cmdInfo.cmd = NULL;
    } else if (cmd->dataLen == 1) {
        cmdInfo.cmdSize = cmd->payload[0];   /* 2: reg addr */
        cmdInfo.dataType = cmd->dataType;       /* 0x05: short data type */
        cmdInfo.cmd = NULL;
    } else {
        // error
        return HDF_FAILURE;
    }
    return MipiTxSetCmd(&cmdInfo);
}

static int MipiTxCheckGetCmdInfo(const GetCmdInfoTag *getCmdInfo)
{
    if (!g_enCfg) {
        HDF_LOGE("mipi_tx dev has not config!\n");
        return -1;
    }
    if (getCmdInfo->devno != 0) {
        HDF_LOGE("mipi_tx dev devno %d err!\n", getCmdInfo->devno);
        return -1;
    }
    if ((getCmdInfo->getDataSize == 0) || (getCmdInfo->getDataSize > MIPI_TX_GET_DATA_SIZE)) {
        HDF_LOGE("mipi_tx dev getDataSize %d err!\n", getCmdInfo->getDataSize);
        return -1;
    }
    if (getCmdInfo->getData == NULL) {
        HDF_LOGE("mipi_tx dev getData is null!\n");
        return -1;
    }
    return 0;
}

static int MipiTxGetCmd(GetCmdInfoTag *getCmdInfo)
{
    int ret;

    ret = MipiTxCheckGetCmdInfo(getCmdInfo);
    if (ret < 0) {
        HDF_LOGE("mipi_tx check combo_dev config failed!\n");
        return ret;
    }
    return MipiTxDrvGetCmdInfo(getCmdInfo);
}

int32_t Hi35xxGetCmd(struct MipiDsiCntlr *cntlr, struct DsiCmdDesc *cmd, uint32_t readLen, uint8_t *out)
{
    GetCmdInfoTag cmdInfo;

    (void)cntlr;
    if (cmd == NULL || out == NULL) {
        return HDF_FAILURE;
    }
    cmdInfo.devno = 0;
    cmdInfo.dataType = cmd->dataType;
    cmdInfo.dataParam = cmd->payload[0];
    cmdInfo.getDataSize = readLen;
    cmdInfo.getData = out;
    return MipiTxGetCmd(&cmdInfo);
}

void Hi35xxToLp(struct MipiDsiCntlr *cntlr)
{
    (void)cntlr;
    MipiTxDrvDisableInput();
}

void Hi35xxToHs(struct MipiDsiCntlr *cntlr)
{
    ComboDevCfgTag *dev = GetDevCfg(cntlr);

    if (dev == NULL) {
        return;
    }
    MipiTxDrvEnableInput(dev->outputMode);
}

static struct MipiDsiCntlr g_mipiTx = {
    .devNo = 0,
    .setCntlrCfg = Hi35xxSetCntlrCfg,
    .setCmd = Hi35xxSetCmd,
    .getCmd = Hi35xxGetCmd,
    .toHs = Hi35xxToHs,
    .toLp = Hi35xxToLp,
};

static int32_t Hi35xxMipiTxInit(struct HdfDeviceObject *device)
{
    int32_t ret;
    (void)device;
    ret = MipiDsiRegisterCntlr(&g_mipiTx);
    if (ret != HDF_SUCCESS) {
        return ret;
    }
    HDF_LOGI("load mipi_tx driver 1212!\n");
    return MipiTxDrvInit(0);
}

static void Hi35xxMipiTxRelease(struct HdfDeviceObject *device)
{
    (void)device;
    MipiTxDrvExit();
}

struct HdfDriverEntry g_mipiTxDriverEntry = {
    .moduleVersion = 1,
    .Init = Hi35xxMipiTxInit,
    .Release = Hi35xxMipiTxRelease,
    .moduleName = "HDF_MIPI_TX",
};
HDF_INIT(g_mipiTxDriverEntry);
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
