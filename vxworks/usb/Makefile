# Makefile - makefile for target/src/drv/usb
#
# modification history
# --------------------
# 01i,28oct04,tpw  Add include of defs.docs for doc build settings.
# 01h,05dec01,wef  removed usbAcmLib usbKlsiEnd and usbNC1080End references
#		   since they have been moved to unsupported for now.
# 01g,25oct01,tam  added repackaging support
# 01f,11oct01,dat  SPR 20403, CC_OPTIM_DRIVER
# 01e,18sep01,wef  merge from wrs.tor2_0.usb1_1-f for veloce
# 01d,14feb01,wef  added the NetChip 1080 driver.
# 01c,05dec00,wef  added new class drivers - Mass Storage (CBI and Bulk 
#		   only), 2 different ethernet class drivers (Klsi and 
#		   Pegusus chips), and the ACM class (modem) driver 
# 01b,27mar00,wef  updated for fcs, added speaker removed uhcilib - reflects
#		   directory structure change - this file was shuffled.
# 01a,25jan00,wef  created
#
# DESCRIPTION
# This file contains the makefile rules for building the usb library
#
#*/


TGT_DIR=$(WIND_BASE)/target

DOC_FILES=	usbBulkDevLib.c usbCbiUfiDevLib.c \
		usbHcdOhciLib.c usbHcdUhciLib.c usbKeyboardLib.c \
		usbMouseLib.c usbPegasusEnd.c \
		usbPrinterLib.c usbSpeakerLib.c \
		usbTransUnitData.c usbTransUnitInit.c usbTransUnitMisc.c usbTransUnitStd.c



LIB_BASE_NAME	= usb

OBJS=		usbBulkDevLib.o usbCbiUfiDevLib.o \
		usbHcdOhciLib.o usbHcdUhciLib.o usbKeyboardLib.o \
		usbMouseLib.o usbPegasusEnd.o \
		usbPrinterLib.o usbSpeakerLib.o \
		usbTransUnitData.o usbTransUnitInit.o usbTransUnitMisc.o usbTransUnitStd.o


include $(TGT_DIR)/h/make/rules.library

CC_OPTIM = $(CC_OPTIM_DRIVER)

DOC_DIR         = $(DOCS_ROOT)/vxworks/ref/usb
DOC_CHAPTER     = USB_libraries
DOC_OPTS        = $(DOC_OPTS_MAN) -category USB

DOC_AREA_USB	= usb_drivers
-include $(TGT_DIR)/h/make/config/comps/usb/defs.docs
