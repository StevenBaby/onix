# ACPI

ACPI (Advanced Configuration and Power Interface) [^acpi] 是高级配置电源接口，由英特尔、微软和东芝开发的 PC 电源管理和配置标准。ACPI 允许操作系统控制每个设备提供的电量(例如，允许它将某些设备置于待机状态或关闭电源)。它还用于控制或检查发热的区域(温度传感器，风扇速度等)，电池电量，PCI IRQ 路由，CPU, NUMA [^numa] 域和许多其他东西。

## 实现 ACPI

ACPI 的信息存储在 BIOS 的内存中(当然是针对那些支持 ACPI 的系统)。ACPI 表由 BIOS 创建，并存放在内存中，OS 需要一个入口去获取到所有的表。

ACPI 有两个主要部分：

- 第一部分是操作系统在引导过程中用于配置的表(包括多少个 CPU、APIC 细节、NUMA 内存范围等)；
- 第二部分是运行时 ACPI 环境，它由 AML 代码(来自 BIOS 和设备的独立于平台的 OOP 语言) 和 ACPI SMM(System Management Mode 系统管理模式) 代码组成；

要使用 ACPI，操作系统必须查找 RSDP(Root System Description Pointer 根系统描述指针)，如果找到 RSDP 并且验证是有效的，它包含一个指向 RSDT(Root System Description Table 根系统描述表) 的指针；对于较新版本的 ACPI (ACPI 2.0 及更高版本)，还有一个额外的 XSDT(eXtended System Description Table 扩展系统描述表)；RSDT 和 XSDT 都包含指向其他表的指针，RSDT 和 XSDT 之间唯一真正的区别是 XSDT 包含 64 位指针而不是 32 位指针；

对于 ACPI 的运行时部分，要检测的主表是 FADT(Fixed ACPI Description Table 固定 ACPI 描述表)，因为它包含启用 ACPI 所需的信息；

有两种使用 ACPI 的可能性：

1. 可以编写自己的 ACPI 表读取器和 AML 解释器；
2. 可以将 ACPICA [^acpica] 集成到您的操作系统中；

## 切换到 ACPI 模式

在某些 PC 上，这已经完成了，如果：

- FADT 中的 SMI 命令字段为 0
- FADT 中的 ACPI enable 和 ACPI disable 字段均为 0
- 置位 PM1a 控制块 I/O 端口的第 0 位(值1)

否则，将 ACPI Enable 字段的值写入 SMI 命令字段所指向
的寄存器号中，如下所示:

```cpp
outb(fadt->smi_command, fadt->acpi_enable);
```

Linux 会等待 3 秒让硬件改变模式。然后轮询 PM1a 控制块，直到第 0 位(值1) 置位。设置此位时，意味着电源管理事件生成的是 SCSI 而不是 SMI，这意味着操作系统必须处理这些事件，而系统管理 BIOS 不做任何事情。SCI 是 FADT 告诉你的一个IRQ。

```cpp
while (inw(fadt->pm1a_control_block) & 1 == 0);
```

## 关机

ACPI 关机 [^shutdown] 的总结来自论坛帖子 [^forum]:

从技术上讲，ACPI 关机是一件非常简单的事情，只需要一样代码，然后，电脑就关机了：

```cpp
outw(PM1a_CNT, SLP_TYPa | SLP_EN);
```

问题在于收集这些值，特别是因为 `SLP_TYPa` 位于 DSDT 中的 \\_S5 对象中，因此是 AML 编码的。

> 警告：上面的代码不应该在生产环境中使用。作者跳过了许多事情，比如调用 `_PTS` 方法。为此需要一个真正的 AML 解释器，例如 ACPICA；

在许多硬件上，调用 `_PTS` 是必需的，如果不调用它，将导致关机失败，或者导致系统在半关机阶段冻结。这种现象主要发生在笔记本电脑上，但也发生在台式机上。

ACPI 表入口由 BIOS 放在某个固定的位置(对于 Legacy BIOS 和 UEFI，OS 获取的方式不同)，这个入口被称为 RSDP，Root System Description Pointer。它是一个结构体，提取其中的 RsdtAddress 即可获取地址，其结构如下：

```c++
struct RSDPtr
{
    u8 Signature[8];
    u8 CheckSum;
    u8 OemID[6];
    u8 Revision;
    u32 *RsdtAddress;
    ...
};
```

```c++
    return (unsigned int *)rsdp->RsdtAddress;
```

## FADT

FADT [^fadt] 是 ACPI 编程接口中使用的一种数据结构。该表包含有关电源管理的固定寄存器块的信息。

### 查找 FADT

FADT 由 RSDT 中的一个条目指向。签名是 “FACP”。即使在另一个 ACPI 有效结构中找到了指针，您也应该执行校验和来检查表是否有效。

### 结构

FADT 是一个复杂的结构，包含大量的数据，如下所示：

```c++
typedef struct FADT
{
    ACPISDTHeader h;
    u32 FirmwareCtrl;
    u32 Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    u8 Reserved;

    u8 PreferredPowerManagementProfile;
    u16 SCI_Interrupt;
    u32 SMI_CommandPort;
    u8 AcpiEnable;
    u8 AcpiDisable;
    u8 S4BIOS_REQ;
    u8 PSTATE_Control;
    u32 PM1aEventBlock;
    u32 PM1bEventBlock;
    u32 PM1aControlBlock;
    u32 PM1bControlBlock;
    u32 PM2ControlBlock;
    u32 PMTimerBlock;
    u32 GPE0Block;
    u32 GPE1Block;
    u8 PM1EventLength;
    u8 PM1ControlLength;
    u8 PM2ControlLength;
    u8 PMTimerLength;
    u8 GPE0Length;
    u8 GPE1Length;
    u8 GPE1Base;
    u8 CStateControl;
    u16 WorstC2Latency;
    u16 WorstC3Latency;
    u16 FlushSize;
    u16 FlushStride;
    u8 DutyOffset;
    u8 DutyWidth;
    u8 DayAlarm;
    u8 MonthAlarm;
    u8 Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    u16 BootArchitectureFlags;

    u8 Reserved2;
    u32 Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    u8 ResetValue;
    u8 Reserved3[3];

    // 64bit pointers - Available on ACPI 2.0+
    u64 X_FirmwareControl;
    u64 X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} FADT;
```

