#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/string.h>
#include <onix/bitmap.h>
#include <onix/multiboot2.h>
#include <onix/task.h>
#include <onix/syscall.h>
#include <onix/fs.h>
#include <onix/printk.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...)

#ifdef ONIX_DEBUG
#define USER_MEMORY true
#else
#define USER_MEMORY false
#endif

#define ZONE_VALID 1    // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12)            // 获取 addr 的页索引
#define DIDX(addr) (((u32)addr >> 22) & 0x3ff) // 获取 addr 的页目录索引
#define TIDX(addr) (((u32)addr >> 12) & 0x3ff) // 获取 addr 的页表索引
#define PAGE(idx) ((u32)idx << 12)             // 获取页索引 idx 对应的页开始的位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

#define PDE_MASK 0xFFC00000

// 内核页表索引
static u32 KERNEL_PAGE_TABLE[] = {
    0x2000,
    0x3000,
    0x4000,
    0x5000,
};

#define KERNEL_MAP_BITS 0x6000

bitmap_t kernel_map;

typedef struct ards_t
{
    u64 base; // 内存基地址
    u64 size; // 内存长度
    u32 type; // 类型
} _packed ards_t;

static u32 memory_base = 0; // 可用内存基地址，应该等于 1M
static u32 memory_size = 0; // 可用内存大小
static u32 total_pages = 0; // 所有内存页数
static u32 free_pages = 0;  // 空闲内存页数

#define used_pages (total_pages - free_pages) // 已用页数

void memory_init(u32 magic, u32 addr)
{
    u32 count = 0;

    // 如果是 onix loader 进入的内核
    if (magic == ONIX_MAGIC)
    {
        count = *(u32 *)addr;
        ards_t *ptr = (ards_t *)(addr + 4);

        for (size_t i = 0; i < count; i++, ptr++)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)ptr->base, (u32)ptr->size, (u32)ptr->type);
            if (ptr->type == ZONE_VALID && ptr->size > memory_size)
            {
                memory_base = (u32)ptr->base;
                memory_size = (u32)ptr->size;
            }
        }
    }
    else if (magic == MULTIBOOT2_MAGIC)
    {
        u32 size = *(unsigned int *)addr;
        multi_tag_t *tag = (multi_tag_t *)(addr + 8);

        LOGK("Announced mbi size 0x%x\n", size);
        while (tag->type != MULTIBOOT_TAG_TYPE_END)
        {
            if (tag->type == MULTIBOOT_TAG_TYPE_MMAP)
                break;
            // 下一个 tag 对齐到了 8 字节
            tag = (multi_tag_t *)((u32)tag + ((tag->size + 7) & ~7));
        }

        multi_tag_mmap_t *mtag = (multi_tag_mmap_t *)tag;
        multi_mmap_entry_t *entry = mtag->entries;
        while ((u32)entry < (u32)tag + tag->size)
        {
            LOGK("Memory base 0x%p size 0x%p type %d\n",
                 (u32)entry->addr, (u32)entry->len, (u32)entry->type);
            count++;
            if (entry->type == ZONE_VALID && entry->len > memory_size)
            {
                memory_base = (u32)entry->addr;
                memory_size = (u32)entry->len;
            }
            entry = (multi_mmap_entry_t *)((u32)entry + mtag->entry_size);
        }
    }
    else
    {
        panic("Memory init magic unknown 0x%p\n", magic);
    }

    LOGK("ARDS count %d\n", count);
    LOGK("Memory base 0x%p\n", (u32)memory_base);
    LOGK("Memory size 0x%p\n", (u32)memory_size);

    assert(memory_base == MEMORY_BASE); // 内存开始的位置为 1M
    assert((memory_size & 0xfff) == 0); // 要求按页对齐

    total_pages = IDX(memory_size) + IDX(MEMORY_BASE);
    free_pages = IDX(memory_size);

    LOGK("Total pages %d\n", total_pages);
    LOGK("Free pages %d\n", free_pages);

    if (memory_size < KERNEL_MEMORY_SIZE)
    {
        panic("System memory is %dM too small, at least %dM needed\n",
              memory_size / MEMORY_BASE, KERNEL_MEMORY_SIZE / MEMORY_BASE);
    }
}

static u32 start_page = 0;   // 可分配物理内存起始位置
static u8 *memory_map;       // 物理内存数组
static u32 memory_map_pages; // 物理内存数组占用的页数

