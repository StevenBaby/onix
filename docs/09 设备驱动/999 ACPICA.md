# ACPICA

ACPI 组件体系结构 ACPICA [^acpica] [^osdev] 提供了一个独立于操作系统的高级配置和电源接口的参考实现。它可以适应任何主机操作系统。ACPICA 代码应该直接集成到主机操作系统中，作为驻留在内核中的子系统。托管 ACPICA 不需要更改核心 ACPICA 代码。然而，它确实需要一个小的特定于操作系统的接口层，它必须专门为每个主机操作系统编写。

ACPI 规范的复杂性导致了在 OS 软件中冗长而困难的实现。ACPI 组件体系结构的目的是简化操作系统供应商(Operating System Vendors OSV) 的 ACPI 实现，方法是在与操作系统无关的 ACPI 模块中提供 ACPI 实现的主要部分，这些模块可以轻松集成到任何操作系统中。

如前所述，您需要实现属于操作系统接口层(OS Interface Layer OSL) 一部分的几个函数。下面是这些函数:

## 操作系统层

至少有 45 个函数需要实现(幸运的是，其中大多数都非常简单):

### 环境和 ACPI 表

```cpp
ACPI_STATUS AcpiOsInitialize()
```

这是在 ACPICA 初始化期间调用的。它为 OSL 初始化自身提供了可能性。一般来说，它应该什么都不做；

---

```cpp
ACPI_STATUS AcpiOsTerminate()
```

这是在 ACPICA 关闭期间调用的(这不是计算机关闭，只是ACPI)。这里你可以释放在 AcpiOsInitialize 中分配的任何内存；

---

```cpp
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
```

ACPICA 把寻找 RSDP 平台兼容性的工作留给了您。但是，在 x86 上，您可以这样做:

```cpp
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    ACPI_PHYSICAL_ADDRESS Ret = 0;
    AcpiFindRootPointer(&Ret);
    return Ret;
}
```

其中 `AcpiFindRootPointer` 是 ACPICA 本身的一部分。

> 注意: ACPI 规范是高度可移植的规范，然而，它有一个静态部分通常是不可移植的：根系统描述符指针的位置。根据芯片组的不同，这个指针可以以许多不同的方式找到。在 PC 兼容的计算机(没有 EFI)上，它位于较低的内存中，通常位于 0x80000 和 0x100000 之间。但是，即使在 PC 兼容平台中，启用 EFI 的主板在通过 EFI 系统表加载时也会将 RSDP 导出到操作系统上。服务器机器上不兼容 PC 的其他主板，如实现 ACPI 的嵌入式和手持设备，将再次，并非所有人都期望将 RSDP 定位在与任何其他主板相同的位置。因此，RSDP 以芯片组特定的方式定位；从操作系统拥有 RSDP 开始，ACPI 的其余部分就完全可移植了。然而，找到 RSDP 的方式却不是这样。这就是 ACPICA 代码不会尝试提供例程以可移植方式明确查找 RSDP 的原因。如果您的系统使用 EFI，请在系统表中找到它，或者使用符合 Multiboot2 的加载程序，它会为您提供 RSDP。

---

```cpp
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
```

该功能允许主机覆盖 ACPI 命名空间中的预定义对象。当在 ACPI 名称空间中发现新对象时调用它。然而，你可以把 `NULL` 赋值给 `*NewValue`，然后返回；

---

```cpp
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
```

与 `AcpiOsPredefinedOverride` 相同，但适用于整个 ACPI 表。你可以替换它们。只是把 NULL 在 *NewTable 和返回。

### 内存管理

```cpp
void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
```

这并不容易。ACPICA 要求您在虚拟地址空间中映射物理地址。如果不使用分页，只返回 `PhysicalAddress`。你需要:

- 将长度四舍五入到一个页面的大小(例如，长度可以是 2,1024)
- 找到映射物理帧的虚拟地址范围；
- 将物理帧映射到所选择的虚拟地址；
- 返回虚拟地址加上物理地址的页偏移量。(例如：如果你被要求映射 `0x40E`，你必须返回 `0xF000040E`，而不仅仅是 `0xF0000000`)

---

```cpp
void AcpiOsUnmapMemory(void *where, ACPI_SIZE length)
```

