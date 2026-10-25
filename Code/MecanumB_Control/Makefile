# Reproducible GCC build for the checked-in STM32F429ZG + FreeRTOS source tree.
TARGET := MecanumB_Control
BUILD := build
CROSS_COMPILE ?= arm-none-eabi-
CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE := $(CROSS_COMPILE)size
RTOS := Middlewares/Third_Party/FreeRTOS/Source
HAL := Drivers/STM32F4xx_HAL_Driver
MAVLINK := Middlewares/Third_Party/MAVLink
CPU := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
INCLUDES := -IInc -IApp/include -I$(HAL)/Inc -I$(HAL)/Inc/Legacy \
 -IDrivers/CMSIS/Include -IDrivers/CMSIS/Device/ST/STM32F4xx/Include \
 -I$(RTOS)/include -I$(RTOS)/portable/GCC/ARM_CM4F -I$(MAVLINK)
CFLAGS := $(CPU) $(INCLUDES) -DUSE_HAL_DRIVER -DSTM32F429xx \
 -DVEHICLE_LINK_PROTOCOL_MAVLINK=1 \
 -std=gnu11 -Og -g3 -Wall -Wextra -Wno-address-of-packed-member \
 -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := $(CPU) -TSTM32F429ZGTX_FLASH.ld --specs=nano.specs --specs=nosys.specs \
 -Wl,--gc-sections,-Map=$(BUILD)/$(TARGET).map -Wl,--print-memory-usage
SOURCES := $(wildcard Src/*.c) $(wildcard App/src/*.c) $(wildcard $(HAL)/Src/*.c) \
 $(wildcard $(RTOS)/*.c) $(RTOS)/portable/GCC/ARM_CM4F/port.c \
 $(RTOS)/portable/MemMang/heap_4.c
ASM := Startup/startup_stm32f429zgtx.s
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o)) $(BUILD)/$(ASM:.s=.o)
.PHONY: all clean check-config
all: check-config $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin
check-config:
	python3 ../tools/check_clock_config.py
$(OBJECTS): | check-config
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(CPU) -x assembler-with-cpp -c $< -o $@
$(BUILD)/$(TARGET).elf: $(OBJECTS) STM32F429ZGTX_FLASH.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -Wl,--start-group -lc -lm -Wl,--end-group -o $@
	$(SIZE) $@
$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@
$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)
