/**
 * 
 * 
 * 
 * 
 * 
**/


#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

// must change both together
#define NUMBER_PAGES 512
#define NUMBER_PAGE_BITS 9 // (log 2 of NUMBER_PAGES)
#define PAGE_SIZE 4096

typedef enum _PFN_PAGE_LOCATION {
    Free,
    Zero,
    Modified,
    Standby,
    Active,
    Bad
} PFN_PAGE_LOCATION, *PPFN_PAGE_LOCATION;

// create struct for PFN entries
typedef struct _PFN_METADATA {
    PVOID Flink;
    PVOID Blink;
    PFN_PAGE_LOCATION PageLocation;
} PFN_METADATA, *PPFN_METADATA;

typedef struct _PTE_METADATA {
    // valid bit
    ULONG64 Valid: 1; // syntax: ulong64 that only uses 1 bit
    ULONG64 Transition: 1;
    ULONG64 Dirty: 1;
    ULONG64 PFNIndex: NUMBER_PAGE_BITS;
    // PVOID Flink;
    // PVOID Blink;
} PTE_METADATA, *PPTE_METADATA;

typedef struct _VAD_NODE {
    // valid bit
    ULONG64 Valid: 1;
    // links for the tree
    PVOID Parent;
    PVOID Child;
} VAD_NODE, *PVAD_NODE;

// create global list heads
LIST_ENTRY FreeListHead;
LIST_ENTRY ZeroListHead;
LIST_ENTRY ModifiedListHead;
LIST_ENTRY StandbyListHead;
LIST_ENTRY BadListHead;

// create global coutner variables
ULONG FreeCount;
ULONG ZeroCount;
ULONG ModifiedCount;
ULONG StandbyCount;
ULONG BadCount;

// used to calculate leaf page offset
PVOID BaseLeafPageAddress;
PPTE_METADATA BasePTEArrayAddress;
PPFN_METADATA BasePFNArrayAddress;

VOID
EnqueueToHead (
    PLIST_ENTRY ListHead,
    PLIST_ENTRY Add
    )
{
    PLIST_ENTRY PreviousFirst;

    PreviousFirst = ListHead->Flink;

    PreviousFirst->Blink = Add;
    Add->Flink = PreviousFirst;
    ListHead->Flink = Add;
    Add->Blink = ListHead;
}

PLIST_ENTRY
DequeueFromHead (
    PLIST_ENTRY ListHead
    )
{
    if (ListHead->Flink == ListHead) {
        printf("error. list empty\n");
        return  NULL;
    }
    else {
        PLIST_ENTRY Remove;
        PLIST_ENTRY NewFirst;

        Remove = ListHead->Flink;
        NewFirst = Remove->Flink;

        ListHead->Flink = NewFirst;
        NewFirst->Blink = ListHead;
        
        return Remove;
    }
}

VOID
DequeueFromList (
    PLIST_ENTRY Remove
    )
{
    PLIST_ENTRY BeforeRemove;
    PLIST_ENTRY AfterRemove;

    BeforeRemove = Remove->Blink;
    AfterRemove = Remove->Flink;
    BeforeRemove->Flink = AfterRemove;
    AfterRemove->Blink = BeforeRemove;

    return;
}

VOID
InitializeListHead (
    PLIST_ENTRY ListHead
    )
{
    ListHead->Flink = ListHead;
    ListHead->Blink = ListHead;
}

#if 0
VOID
MyAlloc (
)
{
    PPFN_METADATA NewPFN;

    // test malloc
    NewPFN = malloc (sizeof (PFN_METADATA));
    if (NewPFN == NULL) {
        printf("error creating PFN");
        exit (-1);
    }
    else {

    }
}
#endif

VOID
DemandZero (
    )
{

}

VOID
PageFault (
    PVOID LeafPageAddress
    )
{
    ULONG_PTR OffsetBytes;
    ULONG_PTR OffsetPage;
    PPTE_METADATA Pte;

    // take the offset of given leaf page, and divide by 4k to get page offset
    OffsetBytes = (ULONG_PTR) LeafPageAddress - (ULONG_PTR) BaseLeafPageAddress;
    OffsetPage = OffsetBytes / PAGE_SIZE;
    Pte = BasePTEArrayAddress + OffsetPage; // reason dont need sizeof is bc adding things of size PTE, compiler knows size of base PTE address
    
    if (Pte->Valid == 1) {
        printf("already valid\n");
        return;
    }
    if (Pte->Transition == 1) {
        ULONG64 Dequeued;
        PPFN_METADATA DequeuedAddres;
        printf("a");
        Dequeued = Pte->PFNIndex;
        DequeuedAddres = BasePFNArrayAddress + Dequeued;

        DequeueFromList((PLIST_ENTRY) DequeuedAddres);

        Pte->Transition = 0;
        Pte->Valid = 1;
        StandbyCount -= 1;

        printf("pte address: %p, removed offset page: %I64u\n", Pte, Dequeued);

        return;
    }
    else {
        PPFN_METADATA Removed;
        ULONG_PTR RemovedOffsetPage;
        
        //printf("%p, %p, %p", &FreeListHead, FreeListHead.Flink, FreeListHead.Blink);
        Removed = (PPFN_METADATA) DequeueFromHead(&FreeListHead);
        if (Removed == NULL) {
            printf("error. no page available\n");
            return;
        }
        RemovedOffsetPage = Removed - BasePFNArrayAddress; // same as Pte addition, but doesnt matter what is being set to it
        // compiler knows that math operation on pointers deals with size of the element of the pointer 
        // equivalent for above would be: RemovedOffsetPage = ((ULONG_PTR) Removed - (ULONG_PTR) BasePFNArrayAddress) / sizeof (PFN_METADATA);
        //printf("%p", Removed);
        if (Removed->PageLocation == Active) {
            printf("error. already active\n");
            return;
        }
        else {
            Removed->PageLocation = Active;
            FreeCount -= 1;
            Pte->Valid = 1;
            Pte->PFNIndex = RemovedOffsetPage;
            printf("pte address: %p, removed offset page: %d, %x\n", Pte, RemovedOffsetPage, RemovedOffsetPage);
        }
    }
    return;
}

