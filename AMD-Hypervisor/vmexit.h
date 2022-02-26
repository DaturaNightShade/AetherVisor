#pragma once
#include "itlb_hook.h"
#include "npt_hook.h"
#include "logging.h"
#include "prepare_vm.h"

enum VMMCALL_ID : uintptr_t
{
    set_mpk_hook = 0x22FFAA1166,
    set_tlb_hook = 0xAAFF226611,
    disable_hv = 0xFFAA221166,
};

enum VMEXIT
{
    CPUID = 0x72,
    MSR = 0x7C,
    VMRUN = 0x80,
    VMMCALL = 0x81,
    NPF = 0x400,
    PF = 0x4E,
    BP = 0x43,
    INVALID = -1,
    GP = 0x4D,
    DB = 0x41,
};

void InjectException(CoreVmcbData* core_data, int vector, int error_code = 0);