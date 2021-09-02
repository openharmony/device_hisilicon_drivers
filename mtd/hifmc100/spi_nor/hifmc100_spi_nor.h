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

#ifndef HIFMC100_NOR_H
#define HIFMC100_NOR_H

#include "hifmc100.h"

int32_t HifmcCntlrSearchSpinorInfo(struct HifmcCntlr *cntlr, struct SpiFlash *spi);
int32_t HifmcCntlrInitSpinorDevice(struct HifmcCntlr *cntlr, struct SpiFlash *spi);
int32_t HifmcCntlrReadIdSpiNor(struct HifmcCntlr *cntlr, uint8_t cs, uint8_t *id, size_t len);
uint8_t HifmcCntlrReadSpinorReg(struct HifmcCntlr *cntlr, struct SpiFlash *spi, uint8_t cmd);

int32_t SpinorGetSpiOps(struct SpiFlash *spi);
#endif /* HIFMC100_NOR_H */
