#include "Utility.h"
#include "Structs.h"
#include "Logging.h"

namespace Utils
{
    KIRQL disableWP()
    {
        KIRQL	tempirql = KeRaiseIrqlToDpcLevel();

        ULONG64  cr0 = __readcr0();

        cr0 &= 0xfffffffffffeffff;

        __writecr0(cr0);

        _disable();

        return tempirql;

    }

    void enableWP(KIRQL		tempirql)
    {
        ULONG64	cr0 = __readcr0();

        cr0 |= 0x10000;

        _enable();

        __writecr0(cr0);

        KeLowerIrql(tempirql);
    }


    PVOID	GetVaFromPfn(ULONG64 pfn)
    {
        PHYSICAL_ADDRESS pa;
        pa.QuadPart = pfn << PAGE_SHIFT;

        return MmGetVirtualForPhysical(pa);
    }

    PFN_NUMBER	GetPfnFromVa(ULONG64 Va)
    {
        return MmGetPhysicalAddress((PVOID)Va).QuadPart >> PAGE_SHIFT;
    }

    PT_ENTRY_64* GetPte(PVOID VirtualAddress, ULONG64 Pml4BasePa, PageTableOperation Operation)
    {
        ADDRESS_TRANSLATION_HELPER helper;
        PT_ENTRY_64* finalEntry;


        helper.AsUInt64 = (UINT64)VirtualAddress;

        PHYSICAL_ADDRESS    addr;

        addr.QuadPart = Pml4BasePa;

        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(addr);

        pml4e = &pml4[helper.AsIndex.Pml4];

        if (Operation)
        {
            Operation((PT_ENTRY_64*)pml4e);
        }

        if (pml4e->Present == FALSE)
        {
            return (PT_ENTRY_64*)pml4e;
        }

        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)GetVaFromPfn(pml4e->PageFrameNumber);

        pdpte = &pdpt[helper.AsIndex.Pdpt];