取消使用 `AcpiOsMapMemory` 映射的页面。其中 `where` 为 `AcpiOsMapMemory` 中返回的虚拟地址，`length` 等于同一函数的长度。只需从页面目录中删除虚拟地址，并将该虚拟地址设置为可重用。注意：对于最后两个函数，您可能需要一个单独的堆；

---

```cpp
ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
```

获取 `LogicalAddress` 所指向的物理地址，并将其放入 `*PhysicalAddress` 中。如果你不使用分页只是把 LogicalAddress 赋值到 *PhysicalAddress；

--- 

```cpp
void *AcpiOsAllocate(ACPI_SIZE Size);
```

在堆中动态分配内存，错误或内存结束时返回 `NULL`。换句话说，等于 `malloc`；

---

```cpp
void AcpiOsFree(void *Memory);
```

释放上面分配的内存；

---

```cpp
BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length)
```

检查从 memory 到 (memory + Length) 的内存是否可读。这是从来没有使用过(至少我从来没有看到它使用)。然而，它应该 (在x86(_64)上) 检查内存的位置是否存在于分页结构中。

---

```cpp
BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length)
```

检查从 memory 到 (memory + Length) 的内存是否可写。这是从来没有使用过(至少我从来没有看到它使用)。然而，它应该(在x86(_64)上)检查内存的位置是否存在于分页结构中，并且它们是否设置了 Write 标志。

---

ACPICA 使用缓存来加快速度。您可以使用内核的缓存，也可以让 ACPICA 使用其内部缓存。要做到这一点，只需在您的 `platform/acwhatever.h` 文件中定义：

```cpp
#define ACPI_CACHE_T            ACPI_MEMORY_LIST
#define ACPI_USE_LOCAL_CACHE    1
```

### 多线程和调度服务

要使用 ACPICA 的所有功能，您还需要调度支持。ACPICA 指定线程，但如果您只有进程，也应该工作。如果没有也不打算使用调度器，则只能使用 ACPICA 的 Table 特性；


```cpp
ACPI_THREAD_ID AcpiOsGetThreadId ()
```

返回正在运行的线程的唯一 ID，在 Linux 实现中:

```cpp
return pthread_self();
```

---

```cpp
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
```

使用参数 Context 创建一个入口点为 Function 的新线程(或进程)。类型并不是很有用。当调度器选择这个线程时，它必须在Context中 传递给第一个参数 (x86-64 的 RDI, x86-32 的堆栈(使用 System V ABI))，类似于:

```cpp
Function(Context);
```

---

```cpp
void AcpiOsSleep(UINT64 Milliseconds)
```

让当前线程休眠 n 毫秒。

---

```cpp
void AcpiOsStall(UINT32 Microseconds)
```

暂停线程 n 微秒。注意：这不应该把线程放在睡眠队列中。线程应该继续运行，只是循环；

### 互斥与同步

是的，你还需要自旋锁、互斥锁和信号量。没人说这是件容易的事 😂

```cpp
ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle)
```

使用 `malloc` 为新的互斥锁创建空间，并将互斥锁的地址放在 *OutHandle 中，如果 `malloc` 返回 `NULL`，则返回 `AE_NO_MEMORY`，否则像大多数其他函数一样返回 `AE_OK`；

---


```cpp
void AcpiOsDeleteMutex(ACPI_MUTEX Handle);
```

这太复杂了，无法在这里解释，所以我把它留给你的想象。应该是释放上面创建的互斥锁；

---

```cpp
ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout)
```

如果没有 `Timeout` 参数，这也会很愚蠢，超时时间可以是:

- 0：如果互斥锁空闲，则获取互斥锁;如果互斥锁空闲，则不等待
- 1 $+\infty$：如果互斥锁空闲，则获取互斥锁，如果没有，则等待 `Timeout` 毫秒
- -1 (0xFFFF)：如果互斥锁是空闲的就获取它，或者等到它空闲了再返回；

----

```cpp
void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
```

需要解释吗?

---

```cpp
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle);
```

创建一个新的信号量，计数器初始化为 InitialUnits，并将其地址放在 *OutHandle 中。我不知道怎么用 MaxUnits。规格说明:此信号量需要接受的最大单位数。然而，如果你忽略这一点，你应该没事。

---

```cpp
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle);
```

需要解释吗?

---

```cpp
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout);
```

就像 AcpiOsAcquireMutex 一样，`Timeout` 的逻辑也是一样的。在 linux 实现中没有使用 `Units`。但是，它应该是调用 `sem_wait` 的次数。我不确定。应该有人去检查一下；