void memory_map_init()
{
    // 初始化物理内存数组
    memory_map = (u8 *)memory_base;

    // 计算物理内存数组占用的页数
    memory_map_pages = div_round_up(total_pages, PAGE_SIZE);
    LOGK("Memory map page count %d\n", memory_map_pages);

    free_pages -= memory_map_pages;

    // 清空物理内存数组
    memset((void *)memory_map, 0, memory_map_pages * PAGE_SIZE);

    // 前 1M 的内存位置 以及 物理内存数组已占用的页，已被占用
    start_page = IDX(MEMORY_BASE) + memory_map_pages;
    for (size_t i = 0; i < start_page; i++)
    {
        memory_map[i] = 1;
    }

    LOGK("Total pages %d free pages %d\n", total_pages, free_pages);

    // 初始化内核虚拟内存位图，需要 8 位对齐
    u32 length = (IDX(KERNEL_RAMDISK_MEM) - IDX(MEMORY_BASE)) / 8;
    bitmap_init(&kernel_map, (u8 *)KERNEL_MAP_BITS, length, IDX(MEMORY_BASE));
    bitmap_scan(&kernel_map, memory_map_pages);
}

// 分配一页物理内存
static u32 get_page()
{
    for (size_t i = start_page; i < total_pages; i++)
    {
        // 如果物理内存没有占用
        if (!memory_map[i])
        {
            memory_map[i] = 1;
            assert(free_pages > 0);
            free_pages--;
            u32 page = PAGE(i);
            LOGK("GET page 0x%p\n", page);
            return page;
        }
    }
    panic("Out of Memory!!!");
}

// 释放一页物理内存
static void put_page(u32 addr)
{
    ASSERT_PAGE(addr);

    u32 idx = IDX(addr);

    // idx 大于 1M 并且 小于 总页面数
    assert(idx >= start_page && idx < total_pages);

    // 保证只有一个引用
    assert(memory_map[idx] >= 1);

    // 物理引用减一
    memory_map[idx]--;

    // 若为 0，则空闲页加一
    if (!memory_map[idx])
    {
        free_pages++;
    }

    assert(free_pages > 0 && free_pages < total_pages);
    LOGK("PUT page 0x%p\n", addr);
}

// 得到 cr2 寄存器
u32 get_cr2()
{
    // 直接将 mov eax, cr2，返回值在 eax 中
    asm volatile("movl %cr2, %eax\n");
}

// 得到 cr3 寄存器
u32 get_cr3()
{
    // 直接将 mov eax, cr3，返回值在 eax 中
    asm volatile("movl %cr3, %eax\n");
}

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde)
{
    ASSERT_PAGE(pde);
    asm volatile("movl %%eax, %%cr3\n" ::"a"(pde));
}

// 将 cr0 寄存器最高位 PG 置为 1，启用分页
static _inline void enable_page()
{
    // 0b1000_0000_0000_0000_0000_0000_0000_0000
    // 0x80000000
    asm volatile(
        "movl %cr0, %eax\n"
        "orl $0x80000000, %eax\n"
        "movl %eax, %cr0\n");
}

// 初始化页表项
static void entry_init(page_entry_t *entry, u32 index)
{
    *(u32 *)entry = 0;
    entry->present = 1;
    entry->write = 1;
    entry->user = 1;
    entry->index = index;
}

// 初始化内存映射
void mapping_init()
{
    page_entry_t *pde = (page_entry_t *)KERNEL_PAGE_DIR;
    memset(pde, 0, PAGE_SIZE);

    idx_t index = 0;

    for (idx_t didx = 0; didx < (sizeof(KERNEL_PAGE_TABLE) / 4); didx++)
    {
        page_entry_t *pte = (page_entry_t *)KERNEL_PAGE_TABLE[didx];
        memset(pte, 0, PAGE_SIZE);

        page_entry_t *dentry = &pde[didx];
        entry_init(dentry, IDX((u32)pte));
        dentry->user = USER_MEMORY; // 只能被内核访问

        for (idx_t tidx = 0; tidx < 1024; tidx++, index++)
        {
            // 第 0 页不映射，为造成空指针访问，缺页异常，便于排错
            if (index == 0)
                continue;

            page_entry_t *tentry = &pte[tidx];
            entry_init(tentry, index);
            tentry->user = USER_MEMORY; // 只能被内核访问
            memory_map[index] = 1;      // 设置物理内存数组，该页被占用
        }
    }

    // 将最后一个页表指向页目录自己，方便修改
    page_entry_t *entry = &pde[1023];
    entry_init(entry, IDX(KERNEL_PAGE_DIR));

    // 设置 cr3 寄存器
    set_cr3((u32)pde);

    // 分页有效
    enable_page();
}

// 获取页目录
static page_entry_t *get_pde()
{
    return (page_entry_t *)(0xfffff000);
}

