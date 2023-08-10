MUSIC:= ./utils/deer.mp3

$(BUILD)/mono.wav: $(MUSIC)
	ffmpeg -i $< -ac 1 -ar 44100 -acodec pcm_u8 -y $@

$(BUILD)/stereo.wav: $(MUSIC)
	ffmpeg -i $< -ac 2 -ar 44100 -acodec pcm_s16le -y $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
	$(BUILD)/boot/vm86.bin \
	$(BUILD)/system.bin \
	$(BUILD)/system.map \
	$(SRC)/utils/master.sfdisk \
	$(SRC)/utils/network.conf \
	$(SRC)/utils/resolv.conf \
	$(BUILTIN_APPS) \
	$(BUILD)/mono.wav \
	$(BUILD)/stereo.wav \

# 创建一个 16M 的硬盘镜像
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@

# 将 boot.bin 写入主引导扇区
	dd if=$(BUILD)/boot/boot.bin of=$@ bs=512 count=1 conv=notrunc

# 将 loader.bin 写入硬盘
	dd if=$(BUILD)/boot/loader.bin of=$@ bs=512 count=4 seek=2 conv=notrunc

# 测试 system.bin 容量，否则需要修改下面的 count
	test -n "$$(find $(BUILD)/system.bin -size -384k)"

# 将 system.bin 写入硬盘
	dd if=$(BUILD)/system.bin of=$@ bs=512 count=1000 seek=10 conv=notrunc

# 执行硬盘分区
	sfdisk $@ < $(SRC)/utils/master.sfdisk

# 挂载设备
	sudo losetup /dev/loop0 --partscan $@

# 创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 /dev/loop0p1

# 挂载文件系统
	sudo mount /dev/loop0p1 /mnt

# 切换所有者
	sudo chown ${USER} /mnt 

# 创建目录
	mkdir -p /mnt/bin
	mkdir -p /mnt/dev
	mkdir -p /mnt/etc
	mkdir -p /mnt/mnt

# 拷贝配置文件
	cp -r $(SRC)/utils/network.conf /mnt/etc
	cp -r $(SRC)/utils/resolv.conf /mnt/etc

# 拷贝音频
	mkdir -p /mnt/data
	cp $(BUILD)/mono.wav /mnt/data
	cp $(BUILD)/stereo.wav /mnt/data

# 拷贝程序
	for app in $(BUILTIN_APPS); \
	do \
		cp $$app /mnt/bin; \
	done

	echo "hello onix!!!" > /mnt/hello.txt

# 拷贝 vm8086 程序
	# 测试 vm86.bin 容量，小于一页
	test -n "$$(find $(BUILD)/boot/vm86.bin -size -4k)"
	cp $(BUILD)/boot/vm86.bin /mnt/bin

# 拷贝图像文件
	cp ./utils/images/bingbing.bmp /mnt/data
	cp ./utils/images/taklimakan.bmp /mnt/data
	cp ./utils/images/mouse.bmp /mnt/data

# 卸载文件系统
	sudo umount /mnt

# 卸载设备
	sudo losetup -d /dev/loop0

$(BUILD)/slave.img: $(SRC)/utils/slave.sfdisk

# 创建一个 32M 的硬盘镜像
	yes | bximage -q -hd=32 -func=create -sectsize=512 -imgmode=flat $@

# 执行硬盘分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk

# 挂载设备
	sudo losetup /dev/loop0 --partscan $@

# 创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 /dev/loop0p1

# 挂载文件系统
	sudo mount /dev/loop0p1 /mnt

# 切换所有者
	sudo chown ${USER} /mnt 

# 创建文件
	echo "slave root direcotry file..." > /mnt/hello.txt

# 卸载文件系统
	sudo umount /mnt

# 卸载设备
	sudo losetup -d /dev/loop0

$(BUILD)/floppya.img:

# 创建一个 1.44M 的软盘镜像
	yes | bximage -q -fd=1.44M -func=create -sectsize=512 -imgmode=flat $@

.PHONY: mount0
mount0: $(BUILD)/master.img
	sudo losetup /dev/loop0 --partscan $<
	sudo mount /dev/loop0p1 /mnt
	sudo chown ${USER} /mnt 

.PHONY: umount0
umount0: /dev/loop0
	-sudo umount /mnt
	-sudo losetup -d $<

.PHONY: mount1
mount1: $(BUILD)/slave.img
	sudo losetup /dev/loop1 --partscan $<
	sudo mount /dev/loop1p1 /mnt
	sudo chown ${USER} /mnt 

.PHONY: umount1
umount1: /dev/loop1
	-sudo umount /mnt
	-sudo losetup -d $<

IMAGES:= $(BUILD)/master.img \
	$(BUILD)/slave.img \
	$(BUILD)/floppya.img

image: $(IMAGES)
