USB_DEV:= /dev/sdb

.PHONY: usb
usb: $(USB_DEV) $(BUILD)/master.img

# 将 master 的 16M 写入 U 盘
	sudo dd if=$(BUILD)/master.img of=/dev/sdb bs=1024 count=16384 conv=notrunc
