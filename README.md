# Onix - 操作系统实现

![](./docs/others/images/snapshot.png)

- [本项目地址](https://github.com/StevenBaby/onix)

- [相关 bilibili 视频](https://www.bilibili.com/video/BV1gR4y1u7or/)

- [参考文献](./docs/others/参考文献.md)

## 开发中的功能

### 硬件驱动：

- [x] CPU 功能检测
- [x] FPU 支持
- [ ] ACPI：控制关机和重启 [@lihanrui2913](https://github.com/lihanrui2913)
- [x] ISA 总线
    - [x] 💾 软盘驱动
    - [x] 📣 Sound Blaster16 驱动
- [x] 💿 ATAPI 光盘驱动
- [x] PCI 总线枚举
- [x] IDE 硬盘 DMA
- [x] 网卡驱动 E1000
- [x] 网卡驱动 RTL8139

### 文件系统

- [x] 虚拟文件系统
- [x] ISO 9660 文件系统

### 网络协议栈

- [x] Ether
- [x] ARP
- [x] IP
- [x] ICMP
- [x] UDP
- [ ] TCP
- [ ] DNS
- [ ] HTTP

## Pull Request 约定

1. 确保每个修改的文件都是有意义的，不要添加与系统无关的文件；
2. 确保每个 commit 都有足够的分量，对于无关紧要的 commit 可以合并在一起；
3. Pull Request 请先提交到 dev 分支，若有新功能开发，再开新分支；

Commit Decription 前面加上 emoji ❤️ 提高阅读性：

- ✨ 视频录制：与 B 站某一视频相关
- 🐛 漏洞修复：修改了系统中的某个 Bug
- 🎈 功能开发：加入原系统中没有的新功能
- 📖 文档整理：修改 docs 中的内容
- 📔 学习笔记：记录学习过程中的一些问题或者感悟
- 🍕 其他：若有与 commit 强相关的 emoji 也可以添加，如：💾 软盘驱动

## 问题及答案

- [问题及答案](./docs/others/问题及答案%20(Question%20and%20Answer).md)
    - [gcc -m32 链接错误](./docs/others/问题及答案%20(Question%20and%20Answer).md#gcc--m32-%E9%93%BE%E6%8E%A5%E9%94%99%E8%AF%AF)
    - [无法调试汇编代码](./docs/others/问题及答案%20(Question%20and%20Answer).md#%E6%97%A0%E6%B3%95%E8%B0%83%E8%AF%95%E6%B1%87%E7%BC%96%E4%BB%A3%E7%A0%81)

## 相关软件版本参考

- bochs >= 2.7 [^bochs]
- qemu >= 6.2.0 [^qemu]
- gcc >= 11.2.0 [^gcc]
- gdb == 12.1 [^gdb]
- nasm == 2.15.05
- binutils >= 2.38
- vmware >= 16.1
- vscode == 1.74.3

## 参考

[^bochs]: <https://bochs.sourceforge.io>
[^qemu]: <https://www.qemu.org/docs/master/>
[^gcc]: <https://gcc.gnu.org/>
[^gdb]: <https://www.sourceware.org/gdb/>
[^nasm]: <https://www.nasm.us/>

