################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Top-level application make file.
#
################################################################################
# \copyright
# Copyright (2025), Cypress Semiconductor Corporation (an Infineon company)
# SPDX-License-Identifier: Apache-2.0
# 
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
################################################################################


################################################################################
# Basic Configuration
################################################################################

# Type of ModusToolbox Makefile Options include:
#
# COMBINED    -- Top Level Makefile usually for single standalone application
# APPLICATION -- Top Level Makefile usually for multi project application
# PROJECT     -- Project Makefile under Application
#
MTB_TYPE=COMBINED

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make library-manager' from command line), which will also update Eclipse IDE launch
# configurations.
TARGET=KIT_FX20_FMC_001

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=mtb-example-fx20-uvc-uac-4k-camera

# Name of toolchain to use. Options include:
#
# GCC_ARM -- GCC is available as part of ModusToolbox Setup program
# ARM     -- ARM Clang Compiler (must be installed separately)
#
# To use the ARM toolchain, ensure the CY_COMPILER_ARM_DIR environment variable is set to the compiler's directory (absolute path).
# For example, the default path for ARMCLANG from a Keil installation is typically C:/Keil_v5/ARM/ARMCLANG.
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# Generate Bootloader enabled hex. Options include:
#
# yes -- Hex with bootloader region
# no  -- Hex without bootloader region
BLENABLE ?= yes

# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
#
# If CONFIG is manually edited, ensure to update or regenerate launch configurations
# for your IDE.
CONFIG=Release

# If set to "true" or "1", display full command-lines when building.
VERBOSE=

# Name of CORE to use. Options include:
#
# CM4  -- Cortex M4
# CM0P -- Cortex M0+
CORE ?= CM4

################################################################################
# Advanced Configuration
################################################################################

# Include the bsp makefile so that we can get the MPN selection details.
-include bsps/TARGET_$(TARGET)/bsp.mk

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS=FREERTOS RTOS_AWARE FX20_$(CORE)

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES=

# Add additional defines to the build process (without a leading -D).
DEFINES= \
        CYUSB4024_BZXI=1 \
        BCLK__BUS_CLK__HZ=75000000 \
        DEBUG_INFRA_EN=1 \
        FREERTOS_ENABLE=1 \
        USB3_LPM_ENABLE=0 \
        USBFS_LOGS_ENABLE=1 \
        FPGA_ENABLE=1

# Select the default USB connection speed from amongst:
# CY_USBD_USB_DEV_SS_GEN2X2, CY_USBD_USB_DEV_SS_GEN2, CY_USBD_USB_DEV_SS_GEN1X2,
# CY_USBD_USB_DEV_SS_GEN1, CY_USBD_USB_DEV_HS and CY_USBD_USB_DEV_FS.
DEFINES += USB_CONN_TYPE=CY_USBD_USB_DEV_SS_GEN2X2


# Set to 1 for using LVCMOS, 0 for LVDS */
DEFINES += LVCMOS_EN=0
# Set to 1 for using LVCMOS DDR, 0 for LVCMOS SDR */
DEFINES += LVCMOS_DDR_EN=0

# Set to 1 for Sony_Imx715
DEFINES += SONY_IMX715_EN=1
# Set to 1 for ThineSensor
DEFINES += THINE_EN=0

# Set to 1 for using port_1, 0 for port_0
DEFINES += PORT1_EN=0

# Set to 1 for Metadata insertion, 0 for FW header addition
DEFINES += INMD_EN=0

# Enable both for Precision Time Measurement, Vendor IF is required to send LDM request using control center.
DEFINES += PTM_ENABLE=0

# Set to 1 for WideLink, 0 for NarrowLink mode
DEFINES += WL_EN=1

# Set to 1 for enabling thread interleaving for Two thread per EP, 0 for single thread per EP
DEFINES += THREAD_INTERLEAVE_EN=0

DEFINES += CY_VENDOR_IF_EN=0
DEFINES += CDC_INTERFACE_EN=0

# Set to 1 to enable Audio I/F. Default stereo is enabled
DEFINES += AUDIO_IF_EN=1

# Set to 1 to restrict USB connection to USBHS.
DEFINES += FORCE_USBHS=0

