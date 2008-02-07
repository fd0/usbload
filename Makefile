
# microcontroller and project specific settings
TARGET = usbload
F_CPU = 16000000UL
MCU = atmega168

SRC = usbload.c
ASRC = usbdrv/usbdrvasm.S
OBJECTS += $(patsubst %.c,%.o,${SRC})
OBJECTS += $(patsubst %.S,%.o,${ASRC})
HEADERS += $(shell echo *.h)
# CFLAGS += -Werror
LDFLAGS += -L/usr/local/avr/avr/lib
CFLAGS += -Iusbdrv -I.
CFLAGS += -DHARDWARE_REV=$(HARDWARE_REV)
ASFLAGS += -x assembler-with-cpp
ASFLAGS += -Iusbdrv -I.

# no safe mode checks
AVRDUDE_FLAGS += -u

# set name for dependency-file
MAKEFILE = Makefile

# bootloader section start
# (see datasheet)
ifeq ($(MCU),atmega168)
	# atmega168 with 1024 words bootloader:
	# bootloader section starts at 0x1c00 (word-address) == 0x3800 (byte-address)
	BOOT_SECTION_START = 0x3800
endif

LDFLAGS += -Wl,--section-start=.text=$(BOOT_SECTION_START)
CFLAGS += -DBOOT_SECTION_START=$(BOOT_SECTION_START)


include avr.mk

.PHONY: all

all: $(TARGET).hex $(TARGET).lss
	@echo "==============================="
	@echo "$(TARGET) compiled for: $(MCU)"
	@echo -n "size is: "
	@$(SIZE) -A $(TARGET).hex | grep "\.sec1" | tr -s " " | cut -d" " -f2
	@echo "==============================="

$(TARGET): $(OBJECTS) $(TARGET).o

%.o: $(HEADERS)

.PHONY: install

# install: program-serial-$(TARGET) program-serial-eeprom-$(TARGET)
install: program-isp-$(TARGET)

.PHONY: clean clean-$(TARGET) clean-uploadtest

clean: clean-$(TARGET)

clean-$(TARGET):
	$(RM) $(TARGET) $(OBJECTS)

clean-bootloader:
	$(MAKE) -C bootloader clean

clean-uploadtest:
	rm -f datatestfile{512,14k}.raw

.PHONY: depend

depend:
	$(CC) $(CFLAGS) -M $(CDEFS) $(CINCS) $(SRC) $(ASRC) >> $(MAKEFILE).dep

datatestfile14k.raw:
	dd if=/dev/urandom of=datatestfile14k.raw bs=1 count=$((1024*14))

datatestfile512.raw:
	dd if=/dev/urandom of=datatestfile512.raw bs=1 count=512

uploadtest: datatestfile14k.raw datatestfile512.raw
	$(AVRDUDE) -p $(AVRDUDE_MCU) -c usbasp -P usb -U flash:w:datatestfile14k.raw
	$(AVRDUDE) $(AVRDUDE_FLAGS) -c $(ISP_PROG) -P $(ISP_DEV) -U flash:v:datatestfile14k.raw
	sleep 2
	$(AVRDUDE) -p $(AVRDUDE_MCU) -c usbasp -P usb -U eeprom:w:datatestfile512.raw
	$(AVRDUDE) $(AVRDUDE_FLAGS) -c $(ISP_PROG) -P $(ISP_DEV) -U eeprom:v:datatestfile512.raw

-include $(MAKEFILE).dep
