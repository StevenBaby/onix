
$(BUILD)/kernel.iso : \
	$(BUILD)/kernel.bin \
	$(SRC)/utils/grub.cfg \
	$(BUILTIN_APPS) \
	$(BUILD)/mono.wav \
	$(BUILD)/stereo.wav \

# 检测内核文件是否合法
	grub-file --is-x86-multiboot2 $<
# 创建 iso 目录
	mkdir -p $(BUILD)/iso/boot/grub
# 拷贝内核文件
	cp $< $(BUILD)/iso/boot
# 创建 bin 目录
	mkdir -p $(BUILD)/iso/bin
# 创建 dev,mnt 目录
	mkdir -p $(BUILD)/iso/dev
	mkdir -p $(BUILD)/iso/data
	mkdir -p $(BUILD)/iso/mnt
# 拷贝应用程序
	cp $(BUILD)/builtin/*.out $(BUILD)/iso/bin
# 拷贝数据
	cp $(BUILD)/mono.wav $(BUILD)/iso/data
	cp $(BUILD)/stereo.wav $(BUILD)/iso/data

# 拷贝 grub 配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
# 生成 iso 文件
	grub-mkrescue -o $@ $(BUILD)/iso
	cp $@ $(BUILD)/onix_$(ONIX_VERSION).iso

.PHONY: bochsb
bochsb: $(BUILD)/kernel.iso
	bochs -q -f ../bochs/bochsrc.grub -unlock

QEMU_CDROM := -drive file=$(BUILD)/kernel.iso,media=cdrom,if=ide # 光盘镜像

QEMU_CDROM_BOOT:= -boot d

.PHONY: qemu-cd
qemu-cd: $(BUILD)/kernel.iso $(IMAGES)
	$(QEMU) $(QEMU_CDROM) $(QEMU_CDROM_BOOT) \
	# $(QEMU_DEBUG)

.PHONY: qemug-cd
qemug-cd: $(BUILD)/kernel.iso $(IMAGES)
	$(QEMU) \
	$(QEMU_CDROM) \
	$(QEMU_CDROM_BOOT) \
	$(QEMU_DEBUG)

.PHONY:cdrom
cdrom: $(BUILD)/kernel.iso $(IMAGES)
	-