# Conditionally append BLOAD_ENABLE=1 if BLENABLE is set to yes
ifeq ($(BLENABLE), yes)
    DEFINES += BLOAD_ENABLE=1
endif

# Select softfp or hardfp floating point. Default is softfp.
ifeq ($(CORE), CM4)
    ifeq ($(TOOLCHAIN), GCC_ARM)
        VFP_SELECT= softfp
    else ifeq ($(TOOLCHAIN), ARM)
        VFP_SELECT= soft
    endif
else ifeq ($(CORE), CM0P)
    VFP_SELECT=
endif

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ifeq ($(CORE), CM4)
    ifeq ($(TOOLCHAIN), GCC_ARM)
        CFLAGS= -Os -Og -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -ffunction-sections -fdata-sections -g
    else ifeq ($(TOOLCHAIN), ARM)
        CFLAGS= -Os -Og -mcpu=Cortex-M4 -mfpu=fpv4-sp-d16 -mthumb -ffunction-sections -fdata-sections -g \
                --target=arm-arm-none-eabi -fno-rtti -fno-exceptions -Wno-error -fshort-wchar -fshort-enums
    endif
else ifeq ($(CORE), CM0P)
    ifeq ($(TOOLCHAIN), GCC_ARM)
        CFLAGS= -Os -Og -mcpu=cortex-m0plus -mthumb -ffunction-sections -fdata-sections -g
    else ifeq ($(TOOLCHAIN), ARM)
        CFLAGS= -Os -Og -mcpu=Cortex-M0plus -mthumb -ffunction-sections -fdata-sections -g \
                --target=arm-arm-none-eabi -fno-rtti -fno-exceptions -Wno-error -fshort-wchar -fshort-enums
    endif
endif

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS=

# Additional / custom linker flags.
ifeq ($(CORE), CM4)
    ifeq ($(TOOLCHAIN), GCC_ARM)
        LDFLAGS=-Wl,--start-group -mcpu=cortex-m4 -mthumb --entry=Reset_Handler -Wl,--gc-sections -g -ffunction-sections -finline-functions -Os -Wl,--end-group
    else ifeq ($(TOOLCHAIN), ARM)
        LDFLAGS=--cpu=Cortex-M4 --entry=Reset_Handler --diag_suppress=L6329W,L6314W 
    endif
else ifeq ($(CORE), CM0P)
    ifeq ($(TOOLCHAIN), GCC_ARM)
        LDFLAGS=-Wl,--start-group -mcpu=cortex-m0plus -mthumb --entry=Reset_Handler -Wl,--gc-sections -g -ffunction-sections -finline-functions -Os -Wl,--end-group
    else ifeq ($(TOOLCHAIN), ARM)
        LDFLAGS=--cpu=Cortex-M0plus --entry=Reset_Handler --diag_suppress=L6329W,L6314W
    endif
endif

# Additional / custom libraries to link in to the application.
LDLIBS=

# Default device selection in case information is not provided by the BSP.
DEVICE ?= CYUSB4024-BZXI

# Set the path to linker script based on MPN selection.
ifeq ($(filter $(DEVICE),CYUSB4024-BZXI CYUSB4014-FCAXI CYUSB4013-FCAXI CYUSB4011-FCAXI CYUSB3084-FCAXI CYUSB3083-FCAXI CYUSB3284-FCAXI CYUSB4021-FCAXI),$(DEVICE))
    LDPATH=LinkerScripts/default
else
    $(error Unsupported MPN: $(DEVICE))
endif

# Path to the linker script to use (if empty, use the default linker script).
ifeq ($(CORE), CM4)
    ifeq ($(BLENABLE), yes)
        # Use loadable linker script for CM4 core
        LINKER_SCRIPT = $(if $(filter GCC_ARM,$(TOOLCHAIN)),$(LDPATH)/fx_cm4_loadable.ld,$(LDPATH)/fx_cm4_loadable.sct)
    else
        # Use dual linker script for CM4 core
        LINKER_SCRIPT = $(if $(filter GCC_ARM,$(TOOLCHAIN)),$(LDPATH)/fx_cm4.ld,$(LDPATH)/fx_cm4_dual.sct)
    endif
