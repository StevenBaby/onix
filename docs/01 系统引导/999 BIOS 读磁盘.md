# BIOS 读磁盘

## 参数

- AH：0x42 LBA 模式读磁盘
- DL：驱动器号 0x80：第一个硬盘
- DS:SI：DAP(Disk Address Packet)

Disk Address Packet 结构，四字节对齐，细节参见 `boot.asm` `dap` 的定义

| 偏移 | 大小 | 描述                      |
| ---- | ---- | ------------------------- |
| 0    | 1    | DAP 大小 0x10             |
| 1    | 1    | 保留，应该为 0            |
| 2    | 2    | 扇区数量 (最大可能为127 ) |
| 4    | 4    | 缓存地址 segment:offset   |
| 8    | 8    | 起始扇区 LBA              |

## 返回值

- CF：状态位
- AH：操作状态

> 如果 `AH` 不为 0，表示出错

## 其他

USB 启动未经测试，谨慎使用！！！

## 参考

- https://wiki.osdev.org/Disk_access_using_the_BIOS_(INT_13h)#LBA_in_Extended_Mode
- https://en.wikipedia.org/wiki/INT_13H
