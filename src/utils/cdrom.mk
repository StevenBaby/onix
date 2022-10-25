
$(BUILD)/kernel.iso : $(BUILD)/kernel.bin $(SRC)/utils/grub.cfg

# 检测内核文件是否合法
	grub-file --is-x86-multiboot2 $<
# 创建 iso 目录
	mkdir -p $(BUILD)/iso/boot/grub
# 拷贝内核文件
	cp $< $(BUILD)/iso/boot
# 拷贝 grub 配置文件
	cp $(SRC)/utils/grub.cfg $(BUILD)/iso/boot/grub
# 生成 iso 文件
	grub-mkrescue -o $@ $(BUILD)/iso

.PHONY: bochsb
bochsb: $(BUILD)/kernel.iso
	bochs -q -f ../bochs/bochsrc.grub -unlock

QEMU_CDROM := -drive file=$(BUILD)/kernel.iso,media=cdrom # 光盘镜像

QEMU_CDROM_BOOT:= -boot d

.PHONY: qemub
qemub: $(BUILD)/kernel.iso $(IMAGES)
	$(QEMU) $(QEMU_CDROM) $(QEMU_CDROM_BOOT) \
	# $(QEMU_DEBUG)

.PHONY:cdrom
cdrom: $(BUILD)/kernel.iso $(IMAGES)
	-