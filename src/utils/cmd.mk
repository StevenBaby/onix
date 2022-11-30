
.PHONY: bochs
bochs: $(IMAGES)
	bochs -q -f ../bochs/bochsrc -unlock

.PHONY: bochsg
bochsg: $(IMAGES)
	bochs-gdb -q -f ../bochs/bochsrc.gdb -unlock

QEMU:= qemu-system-i386 # 虚拟机
QEMU+= -m 32M # 内存
QEMU+= -audiodev pa,id=hda # 音频设备
QEMU+= -machine pcspk-audiodev=hda # pcspeaker 设备
QEMU+= -rtc base=localtime # 设备本地时间
QEMU+= -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw # 主硬盘
QEMU+= -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw # 从硬盘
QEMU+= -chardev stdio,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com2 # 字符设备 2
# QEMU+= -chardev udp,mux=on,id=com2,port=6666,ipv4=on # 字符设备 2
QEMU+= -serial chardev:com1 # 串口 1
# QEMU+= -serial chardev:com2 # 串口 2

QEMU_DISK_BOOT:=-boot c

QEMU_DEBUG:= -s -S

.PHONY: qemu
qemu: $(IMAGES)
	$(QEMU) $(QEMU_DISK_BOOT)

.PHONY: qemug
qemug: $(IMAGES)
	$(QEMU) $(QEMU_DISK_BOOT) $(QEMU_DEBUG)

# VMWare 磁盘转换

$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@

.PHONY:vmdk
vmdk: $(BUILD)/master.vmdk
