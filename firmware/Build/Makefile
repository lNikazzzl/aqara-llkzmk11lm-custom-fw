###############################################################################
# Custom firmware for Aqara LLKZMK11LM (JN5169), NXP JN-SW-4170 Zigbee 3.0 SDK
#
# Build from this directory with the SDK msys make (GNU make 3.81), single job:
#   make            release build, debug log on UART0
#   make DEBUG=1    verbose trace of every module
#   make clean
#
# Derived from the JN-AN-1218 Makefile. Notes for this toolchain:
#  - make 3.81 vpath does not resolve /c/... paths, so SDK_BASE_DIR is relative
#  - do not use -j: the PDUM/ZPS generated files are not ordered for parallel jobs
###############################################################################

TARGET              = AqaraRelay
JENNIC_CHIP        ?= JN5169
JENNIC_CHIP_FAMILY ?= JN516x
JENNIC_SDK          = JN-SW-4170
JENNIC_PCB          = DEVKIT4
JENNIC_STACK        = ZCL
JENNIC_MAC          = MiniMacShim
ZBPRO_DEVICE_TYPE   = ZCR
ZNCLKCMD            = AppBuildZBPro.ld
PDM_BUILD_TYPE      = _EEPROM
STACK_SIZE          = 5000
MINIMUM_HEAP_SIZE   = 2000

SDK_BASE_DIR       ?= ../../../../../../NXP/bstudio_nxp/sdk/$(JENNIC_SDK)

# Zigbee features
APP_CLUSTER_LIGHTING_SRC = 1
APP_CLUSTERS_OTA_SRC = 1

# OTA identity of the factory firmware (lumi.relay.c2acn01 v47 OTA file)
OTA_MANUFACTURER     = 0x115F
OTA_IMAGE_TYPE       = 0x0101
OTA_HEADER_STRING    = DR1175r1v1UNENCRYPTED00000JN5169
# version the running firmware reports: kept low so Zigbee2MQTT offers the factory image
OTA_REPORTED_VERSION ?= 1
# version of our OTA file: above the factory 47, 0x0100MMmm for release MM.mm
OTA_FILE_VERSION     ?= 0x01000204
CFLAGS += -DOTA_INTERNAL_STORAGE
CFLAGS += -DCLD_OTA_MANF_ID_VALUE=$(OTA_MANUFACTURER)
CFLAGS += -DAPP_OTA_FILE_VERSION=$(OTA_FILE_VERSION)
CFLAGS += -DAPP_OTA_REPORTED_VERSION=$(OTA_REPORTED_VERSION)
BDB_SUPPORT_NWK_STEERING = 1

# Debug log on UART0 (PROG connector) at 115200
TRACE ?= 1
ifeq ($(TRACE), 1)
CFLAGS += -DDBG_ENABLE
endif
# verbose log of every module; on by default, also in released images (make DEBUG=0 to drop)
DEBUG ?= 1
ifeq ($(DEBUG), 1)
CFLAGS += -DDEBUG_APP -DDEBUG_NODE -DDEBUG_ZCL -DDEBUG_RELAY -DDEBUG_INPUTS -DDEBUG_OTA -DDEBUG_LUMI -DDEBUG_STORE
endif

# wall switch mode, see inputs.h
ifdef INPUT_ROCKER_MODE
CFLAGS += -DINPUT_ROCKER_MODE=$(INPUT_ROCKER_MODE)
endif

CFLAGS += -DPDM_USER_SUPPLIED_ID
CFLAGS += -DEMBEDDED
CFLAGS += -DUSER_VSR_HANDLER
CFLAGS += -DSINGLE_CHANNEL=0

###############################################################################
# Paths

APP_SRC_DIR  = ../Source
APP_BLD_DIR  = .
GEN_DIR      = gen
APP_ZPSCFG   = $(APP_SRC_DIR)/app.zpscfg

UTIL_SRC_DIR = $(COMPONENTS_BASE_DIR)/ZigbeeCommon/Source
BDB_SRC_DIR  = $(COMPONENTS_BASE_DIR)/BDB/Source

###############################################################################
# Sources

APPSRC  = irq_JN516x.S
APPSRC += portasm_JN516x.S
APPSRC += port_JN516x.c
APPSRC += pdum_gen.c
APPSRC += pdum_apdu.S
APPSRC += zps_gen.c
APPSRC += app_main.c
APPSRC += app_node.c
APPSRC += app_zcl.c
APPSRC += relay.c
APPSRC += inputs.c
APPSRC += led.c
APPSRC += app_device.c
APPSRC += app_ota.c
APPSRC += app_store.c
APPSRC += lumi.c
APPSRC += nvm_clean.c

INCFLAGS += -I$(APP_SRC_DIR)
INCFLAGS += -I$(GEN_DIR)
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/Common/Include
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/ZigbeeCommon/Include
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/ZCL/Include
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/ZCIF/Include
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/Xcv/Include/
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/Recal/Include/
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/MicroSpecific/Include
INCFLAGS += -I$(COMPONENTS_BASE_DIR)/HardwareAPI/Include

LDLIBS += Recal_$(JENNIC_CHIP_FAMILY)

OPTIONAL_STACK_FEATURES = $(shell $(ZPSCONFIG) -n $(TARGET) -f $(APP_ZPSCFG) -y)

###############################################################################
# SDK configuration

