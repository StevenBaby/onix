BUILD:=../build
SRC:=.

ONIX_VERSION:=1.0.0
MULTIBOOT2:=0x18000
ENTRYPOINT:=$(shell python -c "print(f'0x{$(MULTIBOOT2) + 0x8000:x}')")

CFLAGS:= -m32 					# 32 位的程序
CFLAGS+= -march=pentium			# pentium 处理器
CFLAGS+= -fno-builtin			# 不需要 gcc 内置函数
CFLAGS+= -nostdinc				# 不需要标准头文件
CFLAGS+= -fno-pic				# 不需要位置无关的代码  position independent code
CFLAGS+= -fno-pie				# 不需要位置无关的可执行程序 position independent executable
CFLAGS+= -nostdlib				# 不需要标准库
CFLAGS+= -fno-stack-protector	# 不需要栈保护
CFLAGS+= -Werror
CFLAGS+= -DONIX					# 定义 ONIX
CFLAGS+= -DONIX_DEBUG			# 定义 ONIX_DEBUG
CFLAGS+= -DONIX_VERSION='"$(ONIX_VERSION)"' # 定义 ONIX_VERSION

CFLAGS:=$(strip ${CFLAGS})

DEBUG:= -g
INCLUDE:=-I$(SRC)/include

$(BUILD)/boot/%.bin: $(SRC)/boot/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@

$(BUILD)/boot/loader.bin: $(SRC)/boot/loader.asm $(BUILD)/system.bin
	$(shell mkdir -p $(dir $@))
	nasm -f bin $< -o $@ -DKERNEL_SIZE=$$(stat -c%s "$(BUILD)/system.bin")

$(BUILD)/%.o: $(SRC)/%.asm
	$(shell mkdir -p $(dir $@))
	nasm -f elf32 $(DEBUG) $< -o $@

