#include <onix/types.h>
#include <onix/syscall.h>
#include <onix/fs.h>
#include <onix/memory.h>
#include <onix/string.h>
#include <onix/assert.h>
#include <onix/debug.h>

#if 0
#include <elf.h>
#endif

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef u32 Elf32_Word;
typedef u32 Elf32_Addr;
typedef u32 Elf32_Off;
typedef u16 Elf32_Half;

// ELF 文件标记
typedef struct ELFIdent
{
    u8 ei_magic[4];    // 内容为 0x7F, E, L, F
    u8 ei_class;       // 文件种类 1-32位，2-64 位
    u8 ei_data;        // 标记大小端 1-小端，2-大端
    u8 ei_version;     // 与 e_version 一样，必须为 1
    u8 ei_pad[16 - 7]; // 占满 16 个字节
} _packed ELFIdent;

// ELF 文件头
typedef struct Elf32_Ehdr
{
    ELFIdent e_ident;       // ELF 文件标记，文件最开始的 16 个字节
    Elf32_Half e_type;      // 文件类型，见 Etype
    Elf32_Half e_machine;   // 处理器架构类型，标记运行的 CPU，见 EMachine
    Elf32_Word e_version;   // 文件版本，见 EVersion
    Elf32_Addr e_entry;     // 程序入口地址
    Elf32_Off e_phoff;      // program header offset 程序头表在文件中的偏移量
    Elf32_Off e_shoff;      // section header offset 节头表在文件中的偏移量
    Elf32_Word e_flags;     // 处理器特殊标记
    Elf32_Half e_ehsize;    // ELF header size ELF 文件头大小
    Elf32_Half e_phentsize; // program header entry size 程序头表入口大小
    Elf32_Half e_phnum;     // program header number 程序头数量
    Elf32_Half e_shentsize; // section header entry size 节头表入口大小
    Elf32_Half e_shnum;     // section header number 节头表数量
    Elf32_Half e_shstrndx;  // 节字符串表在节头表中的索引
} Elf32_Ehdr;

// ELF 文件类型
enum Etype
{
    ET_NONE = 0,        // 无类型
    ET_REL = 1,         // 可重定位文件
    ET_EXEC = 2,        // 可执行文件
    ET_DYN = 3,         // 动态链接库
    ET_CORE = 4,        // core 文件，未说明，占位
    ET_LOPROC = 0xff00, // 处理器相关低值
    ET_HIPROC = 0xffff, // 处理器相关高值
};

// ELF 机器(CPU)类型
enum EMachine
{
    EM_NONE = 0,  // No machine
    EM_M32 = 1,   // AT&T WE 32100
    EM_SPARC = 2, // SPARC
    EM_386 = 3,   // Intel 80386
    EM_68K = 4,   // Motorola 68000
    EM_88K = 5,   // Motorola 88000
    EM_860 = 7,   // Intel 80860
    EM_MIPS = 8,  // MIPS RS3000
};

// ELF 文件版本
enum EVersion
{
    EV_NONE = 0,    // 不可用版本
    EV_CURRENT = 1, // 当前版本
};

// 程序头
typedef struct Elf32_Phdr
{
    Elf32_Word p_type;   // 段类型，见 SegmentType
    Elf32_Off p_offset;  // 段在文件中的偏移量
    Elf32_Addr p_vaddr;  // 加载到内存中的虚拟地址
    Elf32_Addr p_paddr;  // 加载到内存中的物理地址
    Elf32_Word p_filesz; // 文件中占用的字节数
    Elf32_Word p_memsz;  // 内存中占用的字节数
    Elf32_Word p_flags;  // 段标记，见 SegmentFlag
    Elf32_Word p_align;  // 段对齐约束
} Elf32_Phdr;

// 段类型
enum SegmentType
{
    PT_NULL = 0,    // 未使用
    PT_LOAD = 1,    // 可加载程序段
    PT_DYNAMIC = 2, // 动态加载信息
    PT_INTERP = 3,  // 动态加载器名称
    PT_NOTE = 4,    // 一些辅助信息
    PT_SHLIB = 5,   // 保留
    PT_PHDR = 6,    // 程序头表
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7fffffff,
};

enum SegmentFlag
{
    PF_X = 0x1, // 可执行
    PF_W = 0x2, // 可写
    PF_R = 0x4, // 可读
};

typedef struct Elf32_Shdr
{
    Elf32_Word sh_name;      // 节名
    Elf32_Word sh_type;      // 节类型，见 SectionType
    Elf32_Word sh_flags;     // 节标记，见 SectionFlag
    Elf32_Addr sh_addr;      // 节地址
    Elf32_Off sh_offset;     // 节在文件中的偏移量
    Elf32_Word sh_size;      // 节大小
    Elf32_Word sh_link;      // 保存了头表索引链接，与节类型相关
    Elf32_Word sh_info;      // 额外信息，与节类型相关
    Elf32_Word sh_addralign; // 地址对齐约束
    Elf32_Word sh_entsize;   // 子项入口大小
} Elf32_Shdr;

int sys_execve(char *filename, char *argv[], char *envp[])
{
    fd_t fd = open(filename, O_RDONLY, 0);
    if (fd == EOF)
        return EOF;

    // ELF 文件头
    Elf32_Ehdr *ehdr = (Elf32_Ehdr *)alloc_kpage(1);
    lseek(fd, 0, SEEK_SET);
    read(fd, (char *)ehdr, sizeof(Elf32_Ehdr));

    LOGK("ELF ident %s\n", ehdr->e_ident.ei_magic);
    LOGK("ELF class %d\n", ehdr->e_ident.ei_class);
    LOGK("ELF data %d\n", ehdr->e_ident.ei_data);
    LOGK("ELF type %d\n", ehdr->e_type);
    LOGK("ELF machine %d\n", ehdr->e_machine);
    LOGK("ELF entry 0x%p\n", ehdr->e_entry);
    LOGK("ELF ehsize %d %d\n", ehdr->e_ehsize, sizeof(Elf32_Ehdr));
    LOGK("ELF phoff %d\n", ehdr->e_phoff);
    LOGK("ELF phnum %d\n", ehdr->e_phnum);
    LOGK("ELF phsize %d %d\n", ehdr->e_phentsize, sizeof(Elf32_Phdr));
    LOGK("ELF shoff %d\n", ehdr->e_shoff);
    LOGK("ELF shnum %d\n", ehdr->e_shnum);
    LOGK("ELF shsize %d %d\n", ehdr->e_shentsize, sizeof(Elf32_Shdr));

    // 段头
    Elf32_Phdr *phdr = (Elf32_Phdr *)alloc_kpage(1);
    lseek(fd, ehdr->e_phoff, SEEK_SET);
    read(fd, (char *)phdr, ehdr->e_phentsize * ehdr->e_phnum);
    LOGK("ELF segment size mem %d\n", ehdr->e_phentsize * ehdr->e_phnum);

    Elf32_Phdr *ptr = phdr;
    // 内容
    char *content = (char *)alloc_kpage(1);
    for (size_t i = 0; i < ehdr->e_phnum; i++)
    {
        memset(content, 0, PAGE_SIZE);
        lseek(fd, ptr->p_offset, SEEK_SET);
        read(fd, content, ptr->p_filesz);
        LOGK("segment vaddr 0x%p paddr 0x%p\n", ptr->p_vaddr, ptr->p_paddr);
        ptr++;
    }

    free_kpage((u32)ehdr, 1);
    free_kpage((u32)phdr, 1);
    free_kpage((u32)content, 1);
    return 0;
}
