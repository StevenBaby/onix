MUSIC:= ./utils/deer.mp3
LOOP:= $(shell sudo losetup --find)

$(BUILD)/mono.wav: $(MUSIC)
	ffmpeg -i $< -ac 1 -ar 44100 -acodec pcm_u8 -y $@

$(BUILD)/stereo.wav: $(MUSIC)
	ffmpeg -i $< -ac 2 -ar 44100 -acodec pcm_s16le -y $@

$(BUILD)/master.img: $(BUILD)/boot/boot.bin \
	$(BUILD)/boot/loader.bin \
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
	sudo losetup $(LOOP) --partscan $@

# 创建 minux 文件系统
	sudo mkfs.minix -1 -n 14 $(LOOP)p1

# 挂载文件系统
	sudo mount $(LOOP)p1 /mnt

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

# 卸载文件系统
	sudo umount /mnt

# 卸载设备
	sudo losetup -d $(LOOP)

$(BUILD)/slave.img: $(SRC)/utils/slave.sfdisk
	$(shell mkdir -p $(dir $@))

# 创建一个 128M 的硬盘镜像
	yes | bximage -q -hd=128 -func=create -sectsize=512 -imgmode=flat $@

# 执行硬盘分区
	sfdisk $@ < $(SRC)/utils/slave.sfdisk

# 挂载设备
	sudo losetup $(LOOP) --partscan $@

# 创建 minux 文件系统
	sudo mkfs.fat -F 32 -s 2 $(LOOP)p1

# 挂载文件系统
	sudo mount $(LOOP)p1 /mnt -o rw,uid=${USER}

# 创建文件
	sudo mkdir -p /mnt/data
	sudo mkdir -p /mnt/test_directory_for_long_name
	echo "this is fat file..." > /mnt/fat.txt
	cp $(BUILD)/stereo.wav /mnt/data

# 卸载文件系统
	sudo umount /mnt

# 卸载设备
	sudo losetup -d $(LOOP)

$(BUILD)/floppya.img:

# 创建一个 1.44M 的软盘镜像
	yes | bximage -q -fd=1.44M -func=create -sectsize=512 -imgmode=flat $@

.PHONY: mount0
mount0: $(BUILD)/master.img
	sudo losetup $(LOOP) --partscan $<
	sudo mount $(LOOP) /mnt
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
