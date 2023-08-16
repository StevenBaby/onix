
.PHONY: bochs
bochs: $(IMAGES)
	bochs -q -f ../bochs/bochsrc -unlock

.PHONY: bochsg
bochsg: $(IMAGES)
	bochs-gdb -q -f ../bochs/bochsrc.gdb -unlock

QEMU:= qemu-system-i386 # 虚拟机
QEMU+= -m 32M # 内存
QEMU+= -audiodev pa,id=snd # 音频设备
QEMU+= -machine pcspk-audiodev=snd # pcspeaker 设备
QEMU+= -device sb16,audiodev=snd # Sound Blaster 16
QEMU+= -rtc base=localtime # 设备本地时间
QEMU+= -chardev stdio,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com1 # 字符设备 1
# QEMU+= -chardev vc,mux=on,id=com2 # 字符设备 2
# QEMU+= -chardev udp,mux=on,id=com2,port=6666,ipv4=on # 字符设备 2
QEMU+= -serial chardev:com1 # 串口 1
# QEMU+= -serial chardev:com2 # 串口 2
# QEMU+= -netdev bridge,id=eth0 # 网络设备
QEMU+= -netdev tap,id=eth0,ifname=tap0,script=no,downscript=no # 网络设备
QEMU+= -device e1000,netdev=eth0,mac=5A:5A:5A:5A:5A:33 # 网卡 e1000
# QEMU+= -object filter-dump,id=f1,netdev=eth0,file=$(BUILD)/dump.pcap

QEMU_DISK := -drive file=$(BUILD)/master.img,if=ide,index=0,media=disk,format=raw # 主硬盘
QEMU_DISK += -drive file=$(BUILD)/slave.img,if=ide,index=1,media=disk,format=raw # 从硬盘
QEMU_DISK += -drive file=$(BUILD)/floppya.img,if=floppy,index=0,media=disk,format=raw # 软盘a

QEMU_DISK_BOOT:=-boot c

QEMU_DEBUG:= -s -S

.PHONY: qemu
qemu: $(IMAGES) $(BR0) $(TAPS)
	$(QEMU) $(QEMU_DISK) $(QEMU_DISK_BOOT)

.PHONY: qemug
qemug: $(IMAGES) $(BR0) $(TAPS)
	$(QEMU) $(QEMU_DISK) $(QEMU_DISK_BOOT) $(QEMU_DEBUG)

# VMWare 磁盘转换

$(BUILD)/master.vmdk: $(BUILD)/master.img
	qemu-img convert -O vmdk $< $@

.PHONY:vmdk
vmdk: $(BUILD)/master.vmdk
