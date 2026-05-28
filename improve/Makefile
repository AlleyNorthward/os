TOOL_PREFIX = x86_64-linux-gnu-
QEMU = qemu-system-i386

SRC = src
BUILD = build

IMAGE = ../image/disk.img
ELF = $(BUILD)/os.elf
BIN = $(BUILD)/os.bin
LDSCRIPT = $(SRC)/link.ld

CFLAGS = -g -c -O0 -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -nostdinc

.PHONY: all image run debug gdb clean

all: image

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/start.o: $(SRC)/start.S | $(BUILD)
	$(TOOL_PREFIX)gcc $(CFLAGS) $< -o $@

$(BUILD)/os.o: $(SRC)/os.c $(SRC)/os.h | $(BUILD)
	$(TOOL_PREFIX)gcc $(CFLAGS) $< -o $@

# $(ELF): $(BUILD)/start.o $(BUILD)/os.o
# 	$(TOOL_PREFIX)ld -m elf_i386 -Ttext=0x7c00 $^ -o $@
$(ELF): $(BUILD)/start.o $(BUILD)/os.o $(LDSCRIPT)
	$(TOOL_PREFIX)ld -m elf_i386 -T $(LDSCRIPT) -Map=$(BUILD)/os.map \
	$(BUILD)/start.o $(BUILD)/os.o -o $@

$(BIN): $(ELF)
	$(TOOL_PREFIX)objcopy -O binary $< $@

$(IMAGE): | $(BUILD)
	dd if=/dev/zero of=$@ bs=512 count=2880

image: $(BIN) $(IMAGE)
	dd if=$(BIN) of=$(IMAGE) conv=notrunc

run: image
	$(QEMU) -m 128M -drive file=$(IMAGE),format=raw

debug: image
	$(QEMU) \
	-daemonize \
	-m 128M \
	-s -S 
	-drive file=$(IMAGE),index=0,media=disk,format=raw

# 远程调试Ctrl + a c可以切入qemu
ndebug: image
	$(QEMU) \
	-m 128M \
	-s -S \
	-drive file=$(IMAGE),index=0,media=disk,format=raw \
	-nographic

gdb:
	gdb $(ELF) \
	-x ../study/.gdbinit \
	-ex "mode16" \
	-ex "target remote localhost:1234" \
	-ex "boot"

clean:
	rm -rf $(BUILD)