// 获取虚拟地址 vaddr 对应的页表
static page_entry_t *get_pte(u32 vaddr, bool create)
{
    page_entry_t *pde = get_pde();
    u32 idx = DIDX(vaddr);
    page_entry_t *entry = &pde[idx];

    assert(create || (!create && entry->present));

    page_entry_t *table = (page_entry_t *)(PDE_MASK | (idx << 12));

    if (!entry->present)
    {
        LOGK("Get and create page table entry for 0x%p\n", vaddr);
        u32 page = get_page();
        entry_init(entry, IDX(page));
        memset(table, 0, PAGE_SIZE);
    }

    return table;
}

page_entry_t *get_entry(u32 vaddr, bool create)
{
    page_entry_t *pte = get_pte(vaddr, create);
    return &pte[TIDX(vaddr)];
}

// 获取虚拟地址 vaddr 对应的物理地址
u32 get_paddr(u32 vaddr)
{
    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present)
        return 0;

    entry = get_entry(vaddr, false);
    if (!entry->present)
        return 0;

    return PAGE(entry->index) | (vaddr & 0xfff);
}

// 刷新虚拟地址 vaddr 的 块表 TLB
void flush_tlb(u32 vaddr)
{
    asm volatile("invlpg (%0)" ::"r"(vaddr)
                 : "memory");
}

// 从位图中扫描 count 个连续的页
static u32 scan_page(bitmap_t *map, u32 count)
{
    assert(count > 0);
    int32 index = bitmap_scan(map, count);

    if (index == EOF)
    {
        panic("Scan page fail!!!");
    }

    u32 addr = PAGE(index);
    LOGK("Scan page 0x%p count %d\n", addr, count);
    return addr;
}

// 与 scan_page 相对，重置相应的页
static void reset_page(bitmap_t *map, u32 addr, u32 count)
{
    ASSERT_PAGE(addr);
    assert(count > 0);
    u32 index = IDX(addr);

    for (size_t i = 0; i < count; i++)
    {
        assert(bitmap_test(map, index + i));
        bitmap_set(map, index + i, 0);
    }
}

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count)
{
    assert(count > 0);
    u32 vaddr = scan_page(&kernel_map, count);
    LOGK("ALLOC kernel pages 0x%p count %d\n", vaddr, count);
    return vaddr;
}

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count)
{
    ASSERT_PAGE(vaddr);
    assert(count > 0);
    reset_page(&kernel_map, vaddr, count);
    LOGK("FREE  kernel pages 0x%p count %d\n", vaddr, count);
}

// 拷贝一页，返回拷贝后的物理地址
static u32 copy_page(void *page)
{
    u32 paddr = get_page();
    u32 vaddr = 0;

    page_entry_t *entry = get_pte(vaddr, false);
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    memcpy((void *)vaddr, (void *)page, PAGE_SIZE);

    entry->present = false;
    flush_tlb(vaddr);

    return paddr;
}

// 页表写时拷贝
// vaddr 表示虚拟地址
// level 表示层级，页目录，页表，页框
void copy_on_write(u32 vaddr, int level)
{
    // 递归返回
    if (level == 0)
        return;

    // 获得当前虚拟地址对应的入口
    page_entry_t *entry = get_entry(vaddr, false);
    // 对该入口进行写时拷贝，于是页目录和页表拷贝完毕
    copy_on_write((u32)entry, level - 1);

    // 如果该地址已经可写，则返回
    if (entry->write)
        return;

    // 物理内存引用大于 0
    assert(memory_map[entry->index] > 0);

    // 如果引用只有 1 个，则直接可写
    if (memory_map[entry->index] == 1)
    {
        entry->write = true;
        LOGK("WRITE page for 0x%p\n", vaddr);
    }
    else
    {
        // 否则，拷贝该页
        u32 paddr = copy_page((void *)PAGE(IDX(vaddr)));

        // 物理内存引用减一
        memory_map[entry->index]--;

        // 设置新的物理页，可写
        entry->index = IDX(paddr);
        entry->write = true;
        LOGK("COPY page for 0x%p\n", vaddr);
    }

    // 刷新快表，很多错误发生在快表没有及时刷新 😔
    assert(memory_map[entry->index] > 0);
    flush_tlb(vaddr);
}

// 将 vaddr 映射物理内存
void link_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    page_entry_t *entry = get_entry(vaddr, true);

    u32 index = IDX(vaddr);

    // 如果页面已存在，则直接返回
    if (entry->present)
    {
        return;
    }

    copy_on_write((u32)entry, 2);

    u32 paddr = get_page();
    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);

    LOGK("LINK from 0x%p to 0x%p\n", vaddr, paddr);
}

