#ifndef ONIX_PCI_H
#define ONIX_PCI_H

#include <onix/types.h>
#include <onix/list.h>

#define PCI_CONF_VENDOR 0X0   // 厂商
#define PCI_CONF_DEVICE 0X2   // 设备
#define PCI_CONF_COMMAND 0x4  // 命令
#define PCI_CONF_STATUS 0x6   // 状态
#define PCI_CONF_REVISION 0x8 //
#define PCI_CONF_BASE_ADDR0 0x10
#define PCI_CONF_BASE_ADDR1 0x14
#define PCI_CONF_BASE_ADDR2 0x18
#define PCI_CONF_BASE_ADDR3 0x1C
#define PCI_CONF_BASE_ADDR4 0x20
#define PCI_CONF_BASE_ADDR5 0x24
#define PCI_CONF_INTERRUPT 0x3C

#define PCI_CLASS_MASK 0xFF0000
#define PCI_SUBCLASS_MASK 0xFFFF00

#define PCI_CLASS_STORAGE_IDE 0x010100

#define PCI_BAR_TYPE_MEM 0
#define PCI_BAR_TYPE_IO 1

#define PCI_BAR_IO_MASK (~0x3)
#define PCI_BAR_MEM_MASK (~0xF)

#define PCI_COMMAND_IO 0x0001          // Enable response in I/O space
#define PCI_COMMAND_MEMORY 0x0002      // Enable response in Memory space
#define PCI_COMMAND_MASTER 0x0004      // Enable bus mastering
#define PCI_COMMAND_SPECIAL 0x0008     // Enable response to special cycles
#define PCI_COMMAND_INVALIDATE 0x0010  // Use memory write and invalidate
#define PCI_COMMAND_VGA_PALETTE 0x0020 // Enable palette snooping
#define PCI_COMMAND_PARITY 0x0040      // Enable parity checking
#define PCI_COMMAND_WAIT 0x0080        // Enable address/data stepping
#define PCI_COMMAND_SERR 0x0100        // Enable SERR/
#define PCI_COMMAND_FAST_BACK 0x0200   // Enable back-to-back writes

#define PCI_STATUS_CAP_LIST 0x010    // Support Capability List
#define PCI_STATUS_66MHZ 0x020       // Support 66 Mhz PCI 2.1 bus
#define PCI_STATUS_UDF 0x040         // Support User Definable Features [obsolete]
#define PCI_STATUS_FAST_BACK 0x080   // Accept fast-back to back
#define PCI_STATUS_PARITY 0x100      // Detected parity error
#define PCI_STATUS_DEVSEL_MASK 0x600 // DEVSEL timing
#define PCI_STATUS_DEVSEL_FAST 0x000
#define PCI_STATUS_DEVSEL_MEDIUM 0x200
#define PCI_STATUS_DEVSEL_SLOW 0x400

typedef struct pci_addr_t
{
    u8 RESERVED : 2; // 最低位
    u8 offset : 6;   // 偏移
    u8 function : 3; // 功能号
    u8 device : 5;   // 设备号
    u8 bus;          // 总线号
    u8 RESERVED : 7; // 保留
    u8 enable : 1;   // 地址有效
} _packed pci_addr_t;

typedef struct pci_bar_t
{
    u32 iobase;
    u32 size;
} pci_bar_t;

typedef struct pci_device_t
{
    list_node_t node;
    u8 bus;
    u8 dev;
    u8 func;
    u16 vendorid;
    u16 deviceid;
    u8 revision;
    u32 classcode;
} pci_device_t;

u32 pci_inl(u8 bus, u8 dev, u8 func, u8 addr);
void pci_outl(u8 bus, u8 dev, u8 func, u8 addr, u32 value);

err_t pci_find_bar(pci_device_t *device, pci_bar_t *bar, int type);
u8 pci_interrupt(pci_device_t *device);

const char *pci_classname(u32 classcode);

pci_device_t *pci_find_device(u16 vendorid, u16 deviceid);
pci_device_t *pci_find_device_by_class(u32 classcode);
void pci_enable_busmastering(pci_device_t *device);

#endif