include $(SDK_BASE_DIR)/Chip/Common/Build/config.mk
include $(SDK_BASE_DIR)/Platform/Common/Build/config.mk
include $(SDK_BASE_DIR)/Stack/Common/Build/config.mk
include $(SDK_BASE_DIR)/Components/BDB/Build/config.mk

###############################################################################

TEMP     = $(APPSRC:.c=.o)
APPOBJS  = $(TEMP:.S=.o)
APPDEPS  = $(APPOBJS:.o=.d)
# stack libraries requested by the SDK config (chip specific variant if there is one)
APPLDLIBS := $(foreach lib,$(APPLIBS),$(if $(wildcard $(addprefix $(COMPONENTS_BASE_DIR)/Library/lib,$(addsuffix _$(JENNIC_CHIP).a,$(lib)))),$(addsuffix _$(JENNIC_CHIP),$(lib)),$(addsuffix _$(JENNIC_CHIP_FAMILY),$(lib))))
LDLIBS  := $(APPLDLIBS) $(LDLIBS) JPT_$(JENNIC_CHIP)

BIN = $(TARGET)_$(JENNIC_CHIP).bin
ELF = $(TARGET)_$(JENNIC_CHIP).elf

vpath % $(APP_SRC_DIR):$(GEN_DIR):$(ZCL_SRC_DIRS):$(ZCL_SRC):$(UTIL_SRC_DIR):$(BDB_SRC_DIR)

.PHONY: all clean
all: $(BIN)

-include $(APPDEPS)
%.d:
	rm -f $*.o
	touch $@

# generated stack configuration
$(GEN_DIR)/pdum_gen.c $(GEN_DIR)/pdum_gen.h $(GEN_DIR)/pdum_apdu.S: $(APP_ZPSCFG)
	$(info Configuring the PDUM ...)
	mkdir -p $(GEN_DIR)
	$(PDUMCONFIG) -z $(TARGET) -f $< -o $(GEN_DIR)

$(GEN_DIR)/zps_gen.c $(GEN_DIR)/zps_gen.h: $(APP_ZPSCFG) $(GEN_DIR)/pdum_gen.h
	$(info Configuring the Zigbee Protocol Stack ...)
	$(ZPSCONFIG) -n $(TARGET) -t $(JENNIC_CHIP_FAMILY) -l $(ZPS_NWK_LIB) -a $(ZPS_APL_LIB) -c $(TOOL_COMMON_BASE_DIR)/$(TOOLCHAIN_PATH) -f $< -o $(GEN_DIR)

# every object depends on the generated headers
$(APPOBJS): $(GEN_DIR)/zps_gen.h $(GEN_DIR)/pdum_gen.h

pdum_gen.o zps_gen.o pdum_apdu.o: CFLAGS += -fno-lto

# generated sources do not exist yet when make starts, so vpath cannot find them
%.o: $(GEN_DIR)/%.S
	$(info Assembling $< ...)
	$(CC) -c -o $@ $(CFLAGS) $(INCFLAGS) $< -MD -MF $*.d -MP

%.o: $(GEN_DIR)/%.c
	$(info Compiling $< ...)
	$(CC) -c -o $@ $(CFLAGS) $(INCFLAGS) $< -MD -MF $*.d -MP

%.o: %.S
	$(info Assembling $< ...)
	$(CC) -c -o $@ $(CFLAGS) $(INCFLAGS) $< -MD -MF $*.d -MP

%.o: %.c
	$(info Compiling $< ...)
	$(CC) -c -o $@ $(CFLAGS) $(INCFLAGS) $< -MD -MF $*.d -MP

$(ELF): $(APPOBJS)
	$(info Linking $@ ...)
	$(CC) -Wl,--gc-sections -Wl,-u_AppColdStart -Wl,-u_AppWarmStart $(LDFLAGS) -T$(ZNCLKCMD) -L $(SDK_BASE_DIR)/Stack/ZCL/Build/ -o $@ -Wl,--start-group $(APPOBJS) $(addprefix -l,$(LDLIBS)) -lm -Wl,--end-group -Wl,-Map,$(TARGET)_$(JENNIC_CHIP).map
	$(SIZE) $@
	@# JET writes MAC, OTA header and link key at these fixed offsets, and the
	@# factory OTA client checks the key there: anything else corrupts the image
	@$(NM) $@ | grep -q "^00080040 A _FlsMACAddress" && \
	 $(NM) $@ | grep -q "^00080050 A _FlsOtaHeader" && \
	 $(NM) $@ | grep -q "^000800a0 A _FlsLinkKey" || \
	 (echo "ERROR: serialisation sections are not at 0x40/0x50/0xA0, see app_ota.c"; rm -f $@; exit 1)

$(BIN): $(ELF)
	$(info Generating binary ...)
	$(OBJCOPY) -j .version -j .bir -j .flashheader -j .vsr_table -j .vsr_handlers -j .rodata -j .text -j .data -j .bss -j .heap -j .stack -S -O binary $< $@
	$(info Generating OTA images ...)
	sh ../../tools/ota/make_ota.sh $@ . $(TARGET)_$(JENNIC_CHIP) $(OTA_MANUFACTURER) $(OTA_IMAGE_TYPE) $(OTA_HEADER_STRING) $(OTA_REPORTED_VERSION) $(OTA_FILE_VERSION)

clean:
	rm -rf *.o *.d $(GEN_DIR) $(ELF) $(BIN) *.map *_flash.bin *.ota ota_work