$(BUILD)/%.o: $(SRC)/%.c
	$(shell mkdir -p $(dir $@))
	gcc $(CFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

$(BUILD)/lib/libc.o: $(BUILD)/lib/crt.o \
	$(BUILD)/lib/crt1.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/printf.o \
	$(BUILD)/lib/assert.o \
	$(BUILD)/lib/time.o \
	$(BUILD)/lib/restorer.o \
	$(BUILD)/lib/math.o \
	$(BUILD)/lib/strerror.o \
	$(BUILD)/net/addr.o \
	$(BUILD)/net/chksum.o \

	ld -m elf_i386 -r $^ -o $@

BUILTIN_APPS := \
	$(BUILD)/builtin/init.out \
	$(BUILD)/builtin/osh.out \
	$(BUILD)/builtin/env.out \
	$(BUILD)/builtin/echo.out \
	$(BUILD)/builtin/cat.out \
	$(BUILD)/builtin/ls.out \
	$(BUILD)/builtin/kill.out \
	$(BUILD)/builtin/dup.out \
	$(BUILD)/builtin/err.out \
	$(BUILD)/builtin/count.out \
	$(BUILD)/builtin/alarm.out \
	$(BUILD)/builtin/float.out \
	$(BUILD)/builtin/player.out \
	$(BUILD)/builtin/pkt.out \
	$(BUILD)/builtin/server.out \
	$(BUILD)/builtin/client.out \
	$(BUILD)/builtin/ifconfig.out \
	$(BUILD)/builtin/ping.out \
	$(BUILD)/builtin/httpd.out \
	$(BUILD)/builtin/uname.out \
	$(BUILD)/builtin/tcp_server.out \
	$(BUILD)/builtin/tcp_client.out \
	$(BUILD)/builtin/tcp_nagle.out \
	$(BUILD)/builtin/tcp_surge.out \

$(BUILD)/builtin/%.out: $(BUILD)/builtin/%.o \
	$(BUILD)/lib/libc.o \
	
	ld -m elf_i386 -static $^ -o $@ -Ttext 0x1001000

LDFLAGS:= -m elf_i386 \
		-static \
		-Ttext $(ENTRYPOINT)\
		--section-start=.multiboot2=$(MULTIBOOT2)
LDFLAGS:=$(strip ${LDFLAGS})

$(BUILD)/kernel.bin: \
	$(BUILD)/kernel/start.o \
	$(BUILD)/kernel/onix.o \
	$(BUILD)/kernel/main.o \
	$(BUILD)/kernel/uname.o \
	$(BUILD)/kernel/io.o \
	$(BUILD)/kernel/mio.o \
	$(BUILD)/kernel/device.o \
	$(BUILD)/kernel/console.o \
	$(BUILD)/kernel/printk.o \
	$(BUILD)/kernel/assert.o \
	$(BUILD)/kernel/debug.o \
	$(BUILD)/kernel/global.o \
	$(BUILD)/kernel/task.o \
	$(BUILD)/kernel/init.o \
	$(BUILD)/kernel/idle.o \
	$(BUILD)/kernel/mutex.o \
	$(BUILD)/kernel/gate.o \
	$(BUILD)/kernel/schedule.o \
	$(BUILD)/kernel/interrupt.o \
	$(BUILD)/kernel/handler.o \
	$(BUILD)/kernel/clock.o \
	$(BUILD)/kernel/timer.o \
	$(BUILD)/kernel/time.o \
	$(BUILD)/kernel/rtc.o \
	$(BUILD)/kernel/ramdisk.o \
	$(BUILD)/kernel/ide.o \
	$(BUILD)/kernel/serial.o \
	$(BUILD)/kernel/pci.o \
	$(BUILD)/kernel/memory.o \
	$(BUILD)/kernel/arena.o \
	$(BUILD)/kernel/keyboard.o \
	$(BUILD)/kernel/tty.o \
	$(BUILD)/kernel/isa.o \
	$(BUILD)/kernel/sb16.o \
	$(BUILD)/kernel/floppy.o \
	$(BUILD)/kernel/buffer.o \
	$(BUILD)/kernel/system.o \
	$(BUILD)/kernel/execve.o \
	$(BUILD)/kernel/signal.o \
	$(BUILD)/kernel/alarm.o \
	$(BUILD)/kernel/cpu.o \
	$(BUILD)/kernel/fpu.o \
	$(BUILD)/kernel/test.o \
	$(BUILD)/kernel/e1000.o \
	$(BUILD)/net/pbuf.o \
	$(BUILD)/net/chksum.o \
	$(BUILD)/net/netif.o \
	$(BUILD)/net/loopif.o \
	$(BUILD)/net/addr.o \
	$(BUILD)/net/eth.o \
	$(BUILD)/net/arp.o \
	$(BUILD)/net/ip.o \
	$(BUILD)/net/icmp.o \
	$(BUILD)/net/port.o \
	$(BUILD)/net/udp.o \
	$(BUILD)/net/tcp.o \
	$(BUILD)/net/tcp_pcb.o \
	$(BUILD)/net/tcp_input.o \
	$(BUILD)/net/tcp_output.o \
	$(BUILD)/net/tcp_option.o \
	$(BUILD)/net/tcp_timer.o \
	$(BUILD)/net/dhcp.o \
	$(BUILD)/net/socket.o \
	$(BUILD)/net/iovec.o \
	$(BUILD)/net/pkt.o \
	$(BUILD)/net/raw.o \
	$(BUILD)/net/resolv.o \
	$(BUILD)/fs/super.o \
	$(BUILD)/fs/inode.o \
	$(BUILD)/fs/namei.o \
	$(BUILD)/fs/file.o \
	$(BUILD)/fs/fsyscall.o \
	$(BUILD)/fs/dev.o \
	$(BUILD)/fs/minix/minix.o \
	$(BUILD)/fs/pipe/pipe.o \
	$(BUILD)/fs/iso9660/iso9660.o \
	$(BUILD)/lib/bitmap.o \
	$(BUILD)/lib/list.o \
	$(BUILD)/lib/fifo.o \
	$(BUILD)/lib/string.o \
	$(BUILD)/lib/vsprintf.o \
	$(BUILD)/lib/stdlib.o \
	$(BUILD)/lib/syscall.o \
	$(BUILD)/lib/printf.o \
	$(BUILD)/lib/math.o \
	$(BUILD)/lib/strerror.o \
	$(BUILD)/builtin/osh.o \
	$(BUILD)/lib/restorer.o \

	$(shell mkdir -p $(dir $@))
	ld ${LDFLAGS} $^ -o $@

$(BUILD)/system.bin: $(BUILD)/kernel.bin
	objcopy -O binary $< $@
	python utils/generate.py
	objcopy -O binary $< $@

$(BUILD)/system.map: $(BUILD)/kernel.bin
	nm $< | sort > $@

include utils/image.mk
include utils/cdrom.mk
include utils/net.mk
include utils/cmd.mk

.PHONY: clean
clean:
	rm -rf $(BUILD)
