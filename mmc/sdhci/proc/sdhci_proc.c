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

#include "proc_fs.h"
#include "mmc_corex.h"
#include "mmc_sd.h"

#define MCI_PARENT       "mci"
#define MCI_STATS_PROC   "mci_info"

#define CLOCK_UINT_MAX_NUM 4
#define SDHCI_HOST_NUM 2

static struct ProcDirEntry *g_procMciDir = NULL;

static bool ProcGetMmcCardUnplugged(struct MmcCntlr *mmc)
{
    if (mmc->caps.bits.nonremovable == 0) {
        /*
         * removable, for sd cards only.
         * slots for sd cards can detect if the card is plugged.
         */
        return (MmcCntlrDevPluged(mmc) == false);
    } else {
        /* slots for sdio or emmc can't detect if the card is plugged in hardware. */
        return (mmc->curDev == NULL);
    }
}

static int32_t ProcStatsCardPluggedPrint(struct MmcCntlr *mmc, struct SeqBuf *s)
{
    int32_t status;

    if (ProcGetMmcCardUnplugged(mmc) == true) {
        status = LosBufPrintf(s, ": unplugged_disconnected\n");
        if (status != 0) {
            return HDF_FAILURE;
        }
        return HDF_ERR_NOT_SUPPORT;
    }

    status = LosBufPrintf(s, ": plugged");
    if (status != 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t ProcStatsCardConnectedPrint(struct MmcDevice *card, struct SeqBuf *s)
{
    int32_t status;

    if ((card == NULL) || card->state.bits.present == 0) {
        status = LosBufPrintf(s, "_disconnected\n");
        return HDF_FAILURE;
    }

    status = LosBufPrintf(s, "_connected\n");
    if (status != 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static char *ProcGetCardType(enum MmcDevType type)
{
    static char *cardTypeStr[MMC_DEV_INVALID + 1] = {
        "EMMC card",
        "SD card",
        "SDIO card",
        "SD combo (IO+mem) card",
        "unknown"
    };

    if (type >= MMC_DEV_INVALID) {
        return cardTypeStr[MMC_DEV_INVALID];
    } else {
        return cardTypeStr[type];
    }
}

static int32_t ProcStatsCardTypePrint(struct MmcDevice *card, struct SeqBuf *s)
{
    int32_t status;
    const char *type = NULL;

    status = LosBufPrintf(s, "\tType: %s", ProcGetCardType(card->type));
    if (status != 0) {
        return HDF_FAILURE;
    }
    if (card->state.bits.blockAddr > 0) {
        type = "SDHC";
        if (card->state.bits.sdxc > 0) {
            type = "SDXC";
        }
        status = LosBufPrintf(s, "(%s)\n", type);
        if (status != HDF_SUCCESS) {
            return HDF_FAILURE;
        }
    } else {
        status = LosBufPrintf(s, "\n");
        if (status != HDF_SUCCESS) {
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

static uint32_t ProcAnalyzeClockScale(uint32_t clock, uint32_t *val)
{
    uint32_t scale = 0;
    uint32_t tmp = clock;

    *val = tmp;
    while (tmp > 0) {
        tmp = tmp / 1000;
        if (tmp > 0 && scale < (CLOCK_UINT_MAX_NUM - 1)) {
            *val = tmp;
            scale++;
        }
    }
    return scale;
}

static int32_t ProcStatsUhsPrint(struct MmcDevice *card, struct SeqBuf *s)
{
    int32_t status;
    struct SdDevice *dev = (struct SdDevice *)card;
    const char *busSpeedMode = "";
    static char *speedsOfUHS[] = {
        "SDR12 ", "SDR25 ", "SDR50 ", "SDR104 ", "DDR50 "
    };

    if (card->type == MMC_DEV_SD || card->type == MMC_DEV_COMBO) {
        if (card->state.bits.uhs > 0 &&
            dev->busSpeedMode < (sizeof(speedsOfUHS) / sizeof(speedsOfUHS[0]))) {
            busSpeedMode = speedsOfUHS[dev->busSpeedMode];
        }
    }
    status = LosBufPrintf(s, "\tMode: %s%s%s%s\n",
        (card->state.bits.uhs > 0) ? "UHS " : ((card->state.bits.highSpeed > 0) ? "HS " : ""),
        (card->state.bits.hs200 > 0) ? "HS200 " :
        ((card->state.bits.hs400 > 0 || card->state.bits.hs400es > 0) ? "HS400 " : ""),
        (card->state.bits.ddrMode > 0) ? "DDR" : "",
        busSpeedMode);
    if (status != 0) {
        return HDF_FAILURE;
    }

    return HDF_SUCCESS;
}

static int32_t ProcStatsSpeedPrint(struct MmcDevice *card, struct SeqBuf *s)
{
    int32_t status;
    uint32_t speedClass = 0;
    uint32_t uhsSpeedGrade = 0;
    static char *speed = NULL;
    struct SdDevice *dev = (struct SdDevice *)card;

    if (card->type == MMC_DEV_SD || card->type == MMC_DEV_COMBO) {
        speedClass = dev->reg.ssr.speedClass;
        uhsSpeedGrade = dev->reg.ssr.uhsSpeedGrade;
    }

    if (speedClass > SD_SSR_SPEED_CLASS_4) {
        status = LosBufPrintf(s, "\tSpeed Class: Class Reserved\n");
    } else {
        if (speedClass == SD_SSR_SPEED_CLASS_4) {
            speedClass = SD_SSR_SPEED_CLASS_4_VAL;
        } else {
            speedClass *= (SD_SSR_SPEED_CLASS_1_VAL / SD_SSR_SPEED_CLASS_1);
        }
        status = LosBufPrintf(s, "\tSpeed Class: Class %d\n", speedClass);
    }
    if (status != 0) {
        return HDF_FAILURE;
    }

    if (uhsSpeedGrade == SD_SSR_UHS_SPEED_GRADE_0) {
        speed = "Less than 10MB/sec(0h)";
    } else if (uhsSpeedGrade == SD_SSR_UHS_SPEED_GRADE_1) {
        speed = "10MB/sec and above(1h)";
    } else if (uhsSpeedGrade == SD_SSR_UHS_SPEED_GRADE_3) {
        speed = "30MB/sec and above(3h)";
    } else {
        speed = "Reserved";
    }
    status = LosBufPrintf(s, "\tUhs Speed Grade: %s\n", speed);
    if (status != 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t ProcStatsClkPrint(uint32_t clock, struct SeqBuf *s)
{
    int32_t status;
    uint32_t clockScale;
    uint32_t clockValue = 0;
    static char *clockUnit[CLOCK_UINT_MAX_NUM] = {
        "Hz",
        "KHz",
        "MHz",
        "GHz"
    };

    clockScale = ProcAnalyzeClockScale(clock, &clockValue);
    status = LosBufPrintf(s, "\tHost work clock: %d%s\n", clockValue, clockUnit[clockScale]);
    if (status != 0) {
        return HDF_FAILURE;
    }

    status = LosBufPrintf(s, "\tCard support clock: %d%s\n", clockValue, clockUnit[clockScale]);
    if (status != 0) {
        return HDF_FAILURE;
    }

    status = LosBufPrintf(s, "\tCard work clock: %d%s\n", clockValue, clockUnit[clockScale]);
    if (status != 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t ProcStatsCidPrint(struct MmcDevice *card, struct SeqBuf *s)
{
    int32_t status;

    status = LosBufPrintf(s, "\tCard cid: %08x%08x%08x%08x\n",
        card->reg.rawCid[0], card->reg.rawCid[1], card->reg.rawCid[2], card->reg.rawCid[3]);
    if (status != 0) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t ProcStatsCardInfoPrint(struct MmcCntlr *mmc, struct SeqBuf *s)
{
    struct MmcDevice *card = NULL;
    int32_t status;

    status = ProcStatsCardPluggedPrint(mmc, s);
    if (status != HDF_SUCCESS) {
        if (status == HDF_ERR_NOT_SUPPORT) {
            return HDF_SUCCESS;
        }
        return HDF_FAILURE;
    }

    card = mmc->curDev;
    if (card == NULL) {
        return HDF_SUCCESS;
    }
    status = ProcStatsCardConnectedPrint(card, s);
    if (status != HDF_SUCCESS) {
        return status;
    }
    status = ProcStatsCardTypePrint(card, s);
    if (status != HDF_SUCCESS) {
        return status;
    }
    status = ProcStatsUhsPrint(card, s);
    if (status != HDF_SUCCESS) {
        return status;
    }
    status = ProcStatsSpeedPrint(card, s);
    if (status != HDF_SUCCESS) {
        return status;
    }
    status = ProcStatsClkPrint(card->workPara.clock, s);
    if (status != HDF_SUCCESS) {
        return status;
    }
    status = LosBufPrintf(s, "\tCard error count: 0\n");
    if (status != 0) {
        return HDF_FAILURE;
    }
    status = ProcStatsCidPrint(card, s);
    if (status != HDF_SUCCESS) {
        return HDF_FAILURE;
    }
    return HDF_SUCCESS;
}

static int32_t ProcStatsSeqPrint(struct SeqBuf *s)
{
    int32_t status;
    uint32_t hostId;
    struct MmcCntlr *mmc = NULL;

    for (hostId = 0; hostId < SDHCI_HOST_NUM; hostId++) {
        mmc = MmcCntlrGetByNr(hostId);
        if (mmc == NULL || mmc->priv == NULL) {
            status = LosBufPrintf(s, "\nMCI%d: invalid\n", hostId);
            if (status != 0) {
                return HDF_FAILURE;
            }
            continue;
        }
        status = LosBufPrintf(s, "\nMCI%d", hostId);
        if (status != 0) {
            return HDF_FAILURE;
        }
        status = ProcStatsCardInfoPrint(mmc, s);
        if (status != 0) {
            return HDF_FAILURE;
        }
    }
    return HDF_SUCCESS;
}

static int32_t ProcMciShow(struct SeqBuf *m, void *v)
{
    (void)v;
    return ProcStatsSeqPrint(m);
}

static const struct ProcFileOperations g_mciProcFops = {
    .read = ProcMciShow,
};

int32_t ProcMciInit(void)
{
    struct ProcDirEntry *handle = NULL;

    g_procMciDir = ProcMkdir(MCI_PARENT, NULL);
    if (g_procMciDir == NULL) {
        HDF_LOGE("create directory error!");
        return HDF_FAILURE;
    }

    handle = CreateProcEntry(MCI_STATS_PROC, 0, (struct ProcDirEntry *)g_procMciDir);
    if (handle == NULL) {
        HDF_LOGE("create mount pointer error!");
        return HDF_FAILURE;
    }

    handle->procFileOps = &g_mciProcFops;
    return HDF_SUCCESS;
}