// 去掉 vaddr 对应的物理内存映射
void unlink_page(u32 vaddr)
{
    ASSERT_PAGE(vaddr);

    page_entry_t *pde = get_pde();
    page_entry_t *entry = &pde[DIDX(vaddr)];
    if (!entry->present)
        return;

    entry = get_entry(vaddr, false);
    if (!entry->present)
    {
        return;
    }

    copy_on_write((u32)entry, 2);

    entry->present = false;

    u32 paddr = PAGE(entry->index);

    DEBUGK("UNLINK from 0x%p to 0x%p\n", vaddr, paddr);
    put_page(paddr);

    flush_tlb(vaddr);
}

void map_page(u32 vaddr, u32 paddr)
{
    ASSERT_PAGE(vaddr);
    ASSERT_PAGE(paddr);

    page_entry_t *entry = get_entry(vaddr, true);

    if (entry->present)
    {
        return;
    }

    if (!paddr)
    {
        paddr = get_page();
    }

    entry_init(entry, IDX(paddr));
    flush_tlb(vaddr);
}

void map_area(u32 paddr, u32 size)
{
    ASSERT_PAGE(paddr);
    u32 page_count = div_round_up(size, PAGE_SIZE);
    for (size_t i = 0; i < page_count; i++)
    {
        map_page(paddr + i * PAGE_SIZE, paddr + i * PAGE_SIZE);
    }
    LOGK("MAP memory 0x%p size 0x%X\n", paddr, size);
}

void map_zero()
{
    u32 vaddr = 0;
    page_entry_t *entry = get_pte(vaddr, false);
    entry_init(entry, IDX(0));
    flush_tlb(vaddr);
}

void unmap_zero()
{
    u32 vaddr = 0;
    page_entry_t *entry = get_pte(vaddr, false);
    entry->present = false;
    flush_tlb(vaddr);
}

// 拷贝当前页目录
page_entry_t *copy_pde()
{
    task_t *task = running_task();

    page_entry_t *pde = (page_entry_t *)task->pde;
    page_entry_t *dentry = NULL;
    page_entry_t *entry = NULL;

    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < USER_STACK_TOP >> 22; didx++)
    {
        dentry = &pde[didx];
        if (!dentry->present)
            continue;

        // 将所有页表也置为只读
        assert(memory_map[dentry->index] > 0);
        dentry->write = false;
        memory_map[dentry->index]++;
        assert(memory_map[dentry->index] < 255);

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            entry = &pte[tidx];
            if (!entry->present)
                continue;

            // 对应物理内存引用大于 0
            assert(memory_map[entry->index] > 0);

            // 若不是共享内存，则置为只读
            if (!entry->shared)
            {
                entry->write = false;
            }
            // 对应物理页引用加 1
            memory_map[entry->index]++;

            assert(memory_map[entry->index] < 255);
        }
    }

    pde = (page_entry_t *)alloc_kpage(1);
    memcpy(pde, (void *)task->pde, PAGE_SIZE);

    // 将最后一个页表指向页目录自己，方便修改
    entry = &pde[1023];
    entry_init(entry, IDX(pde));

    set_cr3(task->pde);

    return pde;
}

// 释放当前页目录
void free_pde()
{
    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    page_entry_t *pde = get_pde();

    for (size_t didx = (sizeof(KERNEL_PAGE_TABLE) / 4); didx < USER_STACK_TOP >> 22; didx++)
    {
        page_entry_t *dentry = &pde[didx];
        if (!dentry->present)
        {
            continue;
        }

        page_entry_t *pte = (page_entry_t *)(PDE_MASK | (didx << 12));

        for (size_t tidx = 0; tidx < 1024; tidx++)
        {
            page_entry_t *entry = &pte[tidx];
            if (!entry->present)
            {
                continue;
            }

            assert(memory_map[entry->index] > 0);
            put_page(PAGE(entry->index));
        }

        // 释放页表
        put_page(PAGE(dentry->index));
    }

    // 释放页目录
    free_kpage(task->pde, 1);
    LOGK("free pages %d\n", free_pages);
}

