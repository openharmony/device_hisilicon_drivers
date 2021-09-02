#
# Copyright (c) 2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include $(LITEOSTOPDIR)/config.mk
include $(LITEOSTOPDIR)/../../drivers/adapter/khdf/liteos/lite.mk

MODULE_NAME := hdf_mtd_hisilicon

LOCAL_CFLAGS += $(HDF_INCLUDE)

ifneq ($(LOSCFG_PLATFORM_HI3516DV300)$(LOSCFG_PLATFORM_HI3518EV300),)
    LOCAL_INCLUDE += hifmc100/common
    LOCAL_INCLUDE += hifmc100/spi_nor
    LOCAL_INCLUDE += hifmc100/spi_nand
    LOCAL_SRCS += hifmc100/common/hifmc100.c
    LOCAL_SRCS += hifmc100/spi_nor/hifmc100_spi_nor.c
    LOCAL_SRCS += hifmc100/spi_nor/mx25l.c
    LOCAL_SRCS += hifmc100/spi_nor/w25qh.c
    LOCAL_SRCS += hifmc100/spi_nor/hifmc100_spi_nor_ids.c
    LOCAL_SRCS += hifmc100/spi_nand/hifmc100_spi_nand.c
    LOCAL_SRCS += hifmc100/spi_nand/hifmc100_spi_nand_ids.c
endif

LOCAL_CFLAGS += -fstack-protector-strong -Wextra -Wall -Werror -fsigned-char -fno-strict-aliasing -fno-common

include $(HDF_DRIVER)
