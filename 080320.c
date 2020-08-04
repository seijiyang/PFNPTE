#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

// create struct for PFN entries
typedef struct _PFN_METADATA {
    // do we need flink blink if we store in array?
    PVOID Flink;
    PVOID Blink;
} PFN_METADATA, *PPFN_METADATA;

typedef struct _VAD_NODE {
    // valid bit
    BOOL Valid;
    // links for the tree
    PVOID Parent;
    PVOID Child;
} VAD_NODE, *PVAD_NODE;

// create global list heads
PLIST_ENTRY FreeListHead;
PLIST_ENTRY ZeroListHead;
PLIST_ENTRY ModifiedListHead;
PLIST_ENTRY StandbyListHead;
PLIST_ENTRY BadListHead;

VOID
MyAlloc (
)
{
    PPFN_METADATA NewPFN;

    // test malloc
    NewPFN = malloc (sizeof (PFN_METADATA));
    if (NewPFN == NULL) {
        printf("error creating PFN");
        exit;
    }
    else {
        AddToFree (NewPFN);
    }
}


VOID
AddToFree ( 
    PPFN_METADATA AddPFN
)
{
    // previous first PFN in list
    PPFN_METADATA ReplacePFN;

    ReplacePFN = FreeListHead->Flink;

    ReplacePFN->Blink = AddPFN;
    AddPFN->Flink = ReplacePFN;
    FreeListHead->Flink = AddPFN;
    AddPFN->Blink = FreeListHead;
}

VOID
DemandZero (
)
{

}

VOID
Main (
)
{
    SYSTEM_INFO SystemInfo;

    // initialize list heads
    FreeListHead->Flink = FreeListHead;
    FreeListHead->Blink = FreeListHead;

    ZeroListHead->Flink = ZeroListHead;
    ZeroListHead->Blink = ZeroListHead;

    ModifiedListHead->Flink = ModifiedListHead;
    ModifiedListHead->Blink = ModifiedListHead;

    StandbyListHead->Flink = StandbyListHead;
    StandbyListHead->Blink = StandbyListHead;

    BadListHead->Flink = BadListHead;
    BadListHead->Blink = BadListHead;

    GetSystemInfo(&SystemInfo);
    // find how much physical memory is in machine
}