# Bare-metal EEPROM dumper for JN5169 (no Zigbee stack)
#   make              read-only dumper      -> eeprom_dump_JN5169.bin
#   make RESTORE=1    dumper + restore of ../../../eeprom_backup.bin
#                                           -> eeprom_restore_JN5169.bin
# The restore image contains the Zigbee network key: keep it local.
JENNIC_CHIP        ?= JN5169
JENNIC_CHIP_FAMILY ?= JN516x
JENNIC_SDK          = JN-SW-4170
JENNIC_STACK        = None
SDK_BASE_DIR       ?= ../../../../../../../NXP/bstudio_nxp/sdk/$(JENNIC_SDK)
EEPROM_BACKUP      ?= ../../../eeprom_backup.bin

include $(SDK_BASE_DIR)/Chip/Common/Build/config.mk
include $(SDK_BASE_DIR)/Stack/Common/Build/config.mk
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/Common/Include

ifeq ($(RESTORE), 1)
TARGET   = eeprom_restore
CFLAGS  += -DEEPROM_IMAGE
INCFLAGS += -I.
OBJDIR   = restore
else
TARGET   = eeprom_dump
OBJDIR   = dump
endif

vpath % ../Source

all: $(TARGET)_$(JENNIC_CHIP).bin

eeprom_image.h: $(EEPROM_BACKUP)
	python -I -c "import sys; d=open(sys.argv[1],'rb').read(); \
open(sys.argv[2],'w').write('/* generated from %s */\n#define EEPROM_IMAGE_SIZE %d\nstatic const uint8 au8EepromImage[EEPROM_IMAGE_SIZE] = {\n%s\n};\n' % \
(sys.argv[1], len(d), ',\n'.join(','.join('0x%02x' % b for b in d[i:i+16]) for i in range(0, len(d), 16))))" $< $@

$(OBJDIR)/main.o: main.c $(if $(filter 1,$(RESTORE)),eeprom_image.h)
	mkdir -p $(OBJDIR)
	$(CC) -c -o $@ $(CFLAGS) $(INCFLAGS) $<

$(TARGET)_$(JENNIC_CHIP).elf: $(OBJDIR)/main.o
	$(CC) -Wl,--gc-sections -Wl,-u_AppColdStart -Wl,-u_AppWarmStart $(LDFLAGS) -TAppBuildNone.ld -o $@ $< -Wl,--start-group $(addprefix -l,$(LDLIBS)) -Wl,--end-group
	$(SIZE) $@

$(TARGET)_$(JENNIC_CHIP).bin: $(TARGET)_$(JENNIC_CHIP).elf
	$(OBJCOPY) -j .version -j .bir -j .flashheader -j .vsr_table -j .vsr_handlers -j .rodata -j .text -j .data -j .bss -j .heap -j .stack -S -O binary $< $@

clean:
	rm -rf *.o dump restore *.elf *.bin eeprom_image.h
