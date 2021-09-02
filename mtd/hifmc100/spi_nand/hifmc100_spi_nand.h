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

#ifndef HIFMC100_NAND_H
#define HIFMC100_NAND_H

#include "hifmc100.h"
#include "mtd_spi_nand.h"

int32_t HifmcCntlrSearchSpinandInfo(struct HifmcCntlr *cntlr, struct SpiFlash *spi);
int32_t HifmcCntlrInitSpinandDevice(struct HifmcCntlr *cntlr, struct SpiFlash *spi);
int32_t HifmcCntlrReadIdSpiNand(struct HifmcCntlr *cntlr, uint8_t cs, uint8_t *id, size_t len);
int32_t HifmcCntlrInitOob(struct HifmcCntlr *cntlr, struct SpiFlash *spi);
uint8_t HifmcCntlrReadSpinandReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd);

int32_t HifmcCntlrDevFeatureOp(struct HifmcCntlr *cntlr, struct SpiFlash *spi,
    bool isGet, uint8_t addr, uint8_t *val);

int32_t SpinandGetSpiOps(struct SpiFlash *spi);
#endif /* HIFMC100_NAND_H */