int32 sys_brk(void *addr)
{
    LOGK("task brk 0x%p\n", addr);
    u32 brk = (u32)addr;
    ASSERT_PAGE(brk);

    task_t *task = running_task();
    assert(task->uid != KERNEL_USER);

    assert(task->end <= brk && brk <= USER_MMAP_ADDR);

    u32 old_brk = task->brk;

    if (old_brk > brk)
    {
        for (u32 page = brk; page < old_brk; page += PAGE_SIZE)
        {
            unlink_page(page);
        }
    }
    else if (IDX(brk - old_brk) > free_pages)
    {
        // out of memory
        return -1;
    }

    task->brk = brk;
    return 0;
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    ASSERT_PAGE((u32)addr);

    u32 count = div_round_up(length, PAGE_SIZE);
    u32 vaddr = (u32)addr;

    task_t *task = running_task();
    if (!vaddr)
    {
        vaddr = scan_page(task->vmap, count);
    }

    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);

    for (size_t i = 0; i < count; i++)
    {
        u32 page = vaddr + PAGE_SIZE * i;
        link_page(page);
        bitmap_set(task->vmap, IDX(page), true);

        page_entry_t *entry = get_entry(page, false);
        entry->user = true;
        entry->write = false;
        entry->readonly = true;

        if (prot & PROT_WRITE)
        {
            entry->readonly = false;
            entry->write = true;
        }
        if (flags & MAP_SHARED)
        {
            entry->shared = true;
        }
        if (flags & MAP_PRIVATE)
        {
            entry->privat = true;
        }
        flush_tlb(page);
    }

    if (fd != EOF)
    {
        lseek(fd, offset, SEEK_SET);
        read(fd, (char *)vaddr, length);
    }

    return (void *)vaddr;
}

int sys_munmap(void *addr, size_t length)
{
    task_t *task = running_task();
    u32 vaddr = (u32)addr;
    assert(vaddr >= USER_MMAP_ADDR && vaddr < USER_STACK_BOTTOM);

    ASSERT_PAGE(vaddr);
    u32 count = div_round_up(length, PAGE_SIZE);

    for (size_t i = 0; i < count; i++)
    {
        u32 page = vaddr + PAGE_SIZE * i;
        unlink_page(page);
        assert(bitmap_test(task->vmap, IDX(page)));
        bitmap_set(task->vmap, IDX(page), false);
    }

    return 0;
}

typedef struct page_error_code_t
{
    u8 present : 1;
    u8 write : 1;
    u8 user : 1;
    u8 reserved0 : 1;
    u8 fetch : 1;
    u8 protection : 1;
    u8 shadow : 1;
    u16 reserved1 : 8;
    u8 sgx : 1;
    u16 reserved2;
} _packed page_error_code_t;

void page_fault(
    u32 vector,
    u32 edi, u32 esi, u32 ebp, u32 esp,
    u32 ebx, u32 edx, u32 ecx, u32 eax,
    u32 gs, u32 fs, u32 es, u32 ds,
    u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags)
{
    assert(vector == 0xe);
    u32 vaddr = get_cr2();
    LOGK("fault address 0x%p eip 0x%p\n", vaddr, eip);

    page_error_code_t *code = (page_error_code_t *)&error;
    task_t *task = running_task();

    // assert(KERNEL_MEMORY_SIZE <= vaddr && vaddr < USER_STACK_TOP);

    // 如果用户程序访问了不该访问的内存
    if (vaddr < USER_EXEC_ADDR || vaddr >= USER_STACK_TOP)
    {
        assert(task->uid);
        printk("Segmentation Fault!!!\n");
        task_exit(-1);
    }

    if (code->present)
    {
        assert(code->write);

        page_entry_t *entry = get_entry(vaddr, false);

        assert(entry->present);   // 目前写内存应该是存在的
        assert(!entry->shared);   // 共享内存页，不应该引发缺页
        assert(!entry->readonly); // 只读内存页，不应该被写

        // 页表写时拷贝
        copy_on_write(vaddr, 3);

        return;
    }

    if (!code->present && (vaddr < task->brk || vaddr >= USER_STACK_BOTTOM))
    {
        u32 page = PAGE(IDX(vaddr));
        link_page(page);
        // BMB;
        return;
    }

    LOGK("task 0x%p name %s brk 0x%p page fault\n", task, task->name, task->brk);
    panic("page fault!!!");
}

// 检测内存是否可以访问
bool memory_access(void *vaddr, int size, bool write, bool user)
{
    u32 page = PAGE(IDX(vaddr));
    u32 last = (u32)(vaddr) + size;

    page_entry_t *entry;
    for (size_t i = 0; page < last; i++, page += PAGE_SIZE)
    {
        page_entry_t *pde = get_pde();
        idx_t idx = DIDX(page);
        // 判断页表
        entry = &pde[idx];
        if (!entry->present)
            return false;

        page_entry_t *table = (page_entry_t *)(PDE_MASK | (idx << 12));
        // 页框
        entry = &table[TIDX(page)];
        if (!entry->present)
            return false;

        if (write && entry->readonly)
            return false;

        if (user && !entry->user)
            return false;
    }
    return true;
}