/** active -> standby [x]:
 * set pte valid bit to 0
 * set pte transition bit to 1
 * add pfn standby list, increase standby count by 1 (done automatically)
**/
VOID
ActiveToStandby (
    PVOID LeafPageAddress
    )
{
    PPTE_METADATA Pte;
    PPFN_METADATA Pfn;
    ULONG_PTR OffsetBytes;
    ULONG_PTR OffsetPage;
    ULONG_PTR ATSOffsetBytes;
    ULONG_PTR ATSOffsetPage;

    OffsetBytes = (ULONG_PTR) LeafPageAddress - (ULONG_PTR) BaseLeafPageAddress;
    OffsetPage = OffsetBytes / PAGE_SIZE;
    Pte = BasePTEArrayAddress + OffsetPage;

    // we dont need to get the corresponding pfn, but rather just a pfn that is on the active list
    // we already filled in pte with a pfn index
    
    //printf("1");
    
    if (Pte->Valid == 1) {
        //printf("2");

        Pfn = BasePFNArrayAddress + Pte->PFNIndex;

        Pte->Valid = 0;
        Pte->Transition = 1;
        StandbyCount += 1;

        // need to add pfn here, not pte
        EnqueueToHead (&StandbyListHead, (PLIST_ENTRY)Pfn);
        printf("successfully removed page from active\n");

        return;
    }
    //printf("not an active page");

    return;
}