---

```cpp
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units);
```

等待的反义词。单位:应该调用 `sem_post` 的次数。

---

```cpp
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle);
```

创建一个新的自旋锁，并将其地址放在 `*OutHandle` 中。Spinlock 应该禁用当前 CPU 上的中断，以避免调度，并确保没有其他 CPU 将访问预留区域。

---

```cpp
void AcpiOsDeleteLock(ACPI_HANDLE Handle);
```

---

```cpp
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle);
```

锁定自旋锁并返回一个值，该值将用作 ReleaseLock 的参数。它主要用于获取锁之前的中断状态。

---

```cpp
void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
```

释放锁。``Flags`` 是 `AcquireLock` 的返回值。如果您使用它来存储中断状态，现在是使用它的时候了。

### 中断处理

```cpp
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context)
```

ACPI 有时会中断。ACPICA 会照顾他们的。InterruptLevel 是ACPI 将使用的 IRQ 号。Handler 是 ACPICA 的内部函数，用于处理中断。Context 是传递给 Handler 的参数。如果你足够幸运，你的 IRQ 管理器使用这种形式的处理程序:

```cpp
uint32_t handler(void *);
```

在本例中，只需将处理程序分配给带有该上下文的 IRQ 号。我没那么幸运，所以我做了：

```cpp
#include <Irq.h>
ACPI_OSD_HANDLER ServiceRout;

InterruptState *AcpiInt(InterruptState *ctx, void *Context)
{
    ServiceRout(Context);
    return ctx;
}

UINT32 AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context)
{
    ServiceRout = ServiceRoutine;

    RegisterIrq(InterruptNumber, AcpiInt, Contex);
    return AE_OK;
}
```

---

```cpp
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler);
```

只是 `UnregisterIrq(InterruptNumber)`。如果您有一个 IRQ 管理器，它可以为一个 IRQ 拥有多个处理程序，则提供处理程序。这将使您知道必须删除 IRQ 上的哪个处理程序。

## 在操作系统中使用 ACPICA

我没有找到任何关于将 ACPICA 源代码集成到操作系统中的好的描述，并且发布的包基本上只是一堆没有什么组织的 C 文件。这是我最后不得不做的:

1. 将以下 C 文件复制到一个 acpi 目录中：

   - dispatcher/
   - events/
   - executer/
   - hardware/
   - parser/
   - namespace/
   - utilities/
   - tables/
   - resources/

2. 从 `include/` 复制了头文件；

3. 基于 `aclinux.h` 创建了自定义的头文件，其中去掉了所有用户空间的东西，然后修改了其余部分，使其适合新操作系统的定义；

4. 编辑 include/platform/acenv.h 文件以删除 `aclinux.h` 的包含，并包含了自定义的头文件；

5. 复制了 `acenv.h`, `acgcc.h` 和自定义的头文件到操作系统的相关目录(include/platform/)；

这是除了编写 AcpiOs 接口层之外的工作，参考手册没有很好地指出您必须实际编辑头文件。不过，头文件中定义的许多宏都有文档记录。

### Visual Studio 中的经验

在 Visual Studio 中，虽然文件中没有多少组织，但移植起来相对容易。在提供的 `/generate` 目录中，有一个 vc9.0 解决方案。集成所需的唯一项目是 `AcpiSubsystem` 。将该项目与列出的所有文件一起复制(您可以保留旧的目录结构)。

#define's 可用于配置它的某些方面，也许将 #ifdef WIN32 更改为 #ifdef X86 可能是一个好主意 (Win64 -&gt;x64)。

完成后，虽然它的基础已经就位，但 actypes.h 是唯一需要修改的头文件(上面列出的)。将选项 **编译为 C 代码** 更改为默认值可能是个好主意，反正都是 C，这使您可以毫无问题地将 c++ 添加到项目中。完成后，添加 `oslp.c` 或 `oslp.cpp`，编写操作系统层，就完成了。

## 参考代码

### 关机

```cpp
AcpiEnterSleepStatePrep(5);
cli(); // 关中断
AcpiEnterSleepState(5);
panic("power off"); // 此时功能无效
```

## 参考

[^osdev]: <https://wiki.osdev.org/ACPICA>
[^acpica]: <https://acpica.org/documentation>
