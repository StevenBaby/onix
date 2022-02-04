# bootloader 补充说明

- boot.asm
- loader.asm

bootloader 的主要功能就是从 BIOS 加载内核，并且提供内核需要的参数。

## bochs-x

<https://github.com/StevenBaby/computer/blob/main/x86_assembly/docs/Ubuntu%20%E9%85%8D%E7%BD%AE%20bochs%20%E7%9A%84%E6%96%B9%E6%B3%95.md>

    sudo apt install bochs-x

----

    bximage -hd -mode="flat" -size=60 -q master.img

## 0xaa55

魔数，就是用来检测错误；

- A 1010
- 5 0101
- 1010101001010101

## 0x7c00

- IBM PC 5150
- DOS 1.0

32K = 0x8000

栈：一般会放在内存的最后，因为栈时向下增长的；

32k - 1k = 0x7c00

## grub

- multiboot / 多系统启动

## 参考文献

- <https://wiki.osdev.org/GRUB>
- <http://nongnu.askapache.com/grub/phcoder/multiboot.pdf>