### 通用地址结构 (GenericAddressStructure)

在深入讨论之前，请记住 GAS 是 ACPI 用来描述寄存器位置的结构。其结构为:

```cpp
struct GenericAddressStructure
{
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
};
```

`AddressSpace` 的可能取值

| 值          | 地址空间             |
| ----------- | -------------------- |
| 0           | 系统内存             |
| 1           | 系统 I/O             |
| 2           | pci配置空间          |
| 3           | 嵌入式控制器         |
| 4           | 系统管理总线         |
| 5           | 系统CMOS             |
| 6           | pci 设备 BAR Target  |
| 7           | 智能平台管理基础设施 |
| 8           | 通用I/O              |
| 9           | 通用串行总线         |
| 0x0A        | 平台通信通道         |
| 0x0B ~ 0x7F | 预留                 |
| 0x80 ~ 0xFF | OME 定义             |

`BitWidth` 和 `BitOffset` 只有在访问位字段时才需要，我想你知道如何使用它们。

`AccessSize` 定义一次可以读/写多少字节。

`AccessSize` 的可能取值

| 值  | 访问大小         |
| --- | ---------------- |
| 0   | 未定义(遗留原因) |
| 1   | 字节访问         |
| 2   | 16 位访问        |
| 3   | 32 位访问        |
| 4   | 64 位访问        |

`Address` 是在定义的地址空间中指向数据结构的64位指针。

### 字段

- FirmwareCtrl

这是一个指向 FACS 的 32 位指针。自 ACPI 2.0 以来，表中增加了一个新的字段，类型为 GAS 的 X_FirmwareControl，它是 64 位宽的。两个字段中只使用一个，另一个字段包含 0。根据Specs，只有当 FACS 位于第 4GB 以上时才使用 X_ 字段。

- Dsdt

指向 DSDT 的 32 位指针。它也有一个 X_Dsdt 兄弟。


- PreferredPowerManagementProfile

该字段包含一个值，该值应该将您定位到电源管理配置文件。例如，如果它包含 2，则该计算机是笔记本电脑，您应该将电源管理配置为省电模式；

| 值  | 描述        |
| --- | ----------- |
| 0   | 未指明的    |
| 1   | 桌面        |
| 2   | 移动        |
| 3   | 工作站      |
| 4   | 企业服务器  |
| 5   | SOHO 服务器 |
| 6   | 家电PC      |
| 7   | 性能服务器  |


- SCI_Interrupt

系统控制中断被 ACPI 用来通知操作系统关于固定事件，例如，按下电源按钮，或用于通用事件(gpe)，这是固件特定的。FADT 结构中的这个成员表示它的 PIC 或 IOAPIC 中断引脚。要知道它是否是PIC IRQ，请检查是否通过 MADT 存在双 8259 中断控制器。否则，它就是 GSI。如果您正在使用 IOAPIC 并且 PIC 存在，请记住首先检查中断源覆盖以获得与 IRQ 源关联的 GSI；

- SMI_CommandPort

这是一个 I/O 端口。这是操作系统写入 AcpiEnable 或AcpiDisable 以获取或释放 ACPI 寄存器的所有权的地方。在不支持系统管理模式的系统上，该值为 0；

---

FACP 和 SSDT 是平级的关系，而 DSDT 的指针在 FACP 里面，但是 SSDT 和 DSDT 里面数据的格式是完全一样的 (当然只是数据格式，如果数据也完全一样那么我们看一个就够了)

偏移 0~36 依然是这个 Header，我们只需要其中的 length,signature，偏移 36~? 数据我们要做的就是在 "数据 " 这一部分里查找关机的命令，经过查找，第一行是标识符，第二行才是真正的数据；

## 模拟器特殊方法

在某些情况下(例如测试)，可能只需要一个关闭电源的方法，但不需要它在实际硬件上工作。

在 Bochs 和旧版本的 QEMU(2.0以上) 中，可以执行以下操作:

```cpp
outw(0xB004, 0x2000);
```

在较新版本的 QEMU 中，可以使用:

```cpp
outw(0x604, 0x2000);
```

在 Virtualbox 中，可以使用:

```cpp
outw(0x4004, 0x3400);
```

## 重启

重启比较简单，我所知道的有两种方式，一种是直接向0x64端口发送0xfe重置cpu，另一种是向acpi的reset寄存器发送resetvalue

位了确保成功率，我们两种方法都使用，第一种不行用第二种。

重启后，之前初始化的内存可能不是 0，需要重置。

## 参考

[^acpi]: <https://wiki.osdev.org/Acpi>
[^numa]: <https://en.wikipedia.org/wiki/Non-uniform_memory_access>
[^acpica]: <https://wiki.osdev.org/ACPICA>
[^shutdown]: <https://wiki.osdev.org/Shutdown>
[^forum]: <https://forum.osdev.org/viewtopic.php?t=16990>
[^fadt]: <https://wiki.osdev.org/FADT>
