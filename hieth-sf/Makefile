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

include $(LITEOSTOPDIR)/config.mk
include $(LITEOSTOPDIR)/../../drivers/adapter/khdf/liteos/lite.mk

MODULE_NAME := $(notdir $(shell pwd))

LOCAL_SRCS :=  $(wildcard src/*.c)
LOCAL_SRCS +=  $(wildcard adapter/*.c)

LOCAL_FLAGS := -I include/internal
LOCAL_FLAGS += -I $(LITEOSTOPDIR)/../../drivers/framework/model/network/ethernet/include
LOCAL_FLAGS += -I $(LITEOSTOPDIR)/../../drivers/framework/include/ethernet
LOCAL_FLAGS += -I $(LITEOSTOPDIR)/../../device/hisilicon/drivers/hieth-sf/adapter

include $(HDF_DRIVER)

