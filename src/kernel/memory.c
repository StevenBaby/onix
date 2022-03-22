#include <onix/memory.h>
#include <onix/types.h>
#include <onix/debug.h>
#include <onix/assert.h>
#include <onix/stdlib.h>
#include <onix/string.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define ZONE_VALID 1    // ards 可用内存区域
#define ZONE_RESERVED 2 // ards 不可用区域

#define IDX(addr) ((u32)addr >> 12) // 获取 addr 的页索引
#define PAGE(idx) ((u32)idx << 12)  // 获取页索引 idx 对应的页开始的位置
#define ASSERT_PAGE(addr) assert((addr & 0xfff) == 0)

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
    u32 count;
    ards_t *ptr;

    // 如果是 onix loader 进入的内核
    if (magic == ONIX_MAGIC)
    {
        count = *(u32 *)addr;
        ptr = (ards_t *)(addr + 4);
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
            free_pages--;
            assert(free_pages >= 0);
            u32 page = ((u32)i) << 12;
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

void memory_test()
{
    u32 pages[10];
    for (size_t i = 0; i < 10; i++)
    {
        pages[i] = get_page();
    }

    for (size_t i = 0; i < 10; i++)
    {
        put_page(pages[i]);
    }
}
