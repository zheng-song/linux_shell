# Makefile - makefile for target/src/drv/usb/target
#
# modification history
# --------------------
# 01d,28oct04,tpw  Add include of defs.docs for doc build settings.
# 01c,08oct04,hch  Update for documentation build
# 01b,30sep04,pdg  DMA flag added for NET2280 tcd
# 01a,08sep04,ami  Changes for NET2280
# 01f,01oct04,hch  Merge from Bangalore deveopement branch after netcip dma
#                  testing
# 01e,20sep04,pmr  Netchip & docs build
# 01d,08sep04,ami  Changes for NET2280
# 01c,20jul04,???  usbTargMsLib.o added
# 01b,01jun04,???  Changes for Interrupt Mode
# 01a,15Mar2004,Amit   created
#
# DESCRIPTION
# This file contains the makefile rules for building the usb target driver library
#
#*/

TGT_DIR=$(WIND_BASE)/target

EXTRA_INCLUDE = -DuISP1582_POLLING -DuDMA_SUPPORTED -DuTEST_PRINT_PERFORMANCE \
		-DuNET2280_DMA_SUPPORTED
DOC_FILES= usbTcdPdiusbd12InitExit.c usbTcdIsp1582InitExit.c usbTcdNET2280InitExit.c usbTargKbdLib.c usbTargPrnLib.c usbTargMsLib.c usbTargRbcLib.c usbTargRbcCmd.c

LIB_BASE_NAME=usb

OBJS=       usbTargKbdLib.o  usbTargPrnLib.o usbTcdPdiusbd12InitExit.o \
usbTcdIsp1582InitExit.o usbTargMsLib.o usbTargRbcLib.o usbTargRbcCmd.o \
usbTcdNET2280InitExit.o


include $(TGT_DIR)/h/make/rules.library

DOC_DIR         = $(DOCS_ROOT)/vxworks/ref/usb
DOC_CHAPTER     = USB_libraries
DOC_OPTS        = $(DOC_OPTS_MAN) -category USB

DOC_AREA_USB	= usb_drivers
-include $(TGT_DIR)/h/make/config/comps/usb/defs.docs
