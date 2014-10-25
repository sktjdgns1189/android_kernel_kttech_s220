############################################################################
#
# Makefile
#
# Copyright (C) 2011 ALPS ELECTRIC CO., LTD. All Rights Reserved.
#
# This software is licensed under the terms of the GNU General Public
# License version 2, as published by the Free Software Foundation, and
# may be copied, distributed, and modified under those terms.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
############################################################################

# Debug Option
#CFLAGS_MODULE += -DALPS_DEBUG

# Select Build Module
#MODULES = alps-input.o hscdtd004a_i2c.o bma150_i2c.o
#MODULES = alps-input.o hscdtd004a_spi.o bma150_spi.o
#MODULES = alps-input.o hscdtd004a_i2c.o mma8450_i2c.o
#MODULES = alps-input.o hscdtd004a_i2c.o kxtf9_i2c.o

# Kernel Root Directory
#ROOTDIR = $(ANDROID_R)/linux-2.6.35-at-alpha2-android
#ROOTDIR = $(ANDROID_R)/kernel-beagleboard-xm

#ifneq ($(KERNELRELEASE), )

#obj-m := $(MODULES)
obj-$(CONFIG_KTTECH_SENSOR_CAPELLA_O3)			+= capella_kttech.o
obj-$(CONFIG_KTTECH_COMPASS_ALPS)			+= alps-input.o
obj-$(CONFIG_KTTECH_COMPASS_ALPS)			+= hscdtd004a_i2c.o
obj-$(CONFIG_KTTECH_COMPASS_ALPS)			+= bma150_i2c.o


#else

#include $(ROOTDIR)/.config

#all: modules

#modules:
#	make -C $(ROOTDIR) M=${shell pwd} modules

#clean:
#	-rm -f *.[oas] *.ko *.mod.c .*.d .*.tmp .*.cmd *.symvers
#	-rm -rf .tmp_versions modules.order

#endif

