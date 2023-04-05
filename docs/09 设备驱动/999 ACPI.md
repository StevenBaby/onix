# ACPI

ACPI [^acpi] 是高级配置电源接口，利用他我们可以实现关机，重启，响应关机按钮等操作。相关代码来自 [^forum]。

ACPI 表由 BIOS 创建，并存放在内存中，OS 需要一个入口去获取到所有的表。

入口由 BIOS 放在某个固定的位置(对于 Legacy BIOS 和 UEFI，OS 获取的方式不同)，这个入口被称为 RSDP，Root System Description Pointer。它是一个结构体，提取其中的 RsdtAddress 即可获取地址，其结构如下：

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

# 提取关机数据

FACP 和 SSDT 是平级的关系，而 DSDT 的指针在 FACP 里面，但是 SSDT 和 DSDT 里面数据的格式是完全一样的 (当然只是数据格式，如果数据也完全一样那么我们看一个就够了)

偏移 0~36 依然是这个 Header，我们只需要其中的 length,signature，偏移 36~? 数据我们要做的就是在 "数据 " 这一部分里查找关机的命令，经过查找，第一行是标识符，第二行才是真正的数据：

```c++
typedef struct ACPI_FADT
{
    struct ACPISDTHeader h;
    u32 FirmwareCtrl;
    u32 *Dsdt;

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
    u32 X_FirmwareControl[2];
    u32 X_Dsdt[2];

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} ACPI_FADT;
```

# 重启

重启之前。需要把缓存读空
重启比较简单，直接向键盘控制器(0x64)发送reset(0xFE)或者向acpi的reset寄存器发送reset指令即可

```c++
u8 good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
```

## 参考

[^shutdown]: <https://forum.osdev.org/viewtopic.php?t=16990>
[^reboot]: <https://wiki.osdev.org/Reboot>
[^acpi]: <https://wiki.osdev.org/Acpi>
