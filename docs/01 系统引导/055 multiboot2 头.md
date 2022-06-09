# multiboot2 头

要支持 multiboot2，内核必须添加一个 multiboot 头，而且必须再内核开始的 32768(0x8000) 字节，而且必须 64 字节对齐；

| 偏移  | 类型 | 名称                | 备注 |
| ----- | ---- | ------------------- | ---- |
| 0     | u32  | 魔数 (magic)        | 必须 |
| 4     | u32  | 架构 (architecture) | 必须 |
| 8     | u32  | 头部长度 (header_length)   | 必须 |
| 12    | u32  | 校验和 (checksum)   | 必须 |
| 16-XX |      | 标记 (tags)         | 必须 |

- `magic` = 0xE85250D6
- `architecture`:
    - 0：32 位保护模式
- `checksum`：与 `magic`, `architecture`, `header_length` 相加必须为 `0`

## 参考文献

- <https://www.gnu.org/software/grub/manual/grub/grub.html>
- <https://www.gnu.org/software/grub/manual/multiboot2/multiboot.pdf>
- <https://intermezzos.github.io/book/first-edition/multiboot-headers.html>
- <https://os.phil-opp.com/multiboot-kernel/>
- <https://bochs.sourceforge.io/doc/docbook/user/bios-tips.html>
- <https://forum.osdev.org/viewtopic.php?f=1&t=18171>
- <https://wiki.gentoo.org/wiki/QEMU/Options>
- <https://hugh712.gitbooks.io/grub/content/>

