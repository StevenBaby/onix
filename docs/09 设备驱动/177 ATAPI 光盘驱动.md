# ATAPI 光盘驱动

ATAPI [^atapi] 是指使用 ATA6(或更高) 标准命令集的包接口(Packet Interface) 的设备。它基本上是一种向附加到 ATA 总线的 CD-ROM、CD-RW、DVD 或磁带驱动器发出 SCSI 命令的方法。

ATAPI 使用的 ATA 命令非常少。最重要的是 PACKET 命令 (0xA0) 和 IDENTIFY PACKET DEVICE (0xA1)。

## 检测

来自 ATA/ATAPI 规范的草案副本(T13 1410D rev 3b, 365页)，对 qemu 有效。

要检测 ATA 磁盘是非包设备还是包设备，可以使用存储在 `sector count` 和 `LBA Low,Mid,High` 寄存器中的签名 (对于 ATA 控制器，寄存器 0x1F2-0x1F5)。

如果这些寄存器是 0x01, 0x01, 0x00, 0x00，那么连接的设备是非包设备，`IDENTIFY DEVICE(0xEC)` 功能应该有效。如果它们是 0x01, 0x01, 0x14, 0xEB，那么该设备是一个包设备，并且应该使用 `IDENTIFY PACKET DEVICE(0xA1)` 来检测设备。

当设备上电、复位或接收 `EXECUTE DEVICE DIAGNOSTIC` 命令时，该签名被设置/重置 (对于数据包设备，`IDENTIFY DEVICE` 也会重置签名)。

## PACKET 命令 0xA0

每个 ATAPI 命令包由一个 **命令字节**(来自SCSI命令集)，后面跟着 11 个 **数据** 字节组成。例如，读取目录是通过将以下字节发送到设备，作为一个命令。

```c++
uint8_t atapi_readtoc[]=  { 0x43 /* ATAPI_READTOC */, 0, 1, 0, 0, 0, 0, 0, 12, 0x40, 0, 0};
```

`ATA PACKET` 命令在 PIO 模式下分三个阶段工作。

1. 用 ATAPI 特定的值设置标准 ATA IO 端口寄存器。然后将 `ATA PACKET` 命令发送到设备，就像任何其他 ATA 命令一样；

   ```c++
    outb(ATA Command Register, 0xA0);
   ```

2. 准备进行 PIO 数据传输，正常的方式，到设备。当设备准备好 (BSY 清除，DRQ 设置) 时，您将 ATAPI 命令字符串 (如上所示) 作为 6 个字的 PIO 传输发送到设备；

3. 等待中断 IRQ。当它到达时，您必须读取 LBA Mid 和 LBA High IO 端口寄存器。它们告诉您 ATAPI 驱动器将发送给您或必须从您接收的数据包字节数。在循环中，传输这个字节数，然后等待下一个 IRQ。

在 DMA 模式中，只发生前两个阶段。该设备通过 PCI 驱动器控制器处理阶段 3 本身的细节。

## IDENTIFY PACKET DEVICE 命令 0xA1

此命令为普通 ATA PIO 模式命令，在初始化时使用。它是 ATA IDENTIFY 命令的精确镜像，只是它只返回关于ATAPI设备的信息。以与使用 IDENTIFY 完全相同的方式使用它。

## 光盘内容

光学介质和驱动器由 SCSI 规范的 MMC 部分管理。它们以会话和轨道为结构。可读的实体称为逻辑磁道。它们是连续的块串。有些媒体类型可以承载带有多个磁道的多个会话，而其他媒体类型只能承载带有一个磁道和一个会话。

有关评估、读取和写入光学媒体的更多信息，请参见文章光盘驱动器的可读磁盘内容部分。

## x86 方向

重要提示：主控制器上，ATA IO 端口的是 `0x1F0` 到 `0x1F7`。在大部分或所有 ATAPI 文档中，您将看到这组称为 **任务文件**(Task File) 的 IO 端口。这个术语似乎很让人困惑。

首先，你需要一个缓冲。如果它将成为 DMA 缓冲区，则需要遵循 PRD 规则(请参阅使用 DMA 的 ATA/ATAPI 文章)。如果它将是一个PIO 缓冲区，那么您需要知道缓冲区的大小。称这个大小为 `maxByteCount`。它**必须**是**无符号字**，对于 PIO 模式，0 是**非法**的。0 值对于 DMA 模式是**必须**的，无论 PRD 缓冲区有多大。

假设命令的长度为 Command1 到 Command6。设备是主从设备。通过在设备选择寄存器中设置主/从位来选择目标设备。不需要其他比特了。

```c++
outb(0x1F6, slavebit<<4);
```

如果该命令将使用 DMA，则将 Features Register 设置为 1，否则为 PIO 设置为 0。

```c++
outb(0x1F1, isDMA);
```

扇区计数寄存器和 LBA 低寄存器目前未使用。在 LBA 中寄存器和 LBA 高寄存器中发送 maxByteCount。

```c++
outb(0x1F4, (maxByteCount & 0xff));
outb(0x1F5, (maxByteCount >> 8));
```

将 `ATAPI PACKET` 命令发送到命令寄存器

```c++
outb(0x1F7, 0xA0);
```

等待 IRQ 或轮询 BSY 清除和 DRQ 设置。然后将 ATAPI 命令作为 6 个字发送到数据端口。

```c++
outw(0x1F0, Command1);
outw(0x1F0, Command2);
outw(0x1F0, Command3);
outw(0x1F0, Command4);
outw(0x1F0, Command5);
outw(0x1F0, Command6);
```

然后等待另一个IRQ。你不能轮询。