else ifeq ($(CORE),CM0P)
    ifeq ($(BLENABLE), yes)
        # Use loadable linker script for CM0P core
        LINKER_SCRIPT = $(if $(filter GCC_ARM,$(TOOLCHAIN)),$(LDPATH)/fx_cm0plus_loadable.ld,$(LDPATH)/fx_cm0plus_loadable.sct)
    else
        # Use linker script for CM0P core
        LINKER_SCRIPT = $(if $(filter GCC_ARM,$(TOOLCHAIN)),$(LDPATH)/fx_cm0plus.ld,$(LDPATH)/fx_cm0plus.sct)
    endif
endif

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
# Generate HEX file. SHA256 checksum is appended when BLENABLE is set to yes.
ifeq ($(BLENABLE), yes)
    POSTBUILD=\
        $(CY_MCUELFTOOL) --sign build/$(TARGET)/$(CONFIG)/$(APPNAME).elf SHA256 --output build/$(TARGET)/$(CONFIG)/$(APPNAME).sha.elf && \
        $(OBJCOPY) -O ihex --gap-fill 0 build/$(TARGET)/$(CONFIG)/$(APPNAME).sha.elf build/$(TARGET)/$(CONFIG)/$(APPNAME).hex
else
    ifeq ($(TOOLCHAIN), GCC_ARM)
        POSTBUILD=\
            $(OBJCOPY) -O ihex build/$(TARGET)/$(CONFIG)/$(APPNAME).elf build/$(TARGET)/$(CONFIG)/$(APPNAME).hex
    else ifeq ($(TOOLCHAIN), ARM)
        POSTBUILD=\
            $(FROMELF) --i32combined --base=0x10000000 -o build/$(TARGET)/$(CONFIG)/$(APPNAME).hex build/$(TARGET)/$(CONFIG)/$(APPNAME).elf
    endif
endif

################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> field
# begins with $$ASSET_REPO$$, then the repo is deposited in the path specified by
# the CY_GETLIBS_SHARED_PATH variable. The default location is one directory level
# above the current app directory.
# This is used with CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

# Absolute path to the compiler's "bin" directory. The variable name depends on the
# toolchain used for the build. Refer to the ModusToolbox user guide to get the correct
# variable name for the toolchain used in your build.
#
# The default path depends on the selected TOOLCHAIN and is set in the Make recipe.
CY_COMPILER_GCC_ARM_DIR?=

# Locate ModusToolbox helper tools folders in default installation
# locations for Windows, Linux, and macOS.
CY_WIN_HOME=$(subst \,/,$(USERPROFILE))
CY_TOOLS_PATHS ?= $(wildcard \
    $(CY_WIN_HOME)/ModusToolbox/tools_* \
    $(HOME)/ModusToolbox/tools_* \
    /Applications/ModusToolbox/tools_*)

# If you install ModusToolbox software in a custom location, add the path to its
# "tools_X.Y" folder (where X and Y are the version number of the tools
# folder). Make sure you use forward slashes.
CY_TOOLS_PATHS+=

# Default to the newest installed tools folder, or the users override (if it's
# found).
CY_TOOLS_DIR=$(lastword $(sort $(wildcard $(CY_TOOLS_PATHS))))

ifeq ($(CY_TOOLS_DIR),)
    $(error Unable to find any of the available CY_TOOLS_PATHS -- $(CY_TOOLS_PATHS). On Windows, use forward slashes.)
endif

$(info Tools Directory: $(CY_TOOLS_DIR))

# Path to Elf tool directory.
CY_MCUELFTOOL_DIR=$(wildcard $(CY_TOOLS_DIR)/cymcuelftool-*)

# CY MCU ELF tool executable path.
ifeq ($(OS), Windows_NT)
    CY_MCUELFTOOL=$(CY_MCUELFTOOL_DIR)/bin/cymcuelftool.exe
else
    CY_MCUELFTOOL=$(CY_MCUELFTOOL_DIR)/bin/cymcuelftool
endif

# Path to FROMELF tool.
FROMELF=$(MTB_TOOLCHAIN_ARM__ELF2BIN)


# Path to OBJCOPY tool.
OBJCOPY=$(MTB_TOOLCHAIN_GCC_ARM__ELF2BIN)

include $(CY_TOOLS_DIR)/make/start.mk
