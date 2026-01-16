TARGET?=uart_bootloader
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
CFLAGS=-mcpu=cortex-m4 -mthumb -O2 -ffunction-sections -fdata-sections -Wall -Wextra -DUSE_HAL_DRIVER=0
LDFLAGS=-Tbootloader.ld -Wl,--gc-sections
SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:.c=.o)

all: build

build: $(TARGET).elf
	$(OBJCOPY) -O binary $< $(TARGET).bin

$(TARGET).elf: $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.elf *.bin src/*.o

sim: CFLAGS += -DUSE_SIM -g
sim:
	@echo "Building simulator/native build (USE_SIM)"
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)_sim

flash: build
	@echo "Use your uploader (openocd/st-flash) to flash $(TARGET).bin to MCU"

.PHONY: all build clean sim flash
