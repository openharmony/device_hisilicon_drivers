#
# Copyright (C) 2021 HiSilicon (Shanghai) Technologies CO., LIMITED.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

include $(LITEOSTOPDIR)/config.mk
include $(LITEOSTOPDIR)/../../drivers/adapter/khdf/liteos/lite.mk

MODULE_NAME := hdf_vendor_wifi
HDF_WLAN_SIDO_INC=$(LITEOSTOPDIR)/../../device/hisilicon/drivers/huawei_proprietary/mmc/include
VENDOR_WIFI_ROOT = .
HDM_WIFI_ROOT = $(LITEOSTOPDIR)/../../drivers/framework/model/network/wifi

LOCAL_INCLUDE = $(VENDOR_WIFI_ROOT)/core \
                $(VENDOR_WIFI_ROOT)/kits \
                $(HDM_WIFI_ROOT)/include \
                $(HDM_WIFI_ROOT)/platform/include \
                $(HDM_WIFI_ROOT)/core \
                $(LITEOSTOPDIR)/../../drivers/peripheral/wlan/client/include \
                ${HDF_WLAN_SIDO_INC}/internal \
                ${HDF_WLAN_SIDO_INC}/internal/mmc \
                $(VENDOR_WIFI_ROOT)/hi3881/linuxkpi/include

HDF_WIFI_CORE = $(VENDOR_WIFI_ROOT)/core
HDF_WIFI_KITS = $(VENDOR_WIFI_ROOT)/kits
HDF_WIFI_CFG = $(VENDOR_WIFI_ROOT)/config

LOCAL_SRCS += $(HDF_WIFI_CORE)/hdf_wlan_sdio_adapt.c

LOCAL_CFLAGS += -fstack-protector-strong -fno-common -fsigned-char -fno-strict-aliasing

include $(HDF_DRIVER)
