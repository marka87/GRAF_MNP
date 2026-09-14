# Makefile for GRAF_Bootloader (STM32F746NGH6, Sector 0: 32 KB)

TARGET = GRAF_Bootloader

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

CPU = -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

INCLUDES = \
  -I../GRAF_MNPA/Drivers/CMSIS/Device/ST/STM32F7xx/Include \
  -I../GRAF_MNPA/Drivers/CMSIS/Include \
  -I../GRAF_MNPA/Drivers/STM32F7xx_HAL_Driver/Inc

CFLAGS = $(CPU) $(INCLUDES) -DSTM32F746xx -O2 -Wall -ffunction-sections -fdata-sections -std=gnu11
LDFLAGS = $(CPU) -TSTM32F746NGHX_BOOTLOADER.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-Map=$(TARGET).map

SRCS = \
  Core/Src/main.c \
  Core/Startup/startup_stm32f746nghx.s \
  ../GRAF_MNPA/Core/Src/system_stm32f7xx.c \
  ../GRAF_MNPA/Core/Src/syscalls.c

OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.s=.o)

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(SIZE) $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map

.PHONY: all clean
