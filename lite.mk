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

SOC_COMPANY := $(subst $\",,$(LOSCFG_DEVICE_COMPANY))
SOC_PLATFORM := $(subst $\",,$(LOSCFG_PLATFORM))
SOC_BOARD := $(subst $\",,$(LOSCFG_PRODUCT_NAME))
CUR_COMPILER := llvm
CUR_OS := ohos


ifeq ($(SOC_BOARD), ipcamera_hi3516dv300_liteos)
    SOC_BOARD := hi3516dv300
endif
ifeq ($(SOC_BOARD), ipcamera_hi3518ev300_liteos)
    SOC_BOARD := hi3518ev300
endif

HISILICON_DRIVERS_ROOT := $(LITEOSTOPDIR)/../../device/$(SOC_COMPANY)/drivers/
HISILICON_DRIVERS_SOURCE_ROOT := $(LITEOSTOPDIR)/../../device/$(SOC_COMPANY)/drivers/huawei_proprietary/

BUILD_FROM_SOURCE := $(shell if [ -d $(HISILICON_DRIVERS_SOURCE_ROOT) ]; then echo y; else echo n; fi)

HDF_INCLUDE += -I $(LITEOSTOPDIR)/../../device/$(SOC_COMPANY)/$(SOC_BOARD)/sdk_liteos/config/board/include/
HDF_INCLUDE += -I $(LITEOSTOPDIR)/../../device/$(SOC_COMPANY)/$(SOC_BOARD)/sdk_liteos/config/board/include/hisoc

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_I2C), y)
    LITEOS_BASELIB += -lhdf_i2c
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/i2c
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_SPI), y)
    LITEOS_BASELIB += -lhdf_spi
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/spi
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_GPIO), y)
    LITEOS_BASELIB += -lhdf_gpio
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/gpio
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_WATCHDOG), y)
    LITEOS_BASELIB += -lhdf_watchdog
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/watchdog
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_SDIO), y)
    LITEOS_BASELIB += -lhdf_sdio
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/sdio
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_EMMC), y)
    LITEOS_BASELIB += -lhdf_emmc
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/emmc
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_RTC), y)
    LITEOS_BASELIB += -lhdf_rtc
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/rtc
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_UART), y)
    LITEOS_BASELIB += -lhdf_uart
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/uart
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_PWM), y)
    LITEOS_BASELIB += -lhdf_pwm
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/pwm
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_HISI_SDK), y)
    LITEOS_BASELIB += -lhdf_hisi_sdk
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/hisi_sdk
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_MIPI_DSI), y)
    LITEOS_BASELIB += -lhdf_mipi_dsi
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/mipi_dsi
endif
endif

ifeq ($(LOSCFG_DRIVERS_HDF_PLATFORM_DMAC), y)
    LITEOS_BASELIB += -lhdf_dmac
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    += $(HISILICON_DRIVERS_SOURCE_ROOT)/dmac
endif
endif

ifeq ($(BUILD_FROM_SOURCE), y)
ifeq ($(LOSCFG_DRIVERS_HIEDMAC), y)
    LITEOS_BASELIB    += -lhiedmac
    LIB_SUBDIRS       += $(HISILICON_DRIVERS_SOURCE_ROOT)/hiedmac
    LITEOS_HIDMAC_INCLUDE   += -I $(HISILICON_DRIVERS_SOURCE_ROOT)/hiedmac/include
endif
endif

ifeq ($(LOSCFG_DRIVERS_HIETH_SF), y)
    LITEOS_BASELIB    += -lhieth-sf
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS       +=  $(HISILICON_DRIVERS_SOURCE_ROOT)/hieth-sf
    LITEOS_HIETH_SF_INCLUDE += -I $(HISILICON_DRIVERS_SOURCE_ROOT)/hieth-sf/include
endif
    LITEOS_HIETH_SF_INCLUDE += -I $(HISILICON_DRIVERS_ROOT)/include/hieth-sf/include
endif


# mmc dirvers
ifeq ($(LOSCFG_DRIVERS_MMC), y)
    MMC_HOST_DIR := himci
    LITEOS_BASELIB  += -lmmc
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS        += $(HISILICON_DRIVERS_SOURCE_ROOT)/mmc
    LITEOS_MMC_INCLUDE += -I $(HISILICON_DRIVERS_SOURCE_ROOT)/mmc/include
else
    LITEOS_MMC_INCLUDE += -I $(HISILICON_DRIVERS_ROOT)/include/mmc/include
endif
endif

# mtd drivers
ifeq ($(LOSCFG_DRIVERS_MTD), y)
    LITEOS_BASELIB    += -lmtd_common
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS       += $(HISILICON_DRIVERS_SOURCE_ROOT)/mtd/common
    LITEOS_MTD_SPI_NOR_INCLUDE  +=  -I $(HISILICON_DRIVERS_SOURCE_ROOT)/mtd/common/include
else
    LITEOS_MTD_SPI_NOR_INCLUDE  +=  -I $(HISILICON_DRIVERS_ROOT)/include/mtd/common/include
endif

    ifeq ($(LOSCFG_DRIVERS_MTD_SPI_NOR), y)
    ifeq ($(LOSCFG_DRIVERS_MTD_SPI_NOR_HISFC350), y)
        NOR_DRIVER_DIR := hisfc350
    else ifeq ($(LOSCFG_DRIVERS_MTD_SPI_NOR_HIFMC100), y)
        NOR_DRIVER_DIR := hifmc100
    endif

    ifeq ($(BUILD_FROM_SOURCE), y)
        LITEOS_BASELIB   += -lspinor_flash
        LIB_SUBDIRS      += $(HISILICON_DRIVERS_SOURCE_ROOT)/mtd/spi_nor
        LITEOS_MTD_SPI_NOR_INCLUDE  +=  -I $(HISILICON_DRIVERS_SOURCE_ROOT)/mtd/spi_nor/include
    else
        ifeq ($(LOSCFG_SHELL), y)
            LITEOS_BASELIB   += -lspinor_flash
        else
            LITEOS_BASELIB   += -lspinor_flash_noshell
        endif
        LITEOS_MTD_SPI_NOR_INCLUDE  +=  -I $(HISILICON_DRIVERS_ROOT)/include/mtd/spi_nor/include
    endif

    endif

    ifeq ($(LOSCFG_DRIVERS_MTD_NAND), y)
        NAND_DRIVER_DIR := hifmc100

        LITEOS_BASELIB   += -lnand_flash
        LIB_SUBDIRS      += $(HISILICON_DRIVERS_SOURCE_ROOT)/mtd/nand
        LITEOS_MTD_NAND_INCLUDE  +=  -I $(HISILICON_DRIVERS_ROOT)/mtd/nand/include
    endif
endif

# wifi dirvers
ifeq ($(LOSCFG_DRIVERS_HDF_WIFI), y)
    LITEOS_BASELIB += -lhdf_vendor_wifi
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    +=  $(HISILICON_DRIVERS_SOURCE_ROOT)/wifi/driver
endif

ifeq ($(LOSCFG_DRIVERS_HI3881), y)
    LITEOS_BASELIB += -lhi3881
ifeq ($(BUILD_FROM_SOURCE), y)
    LIB_SUBDIRS    +=  $(HISILICON_DRIVERS_SOURCE_ROOT)/wifi/driver/hi3881
endif
endif
endif

ifeq ($(BUILD_FROM_SOURCE), n)
LITEOS_LD_PATH += -L$(HISILICON_DRIVERS_ROOT)/libs/$(CUR_OS)/$(CUR_COMPILER)/$(SOC_PLATFORM)
endif