        if (Operation)
        {
            Operation((PT_ENTRY_64*)pdpte);
        }

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }

        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)GetVaFromPfn(pdpte->PageFrameNumber);

        pde = &pd[helper.AsIndex.Pd];

        if (Operation)
        {
            Operation((PT_ENTRY_64*)pde);
        }

        if ((pde->Present == FALSE) || (pde->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pde;
        }


        PTE_64* pt;
        PTE_64* pte;

        
        pt = (PTE_64*)GetVaFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        if (Operation)
        {
            Operation((PT_ENTRY_64*)pte);
        }

        return  (PT_ENTRY_64*)pte;
    }
    PT_ENTRY_64* GetPte(PVOID VirtualAddress, ULONG64 Pml4BasePa, PDPTE_64** PdpteResult, PDE_64** PdeResult)
    {
        ADDRESS_TRANSLATION_HELPER helper;
        PT_ENTRY_64* finalEntry;


        helper.AsUInt64 = (UINT64)VirtualAddress;

        PHYSICAL_ADDRESS    addr;

        addr.QuadPart = Pml4BasePa;


        PML4E_64* pml4;
        PML4E_64* pml4e;

        pml4 = (PML4E_64*)MmGetVirtualForPhysical(addr);

        pml4e = &pml4[helper.AsIndex.Pml4];

        if (pml4e->Present == FALSE)
        {
            return (PT_ENTRY_64*)pml4e;
        }


        PDPTE_64* pdpt;
        PDPTE_64* pdpte;

        pdpt = (PDPTE_64*)GetVaFromPfn(pml4e->PageFrameNumber);

        *PdpteResult = pdpte = &pdpt[helper.AsIndex.Pdpt];

        if ((pdpte->Present == FALSE) || (pdpte->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pdpte;
        }


        PDE_64* pd;
        PDE_64* pde;

        pd = (PDE_64*)GetVaFromPfn(pdpte->PageFrameNumber);

        *PdeResult = pde = &pd[helper.AsIndex.Pd];

        if ((pde->Present == FALSE) || (pde->LargePage != FALSE))
        {
            return (PT_ENTRY_64*)pde;
        }


        PTE_64* pt;
        PTE_64* pte;


        pt = (PTE_64*)GetVaFromPfn(pde->PageFrameNumber);

        pte = &pt[helper.AsIndex.Pt];

        return  (PT_ENTRY_64*)pte;
    }

    void    GetJmpCode(ULONG64 jmpAddr, char* output)
    {
        char JmpIndirect[15] = "\xFF\x25\x00\x00\x00\x00\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

        memcpy(JmpIndirect + 6, &jmpAddr, sizeof(PVOID));
        memcpy((PVOID)output, JmpIndirect, 14);
    }

    PVOID	GetSystemRoutineAddress(wchar_t* RoutineName, PVOID* RoutinePhysical)
    {
        UNICODE_STRING	Routine_Name = RTL_CONSTANT_STRING(RoutineName);

        PVOID	Routine = MmGetSystemRoutineAddress(&Routine_Name);

        PVOID	RoutinePa = (PVOID)MmGetPhysicalAddress(Routine).QuadPart;

        if (RoutinePhysical)
        {
            *RoutinePhysical = RoutinePa;
        }

        return Routine;
    }


    PMDL    LockPages(PVOID VirtualAddress, LOCK_OPERATION  operation)
    {
        PMDL mdl = IoAllocateMdl(VirtualAddress, PAGE_SIZE, FALSE, FALSE, nullptr);
        
        MmProbeAndLockPages(mdl, KernelMode, operation);

        return mdl;
    }

    NTSTATUS    UnlockPages(PMDL mdl)
    {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);

        return STATUS_SUCCESS;
    }

    PVOID GetDriverBaseAddress(OUT PULONG pSize, UNICODE_STRING DriverName)
    {
        PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;

        UNICODE_STRING  DrvName;

        for (PLIST_ENTRY link = moduleList; 
            link != moduleList->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (RtlCompareUnicodeString(&DriverName, &entry->BaseDllName, false) == 0)
            {
                DbgPrint("found module! \n");
                if (pSize && MmIsAddressValid(pSize))
                {
                    *pSize = entry->SizeOfImage;
                }

                return entry->DllBase;
            }
        }

        return 0;
    }

    uintptr_t GetSectionByName(PVOID base, const char* SectionName)
    {
        PIMAGE_NT_HEADERS64 pHdr = (PIMAGE_NT_HEADERS64)RtlImageNtHeader(base);
       
        if (!pHdr)
            return 0; // STATUS_INVALID_IMAGE_FORMAT;

        PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)((uintptr_t)&pHdr->FileHeader + pHdr->FileHeader.SizeOfOptionalHeader + sizeof(IMAGE_FILE_HEADER));

        PVOID ptr = NULL;

        for (PIMAGE_SECTION_HEADER pSection = pFirstSection; pSection < pFirstSection + pHdr->FileHeader.NumberOfSections; ++pSection)
        {
            if (strcmp((char*)pSection->Name, SectionName) == 0)
            {
                return pSection->VirtualAddress + (uintptr_t)base;
            }
        }

        return 0;
    }

    HANDLE GetProcessID(const wchar_t* procName)
    {
        HANDLE  procId = 0;

        NTSTATUS status = STATUS_SUCCESS;

        PVOID buffer;


        buffer = ExAllocatePoolWithTag(NonPagedPool, 1024 * 1024, 'qpwo');

        if (!buffer) {
            DbgPrint("couldn't allocate \n");
            return 0;
        }

        PSYSTEM_PROCESS_INFORMATION pInfo = (PSYSTEM_PROCESS_INFORMATION)buffer;


        status = ZwQuerySystemInformation(SystemProcessInformation, pInfo, 1024 * 1024, NULL);
        if (!NT_SUCCESS(status)) {
            DbgPrint("ZwQuerySystemInformation Failed, status %p\n", status);
            ExFreePoolWithTag(buffer, 'qpwo');
            return 0;
        }

        UNICODE_STRING  processName;
        RtlInitUnicodeString(&processName, procName);

        if (NT_SUCCESS(status))
        {
            while (1)
            {
                if (RtlEqualUnicodeString(&pInfo->ImageName, &processName, TRUE))
                {
                    DbgPrint("found process, process Id: %i \n", pInfo->ProcessId);
                    procId = pInfo->ProcessId;

                    break;
                }
                else if (pInfo->NextEntryOffset)
                {
                    pInfo = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)pInfo + pInfo->NextEntryOffset);
                }
                else
                {
                    break;
                }
            }
        }

        ExFreePoolWithTag(buffer, 'qpwo');

        return procId;
    }
    PVOID ResolveRelativeAddress(_In_ PVOID Instruction, _In_ ULONG OffsetOffset, _In_ ULONG InstructionSize)
    {
        ULONG_PTR Instr = (ULONG_PTR)Instruction;

        LONG RipOffset = *(PLONG)(Instr + OffsetOffset);
        PVOID ResolvedAddr = (PVOID)(Instr + InstructionSize + RipOffset);

        return ResolvedAddr;
    }

    void PrintModuleFromAddress(PVOID address)
    {
        PLIST_ENTRY moduleList = (PLIST_ENTRY)PsLoadedModuleList;

        UNICODE_STRING  DrvName;

        for (PLIST_ENTRY link = moduleList;
            link != moduleList->Blink;
            link = link->Flink)
        {
            LDR_DATA_TABLE_ENTRY* entry = CONTAINING_RECORD(link, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if ((address > entry->DllBase) && ((ULONG64)address < (ULONG64)entry->DllBase + entry->SizeOfImage))
            {
                logger->Log<PVOID>("[BigBrother] address at %ws", entry->BaseDllName.Buffer, TYPES::pointer);
                logger->Log<ULONG64>("+%p \n", (ULONG64)address-(ULONG64)entry->DllBase, TYPES::pointer);
            }
        }
    }

    int ipow(int base, int power)
    {
        int start = 1;
        for (int i = 0; i < power; ++i)
        {
            start *= base;
        }

        return start;
    }

    NTSTATUS BBSearchPattern(IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, IN const VOID* base, IN ULONG_PTR size, OUT PVOID* ppFound)
    {
        ASSERT(ppFound != NULL && pattern != NULL && base != NULL);
        if (ppFound == NULL || pattern == NULL || base == NULL)
            return STATUS_INVALID_PARAMETER;

        for (ULONG_PTR i = 0; i < size - len; i++)
        {
            BOOLEAN found = TRUE;
            for (ULONG_PTR j = 0; j < len; j++)
            {
                if (pattern[j] != wildcard && pattern[j] != ((PCUCHAR)base)[i + j])
                {
                    found = FALSE;
                    break;
                }
            }

            if (found != FALSE)
            {
                *ppFound = (PUCHAR)base + i;
                return STATUS_SUCCESS;
            }
        }

        return STATUS_NOT_FOUND;
    }


    NTSTATUS BBScan(IN PCCHAR section, IN PCUCHAR pattern, IN UCHAR wildcard, IN ULONG_PTR len, OUT PVOID* ppFound, PVOID base)
    {
        //ASSERT(ppFound != NULL);
        if (ppFound == NULL)
            return STATUS_ACCESS_DENIED; //STATUS_INVALID_PARAMETER

        if (nullptr == base)
            base = Utils::GetDriverBaseAddress(NULL, RTL_CONSTANT_STRING(L"ntoskrnl.exe"));

        if (nullptr == base)
            return STATUS_ACCESS_DENIED; //STATUS_NOT_FOUND;

        PIMAGE_NT_HEADERS64 pHdr = (PIMAGE_NT_HEADERS64)RtlImageNtHeader(base);
        if (!pHdr)
            return STATUS_ACCESS_DENIED; // STATUS_INVALID_IMAGE_FORMAT;

        //PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)(pHdr + 1);
        PIMAGE_SECTION_HEADER pFirstSection = (PIMAGE_SECTION_HEADER)((uintptr_t)&pHdr->FileHeader + pHdr->FileHeader.SizeOfOptionalHeader + sizeof(IMAGE_FILE_HEADER));

        PVOID ptr = NULL;

        for (PIMAGE_SECTION_HEADER pSection = pFirstSection; pSection < pFirstSection + pHdr->FileHeader.NumberOfSections; pSection++)
        {
            ANSI_STRING s1, s2;

            RtlInitAnsiString(&s1, section);
            RtlInitAnsiString(&s2, (PCCHAR)pSection->Name);

            if (((RtlCompareString(&s1, &s2, TRUE) == 0) || (pSection->Characteristics & IMAGE_SCN_CNT_CODE) ||
                (pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE)))
            {
                NTSTATUS status = BBSearchPattern(pattern, wildcard, len, (PUCHAR)base + pSection->VirtualAddress, pSection->Misc.VirtualSize, &ptr);

                if (NT_SUCCESS(status)) {

                    *(PULONG64)ppFound = (ULONG_PTR)(ptr); //- (PUCHAR)base
                    DbgPrint("found\r\n");
                    return status;
                }

            }
            
        }

        return STATUS_ACCESS_DENIED; //STATUS_NOT_FOUND;
    }


}