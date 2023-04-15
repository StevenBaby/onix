#include <onix/types.h>
#include <onix/cpu.h>
#include <onix/printk.h>
#include <onix/debug.h>
#include <onix/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

err_t sys_test()
{
    LOGK("test syscall...\n");

    cpu_vendor_t vendor;

    cpu_vendor_id(&vendor);
    printk("CPU vendor id: %s\n", vendor.info);
    printk("CPU max value: 0x%X\n", vendor.max_value);

    cpu_version_t ver;

    cpu_version(&ver);
    printk("FPU support state: %d\n", ver.FPU);
    printk("APIC support state: %d\n", ver.APIC);
    return EOK;
}
