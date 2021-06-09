#
# Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
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

MODULE_NAME := hdf_mmc
HDF_MMC_INC = $(LITEOSTOPDIR)/../../device/hisilicon/drivers/mmc

LOCAL_CFLAGS += $(HDF_INCLUDE)

ifeq ($(LOSCFG_PLATFORM_HI3516DV300), y)
    LOCAL_INCLUDE = $(HDF_MMC_INC)/himci_v200/proc
    LOCAL_SRCS += himci_v200/himci.c
    LOCAL_SRCS += himci_v200/proc/himci_proc.c
endif

ifeq ($(LOSCFG_PLATFORM_HI3518EV300), y)
    LOCAL_INCLUDE = $(HDF_MMC_INC)/sdhci/proc
    LOCAL_SRCS += sdhci/sdhci.c
    LOCAL_SRCS += sdhci/proc/sdhci_proc.c
endif

LOCAL_CFLAGS += -fstack-protector-strong -Wextra -Wall -Werror -fsigned-char -fno-strict-aliasing -fno-common

include $(HDF_DRIVER)