VOID
main (
    )
{
    SYSTEM_INFO SystemInfo;
    PVOID ATSBaseLeafPageAddress;
    PVOID LeafAddress;
    
#if 1
    InitializeListHead(&FreeListHead);
    InitializeListHead(&ZeroListHead);
    InitializeListHead(&ModifiedListHead);
    InitializeListHead(&StandbyListHead);
    InitializeListHead(&BadListHead);
#else
    FreeListHead.Flink = &FreeListHead;
    FreeListHead.Blink = &FreeListHead;

    ZeroListHead.Flink = &ZeroListHead;
    ZeroListHead.Blink = &ZeroListHead;

    ModifiedListHead.Flink = &ModifiedListHead;
    ModifiedListHead.Blink = &ModifiedListHead;

    StandbyListHead.Blink = &StandbyListHead;
    StandbyListHead.Blink = &StandbyListHead;

    BadListHead.Flink = &BadListHead;
    BadListHead.Blink = &BadListHead;
#endif

    GetSystemInfo(&SystemInfo);
    // find how much physical memory is in machine

    // call virtual alloc for page data
    BaseLeafPageAddress = VirtualAlloc (NULL, NUMBER_PAGES * PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (BaseLeafPageAddress == NULL) {
        printf("error. could not allocate memory\n");
        exit (-1);
    }

    // create PTE metadata array
    BasePTEArrayAddress = VirtualAlloc (NULL, NUMBER_PAGES * sizeof (PTE_METADATA), MEM_COMMIT, PAGE_READWRITE);
    if (BasePTEArrayAddress == NULL) {
        printf("error. could not allocate PTE array\n");
        exit (-1);
    }

    // create PFN array
    BasePFNArrayAddress = VirtualAlloc (NULL, NUMBER_PAGES * sizeof (PFN_METADATA), MEM_COMMIT, PAGE_READWRITE);
    if (BasePFNArrayAddress == NULL) {
        printf("error. could not allocate PFN array\n");
        exit (-1);
    }
    else {
        for (ULONG i = 0; i < NUMBER_PAGES; i += 1) {
            PPFN_METADATA NewPFN;
            NewPFN = BasePFNArrayAddress + i; //dont need to do sizeof() because we already specify that its a PFN_METADATA, automatically multiplies i by variable to its left
            EnqueueToHead (&FreeListHead, (PLIST_ENTRY) NewPFN);
            FreeCount += 1;
        }
    }

    for (ULONG i = 0; i < 10; i += 1) {
        LARGE_INTEGER r;
        ULONG ModdedR;

        // generating random address to page fault
        QueryPerformanceCounter(&r);
        r.QuadPart <<= 12;
        ModdedR = r.QuadPart%(NUMBER_PAGES * PAGE_SIZE);
        printf("%x\n", ModdedR);
        PVOID LeafAddress;
        LeafAddress = (PVOID)((ULONG_PTR) BaseLeafPageAddress + ModdedR);

        PageFault (LeafAddress);
    }

    for (ULONG i = 0; i < NUMBER_PAGES; i += 1) {
        LeafAddress = (PVOID)((ULONG_PTR)BaseLeafPageAddress + (i * PAGE_SIZE));

        PageFault(LeafAddress);
    }

    LeafAddress = BaseLeafPageAddress;
    for (ULONG i = 0; i < NUMBER_PAGES; i += 1) {
        (ULONG_PTR) LeafAddress = (ULONG_PTR) LeafAddress + (i * PAGE_SIZE);

        ActiveToStandby(LeafAddress);
    }

    LeafAddress = BaseLeafPageAddress;
    printf("b");
    for (ULONG i = 0; i < NUMBER_PAGES; i += 1) {
        printf("1");
        PVOID LeafAddress;
        LeafAddress = (PVOID)((ULONG_PTR)BaseLeafPageAddress + (i * PAGE_SIZE));

        PageFault(LeafAddress);
    }
    
    return;
}

// have another for loop to take from active to standby
// need to acces from the ones that are already accessed by the previous for loop
// how to take out and put back from active
// to take out: 
// find active pages: go through each pte, and check for ones with valid = 1
// set pte valid bit to 0
// set pte transition bit to 1. no longer active
// enqueue the pfn to standby list
// 
// accessor is like page fault
// when adding to active: 
// if pte not valid, then check if transition
// if transition, then dequeue from transition, set transition to 0, active to 1
// if not transition (like free), then i alkready have it written
//
// keep count for all listheads in a global variable


// stepping through process currently not working

/**
 * free -> active (page fault) [x]
 * active -> free (virtual free) [x]
 * 
 * standby -> active [x]
 * if the pte transition is 1
 * dequeue pfn from standby
 * set pte transition bit to 0
 * set pte active bit to 1
 * 
 * active -> standby [x]:
 * set valid to 0
 * set transition bit to 1
 * add to standby list, increase standby count by 1
 * decrement active by 1
 * add pfn to the standby list
 * 
 * free -> zero (zero page thread)
 * have zero page thread running, where it:
 * dequeues from free, decrements free
 * zero the page (memset the whole page to zero)
 * enqueue to zero, increment zero
 * 
 * zero -> active (page fault)
 * dequeue zero, decrement zero
 * increment active
 * set valid bit = 1
 * 
 * active -> modified [x]
 * if detected that freo, zero, and standby are running low (each have 10 pages)
 * set valid bit to 0. need to set this before looking at dirty bit because if valid bit is set at end, the sirty bit couldve been 0, and we think it hasnt been dirtied
 * but then it could be modified a moment later
 * set transition bit to 1
 * dirty bit = 1, means its been modified, otherwise put it on standby
 * decrement active
 * enqueue to modified, increment modified
 * 
 * if (FreeList->Count < 10)  //need to implement the struct properly first
 * call RemoveFromActive(some PVOIS input)
 * (we need a page trimming thread that tells u which to remove from active)
 * 
 * VOID
 * RemoveFromActive (
 *  PVOID Input
 * )
 * {
 * translate leaf page address to a Pte: 
 * (OffsetBytes = (ULONG_PTR) LeafPageAddress - (ULONG_PTR) BaseLeafPageAddress;
 * OffsetPage = OffsetBytes / PAGE_SIZE;
 * Pte = BasePTEArrayAddress + OffsetPage;)
 * if (Pte->Transition == 1 && Pte->Dirty == 1) {
 * Pte->Valid = 0;
 * Pte->Transition = 1;
 * Decrement (ActiveList)
 * EnqueueToHead (ModifiedList, PLIST_ENTRY Pte)
 * }
 * 
 * 
 * modified -> active
 * dequeue pfn from modified list, decrement modified
 * set pte transition <- 0
 * set pte valid bit <- 1
 * 
 * in PageFault: if(Pte->Transition = 1 && Pte->Dirty = 1)
 * DequeueFromList (Pfn);
 * Pte->Transition = 0;
 * Pte->Valid = 1;
 * 
 * 
 * modified -> standby
 * if pte transition bit = 1 and pte dirty bit = 1
 * dequeue pfn from modified list (decrement modified count by 1)
 * enqueue pfn to standby list (increment standby count by 1)
 * set dirty bit to 0
 * 
 * Q: conditions for this transition to happen
 * if (meet above conditions) {
 * if (Pte->Transition = 1 && Pte->Dirty == 1) {
 * DequeueFromList (Pfn);
 * EnqueueToHead (StandbyList, PLIST_ENTRY Pte);
 * Pte->Dirty = 0;
 * }
 * }
 * 
 * adding to bad
**/