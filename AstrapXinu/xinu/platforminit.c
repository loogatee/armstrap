/**
 * @file platforminit.c
 * @provides platforminit.
 */

#include <xinu/kernel.h>
#include <xinu/platform.h>
#include <xinu/stddef.h>
#include <xinu/string.h>


#define MMIO32(addr)  (*(volatile  ulong *)(addr))
#define MMIO16(addr)  (*(volatile ushort *)(addr))

#define U_ID            0xe000ed00
#define DEVICE_ID_BASE  0x1fff7a10
#define FLASHSIZE_BASE  0x1fff7a22


struct platform platform;        /* Platform specific configuration */
extern uint   _estack;







/**
 * Determines and stores all platform specific information.
 * @return OK if everything is determined successfully
 */
int xinu_platforminit( void )
{
	uint  tmpreg;


    strncpy(platform.name,   "Armstrap407VE", PLT_STRMAX);
    strncpy(platform.family, "ARM",           PLT_STRMAX);

    tmpreg = (uint)&_estack - 0x100;
    platform.maxaddr = (void *)tmpreg;
    platform.clkfreq = 168000000;


    tmpreg = MMIO32(U_ID);    // Cortex-M4 Technical Reference Manual, CPUID Base Register

    platform.cpuid_Implementer = (uchar )((tmpreg >> 24) &  0xFF);      // 0x41  = ARM
    platform.cpuid_Variant     = (uchar )((tmpreg >> 20) &  0x0F);      // 0x00  = Revision 0
    platform.cpuid_Constant    = (uchar )((tmpreg >> 16) &  0x0F);      // 0x0F  = not explained
    platform.cpuid_PartNo      = (ushort)((tmpreg >> 4)  & 0xFFF);      // 0xC24 = Cortex-M4
    platform.cpuid_Revision    = (uchar )((tmpreg)       &  0x0F);      // 0x01  = Patch Release 1


    platform.devid0 = MMIO32(DEVICE_ID_BASE+0);     // 96 bits starting here
    platform.devid1 = MMIO32(DEVICE_ID_BASE+4);
    platform.devid2 = MMIO32(DEVICE_ID_BASE+8);

    platform.flashsize = MMIO16(FLASHSIZE_BASE);

    return OK;
}



