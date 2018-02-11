/**
 * @file platform.h
 * @modified by Andrew Webster
 *
 * $Id: platform.h 2086 2009-10-06 22:24:27Z brylow $
 */
/* Embedded Xinu, Copyright (C) 2009.  All rights reserved. */

#ifndef _XINUPLATFORM_H_
#define _XINUPLATFORM_H_

#include <xinu/stddef.h>
#include <xinu/platform-local.h>

#define PLT_STRMAX 18
/**
 * Various platform specific settings
 */
struct platform
{
    char     name[PLT_STRMAX];
    char     family[PLT_STRMAX];
    void    *maxaddr;
    ulong    clkfreq;
    uchar    cpuid_Implementer;
    uchar    cpuid_Variant;
    uchar    cpuid_Constant;
    ushort   cpuid_PartNo;
    uchar    cpuid_Revision;
    ulong    devid0;
    ulong    devid1;
    ulong    devid2;
    ushort   flashsize;
};

extern struct platform platform;


#endif                          /* _PLATFORM_H_ */
