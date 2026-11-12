# NeonPine firmware (user flash at 0x08000000).
#   make            build neonpine.bin
#   make flash      flash over USB through the rv003usb bootloader (the running app is rebooted into it)
#   make flash-swd  flash over SWD with a WCH-LinkE on the J2 pogo pads

all : build

TARGET:=neonpine
RV003USB:=../deps/rv003usb
CH32FUN:=$(RV003USB)/ch32fun/ch32fun
TARGET_MCU:=CH32V003

# usb.o = rv003usb.c + the HID report hooks, built on its own (rule below); everything else is one LTO build.
ADDITIONAL_C_FILES+=$(RV003USB)/rv003usb/rv003usb.S usb.o anim.c
EXTRA_CFLAGS:=-I. -I$(RV003USB)/lib -I$(RV003USB)/rv003usb -I$(CH32FUN)/../extralibs
# Size (~150 B with GCC 14): shared prologue/epilogue routines, and no global CSE (it keeps addresses in
# registers; with RV32E's 16 that mostly means spills). Kept out of the USB interrupt path (usb.o).
# Not -mtune=size: its cost model allows misaligned loads/stores, which the CH32V003 core traps on
# (the store read of the tree name crashed the chip into a watchdog reset loop).
SIZE_CFLAGS:=-msave-restore -fno-gcse
EXTRA_CFLAGS+=$(SIZE_CFLAGS)

# -C b003boot: talk to the USB bootloader (1209:B003); minichlink reboots a running app (1209:D003) into it first.
FLASH_COMMAND=$(MINICHLINK)/minichlink -C b003boot -w $(TARGET).bin flash -b

include $(CH32FUN)/ch32fun.mk

# The last 1 kB of flash is the settings / custom animation store (anim.h): the image must stay below it.
STORE_ADDR_OFFSET := 15360
build : size-check
size-check : $(TARGET).bin
	@sz=$$(wc -c < $(TARGET).bin); if [ $$sz -gt $(STORE_ADDR_OFFSET) ]; then \
		echo "$(TARGET).bin is $$sz bytes: it would overwrite the store at 0x3C00 (max $(STORE_ADDR_OFFSET))"; rm -f $(TARGET).bin; exit 1; \
	else echo "$(TARGET).bin: $$sz of $(STORE_ADDR_OFFSET) bytes"; fi

flash : size-check cv_flash

flash-swd : $(TARGET).bin $(MINICHLINK)/minichlink
	$(MINICHLINK)/minichlink -w $< flash -b

# The USB path with ch32fun's stock flags and no LTO (see usb.c).
usb.o : usb.c usb_config.h np_usb.h anim.h funconfig.h $(RV003USB)/rv003usb/rv003usb.c $(RV003USB)/rv003usb/rv003usb.h
	$(PREFIX)-gcc -c -o $@ $< $(filter-out -flto $(SIZE_CFLAGS),$(CFLAGS))

clean : cv_clean
	rm -f usb.o
