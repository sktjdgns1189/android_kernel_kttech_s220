#
# Makefile for nfc devices
#

obj-$(CONFIG_PN544_NFC)		+= pn544.o
obj-$(CONFIG_NFC_PN533)		+= pn533.o
obj-$(CONFIG_NFC_WILINK)	+= nfcwilink.o
obj-$(CONFIG_KTTECH_PN544_NFC)		+= pn544-kttech.o

ccflags-$(CONFIG_NFC_DEBUG) := -DDEBUG
