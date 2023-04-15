#ifndef ONIX_CPU_H
#define ONIX_CPU_H

#include <onix/types.h>

// Vendor strings from CPUs.
#define CPUID_VENDOR_AMD "AuthenticAMD"
#define CPUID_VENDOR_INTEL "GenuineIntel"

bool cpu_check_cpuid();

typedef struct cpu_vendor_t
{
    u32 max_value;
    char info[13];
} _packed cpu_vendor_t;

void cpu_vendor_id(cpu_vendor_t *item);

typedef struct cpu_version_t
{
    u8 stepping : 4;
    u8 model : 4;
    u8 family : 4;
    u8 type : 2;
    u8 RESERVED : 2;
    u8 emodel : 4;
    u8 efamily0 : 4;
    u8 efamily1 : 4;
    u8 RESERVED : 4;

    u8 brand_index;
    u8 clflush;
    u8 max_num;
    u8 apic_id;

    // ECX;
    u8 SSE3 : 1;       // 0
    u8 PCLMULQDQ : 1;  // 1
    u8 DTES64 : 1;     // 2
    u8 MONITOR : 1;    // 3
    u8 DS_CPL : 1;     // 4
    u8 VMX : 1;        // 5
    u8 SMX : 1;        // 6
    u8 EIST : 1;       // 7
    u8 TM2 : 1;        // 8
    u8 SSSE3 : 1;      // 9
    u8 CNXT_ID : 1;    // 10
    u8 SDBG : 1;       // 11
    u8 FMA : 1;        // 12
    u8 CMPXCHG16B : 1; // 13
    u8 xTPR : 1;       // 14
    u8 PDCM : 1;       // 15
    u8 RESERVED : 1;   // 16
    u8 PCID : 1;       // 17
    u8 DCA : 1;        // 18
    u8 SSE4_1 : 1;     // 19
    u8 SSE4_2 : 1;     // 20
    u8 x2APIC : 1;     // 21
    u8 MOVBE : 1;      // 22
    u8 POPCNT : 1;     // 23
    u8 TSCD : 1;       // 24
    u8 AESNI : 1;      // 25
    u8 XSAVE : 1;      // 26
    u8 OSXSAVE : 1;    // 27
    u8 AVX : 1;        // 28
    u8 F16C : 1;       // 29
    u8 RDRAND : 1;     // 30
    u8 RESERVED : 1;   // 31

    // EDX
    u8 FPU : 1;      // 0 x87 FPU on Chip
    u8 VME : 1;      // 1 Virtual-8086 Mode Enhancement
    u8 DE : 1;       // 2 Debugging Extensions
    u8 PSE : 1;      // 3 Page Size Extensions
    u8 TSC : 1;      // 4 Time Stamp Counter
    u8 MSR : 1;      // 5 RDMSR and WRMSR Support
    u8 PAE : 1;      // 6 Physical Address Extensions
    u8 MCE : 1;      // 7 Machine Check Exception
    u8 CX8 : 1;      // 8 CMPXCHG8B Inst.
    u8 APIC : 1;     // 9 APIC on Chip
    u8 RESERVED : 1; // 10
    u8 SEP : 1;      // 11 SYSENTER and SYSEXIT
    u8 MTRR : 1;     // 12 Memory Type Range Registers
    u8 PGE : 1;      // 13 PTE Global Bit
    u8 MCA : 1;      // 14 Machine Check Architecture
    u8 CMOV : 1;     // 15 Conditional Move/Compare Instruction
    u8 PAT : 1;      // 16 Page Attribute Table
    u8 PSE36 : 1;    // 17 Page Size Extension
    u8 PSN : 1;      // 18 Processor Serial Number
    u8 CLFSH : 1;    // 19 CLFLUSH instruction
    u8 RESERVED : 1; // 20
    u8 DS : 1;       // 21 Debug Store
    u8 ACPI : 1;     // 22 Thermal Monitor and Clock Ctrl
    u8 MMX : 1;      // 23 MMX Technology
    u8 FXSR : 1;     // 24 FXSAVE/FXRSTOR
    u8 SSE : 1;      // 25 SSE Extensions
    u8 SSE2 : 1;     // 26 SSE2 Extensions
    u8 SS : 1;       // 27 Self Snoop
    u8 HTT : 1;      // 28 Multi-threading
    u8 TM : 1;       // 29 Therm. Monitor
    u8 RESERVED : 1; // 30
    u8 PBE : 1;      // 31 Pend. Brk. EN.
} _packed cpu_version_t;

void cpu_version(cpu_version_t *ver);

#endif