如果这是一个 DMA 命令(isDMA == 1)，那么就完成了。当 IRQ 到达时，传输就完成了。

如果它是一个 PIO 命令，当 IRQ 到达时，读取 LBA 中寄存器和LBA 高寄存器。这一点至关重要。您告诉驱动器一次要传输的最大数据量。现在驱动器必须告诉你实际的传输大小。

一旦你有了传输大小 (bytecount = LBA High << 8 | LBA Mid)，进行 PIO 传输。

```c++
wordcount = bytecount/2;
```

循环 `inw(0x1F0)` 或 `outw(0x1F0)` 字数次数。

如果传输完成，`BSY` 和 `DRQ` 将被清除。否则，等待下一个 IRQ，再读或写相同数量的单词。

注意:未来可能会将 ATAPI 命令字符串的长度增加到 8 个单词。检查ATAPI 底部的两位识别单词 0 以验证 6 或 8 个单词的命令大小。

同样，如果您在发送 `PACKET` 命令后使用轮询来检查 `BSY`、`DRQ` 和 `ERR`，那么您可能应该忽略前四个循环的 ERR 位。(`ATAPI` 称之为 `CHECK` 位，而不是 `ERR`，但它的意思是一样的。)

## 完整命令集


| SCSI 命令                                         | 命令码      |
| ------------------------------------------------- | ----------- |
| TEST UNIT READY                                   | 0x00        |
| REQUEST SENSE                                     | 0x03        |
| FORMAT UNIT                                       | 0x04        |
| INQUIRY                                           | 0x12        |
| START STOP UNIT (Eject device)                    | 0x1B        |
| PREVENT ALLOW MEDIUM REMOVAL                      | 0x1E        |
| READ FORMAT CAPACITIES                            | 0x23        |
| READ CAPACITY                                     | 0x25        |
| READ (10)                                         | 0x28        |
| WRITE (10)                                        | 0x2A        |
| SEEK (10)                                         | 0x2B        |
| WRITE AND VERIFY (10)                             | 0x2E        |
| VERIFY (10)                                       | 0x2F        |
| SYNCHRONIZE CACHE                                 | 0x35        |
| WRITE BUFFER                                      | 0x3B        |
| READ BUFFER                                       | 0x3C        |
| READ TOC/PMA/ATIP                                 | 0x43        |
| GET CONFIGURATION                                 | 0x46        |
| GET EVENT STATUS NOTIFICATION                     | 0x4A        |
| READ DISC INFORMATION                             | 0x51        |
| READ TRACK INFORMATION                            | 0x52        |
| RESERVE TRACK                                     | 0x53        |
| SEND OPC INFORMATION                              | 0x54        |
| MODE SELECT (10)                                  | 0x55        |
| REPAIR TRACK                                      | 0x58        |
| MODE SENSE (10)                                   | 0x5A        |
| CLOSE TRACK SESSION                               | 0x5B        |
| READ BUFFER CAPACITY                              | 0x5C        |
| SEND CUE SHEET                                    | 0x5D        |
| REPORT LUNS                                       | 0xA0        |
| BLANK                                             | 0xA1        |
| SECURITY PROTOCOL IN                              | 0xA2        |
| SEND KEY                                          | 0xA3        |
| REPORT KEY                                        | 0xA4        |
| LOAD/UNLOAD MEDIUM                                | 0xA6        |
| SET READ AHEAD                                    | 0xA7        |
| READ (12)                                         | 0xA8        |
| WRITE (12)                                        | 0xAA        |
| READ MEDIA SERIAL NUMBER / SERVICE ACTION IN (12) | 0xAB / 0x01 |
| GET PERFORMANCE                                   | 0xAC        |
| READ DISC STRUCTURE                               | 0xAD        |
| SECURITY PROTOCOL OUT                             | 0xB5        |
| SET STREAMING                                     | 0xB6        |
| READ CD MSF                                       | 0xB9        |
| SET CD SPEED                                      | 0xBB        |
| MECHANISM STATUS                                  | 0xBD        |
| READ CD                                           | 0xBE        |
| SEND DISC STRUCTURE                               | 0xBF        |

## 检测介质容量

介质是插入 ATAPI 驱动器中的任何介质，如 CD 或 DVD。通过使用 `SCSI Read Capacity` 命令，您可以读取介质的最后一个 LBA，然后使用以下关系计算介质的容量:

容量=(最后LBA + 1) * 块大小;

处理命令后返回最后的 LBA 和 Block Size。几乎所有的 CD 和 DVD 都使用大小为 2KB 的区块。

处理此命令的算法如下:


- 选择驱动器[主/从]；
- 等待 400ns 选择完成；
- 将 FEATURES 寄存器设置为 0 [PIO模式]；
- 将 LBA1 和 LBA2 寄存器设置为 0x0008[将返回的字节数]；
- 发送数据包命令，然后轮询；
- 发送 ATAPI 报文，然后轮询；ATAPI 报文长度必须为6个字(12字节)；
- 如果没有错误，则从 DATA 寄存器读取 4 个单词[8字节]；

注意：最后两个保留字段是特定于 ATAPI 的，它们不是 SCSI 命令包版本的一部分；

CDB 中的特殊控制字段含义如下:

- RelAdr - LBA(Logical Block Address)值是相对的(仅用于链接命令)。
- PMI 部分中型指标:
   - 0 - 最后一个 LBA 的返回值
   - 1 - 最后一个 LBA 的返回值，在此之后将遇到数据传输中的重大延迟(例如，当前轨道或柱面)

如果轮询后出现错误，则可能没有插入介质，因此该命令也可以用于检测是否有介质，如果有则读取介质容量。

## 参考

[^atapi]: <https://wiki.osdev.org/ATAPI>