/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gdi.h"

#include "core/memory_manager/memory_constants.h"

ADAPTER_INFO gAdapterInfo = {0};
D3DDDI_MAPGPUVIRTUALADDRESS gLastCallMapGpuVaArg = {0};
D3DDDI_RESERVEGPUVIRTUALADDRESS gLastCallReserveGpuVaArg = {0};
uint32_t gMapGpuVaFailConfigCount = 0;
uint32_t gMapGpuVaFailConfigMax = 0;
uint64_t gGpuAddressSpace = 0ull;

#ifdef __cplusplus // If used by C++ code,
extern "C" {       // we need to export the C interface
#endif
BOOLEAN WINAPI DllMain(IN HINSTANCE hDllHandle,
                       IN DWORD nReason,
                       IN LPVOID Reserved) {
    return TRUE;
}

NTSTATUS __stdcall D3DKMTEscape(IN CONST D3DKMT_ESCAPE *pData) {
    static int PerfTicks = 0;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.RetCode = NEO::GTDI_RET_OK;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.gpuPerfTicks = ++PerfTicks;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.cpuPerfTicks = PerfTicks;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.gpuPerfFreq = 1;
    ((NEO::TimeStampDataHeader *)pData->pPrivateDriverData)->m_Data.m_Out.cpuPerfFreq = 1;

    return STATUS_SUCCESS;
}

DECL_FUNCTIONS()

UINT64 PagingFence = 0;

void __stdcall MockSetAdapterInfo(const void *pGfxPlatform, const void *pGTSystemInfo, uint64_t gpuAddressSpace) {
    if (pGfxPlatform != NULL) {
        gAdapterInfo.GfxPlatform = *(PLATFORM *)pGfxPlatform;
    }
    if (pGTSystemInfo != NULL) {
        gAdapterInfo.SystemInfo = *(GT_SYSTEM_INFO *)pGTSystemInfo;
    }
    gGpuAddressSpace = gpuAddressSpace;
    InitGfxPartition();
}

NTSTATUS __stdcall D3DKMTOpenAdapterFromLuid(IN OUT CONST D3DKMT_OPENADAPTERFROMLUID *openAdapter) {
    if (openAdapter == nullptr || (openAdapter->AdapterLuid.HighPart == 0 && openAdapter->AdapterLuid.LowPart == 0)) {
        return STATUS_INVALID_PARAMETER;
    }
    D3DKMT_OPENADAPTERFROMLUID *openAdapterNonConst = const_cast<D3DKMT_OPENADAPTERFROMLUID *>(openAdapter);
    openAdapterNonConst->hAdapter = ADAPTER_HANDLE;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTCreateDevice(IN OUT D3DKMT_CREATEDEVICE *createDevice) {
    if (createDevice == nullptr || createDevice->hAdapter != ADAPTER_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    createDevice->hDevice = DEVICE_HANDLE;
    SetMockCreateDeviceParams(*createDevice);
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTDestroyDevice(IN CONST D3DKMT_DESTROYDEVICE *destoryDevice) {
    if (destoryDevice == nullptr || destoryDevice->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTCreatePagingQueue(IN OUT D3DKMT_CREATEPAGINGQUEUE *createQueue) {
    if (createQueue == nullptr || (createQueue->hDevice != DEVICE_HANDLE)) {
        return STATUS_INVALID_PARAMETER;
    }
    createQueue->hPagingQueue = PAGINGQUEUE_HANDLE;
    createQueue->hSyncObject = PAGINGQUEUE_SYNCOBJECT_HANDLE;
    createQueue->FenceValueCPUVirtualAddress = &PagingFence;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTDestroyPagingQueue(IN OUT D3DDDI_DESTROYPAGINGQUEUE *destoryQueue) {
    if (destoryQueue == nullptr || destoryQueue->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static D3DKMT_CREATECONTEXTVIRTUAL createContextData = {0};
static CREATECONTEXT_PVTDATA createContextPrivateData = {{0}};

NTSTATUS __stdcall D3DKMTCreateContextVirtual(IN D3DKMT_CREATECONTEXTVIRTUAL *createContext) {
    if (createContext == nullptr || createContext->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }

    createContextData = *createContext;
    if (createContext->pPrivateDriverData) {
        createContextPrivateData = *((CREATECONTEXT_PVTDATA *)createContext->pPrivateDriverData);
        createContextData.pPrivateDriverData = &createContextPrivateData;
    }

    if ((createContext->PrivateDriverDataSize != 0 && createContext->pPrivateDriverData == nullptr) ||
        (createContext->PrivateDriverDataSize == 0 && createContext->pPrivateDriverData != nullptr)) {
        return STATUS_INVALID_PARAMETER;
    }
    createContext->hContext = CONTEXT_HANDLE;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTDestroyContext(IN CONST D3DKMT_DESTROYCONTEXT *destroyContext) {
    if (destroyContext == nullptr || destroyContext->hContext != CONTEXT_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static D3DKMT_CREATEALLOCATION *pallocation = nullptr;

NTSTATUS __stdcall D3DKMTCreateAllocation(IN OUT D3DKMT_CREATEALLOCATION *allocation) {
    D3DDDI_ALLOCATIONINFO *allocationInfo;
    int numOfAllocations;
    bool createResource;
    bool globalShare;
    pallocation = allocation;
    if (allocation == nullptr || allocation->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    allocationInfo = allocation->pAllocationInfo;
    if (allocationInfo == NULL) {
        return STATUS_INVALID_PARAMETER;
    }
    numOfAllocations = allocation->NumAllocations;
    createResource = allocation->Flags.CreateResource;
    globalShare = allocation->Flags.CreateShared;
    if (createResource) {
        allocation->hResource = RESOURCE_HANDLE;
    }
    if (globalShare) {
        allocation->hGlobalShare = RESOURCE_HANDLE;
    }

    for (int i = 0; i < numOfAllocations; ++i) {
        if (allocationInfo != NULL) {
            allocationInfo->hAllocation = ALLOCATION_HANDLE;
        }
        allocationInfo++;
    }

    return STATUS_SUCCESS;
}

static unsigned int DestroyAllocationWithResourceHandleCalled = 0u;
static D3DKMT_DESTROYALLOCATION2 destroyalloc2 = {0};
static D3DKMT_HANDLE LastDestroyedResourceHandle = 0;
static D3DKMT_CREATEDEVICE CreateDeviceParams = {{0}};

NTSTATUS __stdcall D3DKMTDestroyAllocation2(IN CONST D3DKMT_DESTROYALLOCATION2 *destroyAllocation) {
    int numOfAllocations;
    const D3DKMT_HANDLE *allocationList;
    LastDestroyedResourceHandle = 0;
    if (destroyAllocation == nullptr || destroyAllocation->hDevice != DEVICE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    destroyalloc2 = *destroyAllocation;
    numOfAllocations = destroyAllocation->AllocationCount;
    allocationList = destroyAllocation->phAllocationList;

    for (int i = 0; i < numOfAllocations; ++i) {
        if (allocationList != NULL) {
            if (*allocationList != ALLOCATION_HANDLE) {
                return STATUS_UNSUCCESSFUL;
            }
        }
        allocationList++;
    }
    if (numOfAllocations == 0 && destroyAllocation->hResource == 0u) {
        return STATUS_UNSUCCESSFUL;
    }
    if (destroyAllocation->hResource) {
        DestroyAllocationWithResourceHandleCalled = 1;
        LastDestroyedResourceHandle = destroyAllocation->hResource;
    }

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTMapGpuVirtualAddress(IN OUT D3DDDI_MAPGPUVIRTUALADDRESS *mapGpuVA) {
    if (mapGpuVA == nullptr) {
        memset(&gLastCallMapGpuVaArg, 0, sizeof(gLastCallMapGpuVaArg));
        return STATUS_INVALID_PARAMETER;
    }

    memcpy(&gLastCallMapGpuVaArg, mapGpuVA, sizeof(gLastCallMapGpuVaArg));

    if (mapGpuVA->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    if (mapGpuVA->hAllocation != ALLOCATION_HANDLE && mapGpuVA->hAllocation != NT_ALLOCATION_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    if (mapGpuVA->MinimumAddress != 0) {
        if (mapGpuVA->BaseAddress != 0 && mapGpuVA->BaseAddress < mapGpuVA->MinimumAddress) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (mapGpuVA->MaximumAddress != 0) {
        if (mapGpuVA->BaseAddress != 0 && mapGpuVA->BaseAddress > mapGpuVA->MaximumAddress) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    if (mapGpuVA->BaseAddress == 0) {
        if (mapGpuVA->MinimumAddress) {
            mapGpuVA->VirtualAddress = mapGpuVA->MinimumAddress;
        } else {
            mapGpuVA->VirtualAddress = MemoryConstants::pageSize64k;
        }
    } else {
        if (MemoryConstants::maxSvmAddress != mapGpuVA->MaximumAddress && gLastCallReserveGpuVaArg.MinimumAddress != mapGpuVA->BaseAddress) {
            return STATUS_INVALID_PARAMETER;
        }
        mapGpuVA->VirtualAddress = mapGpuVA->BaseAddress;
    }

    if (gMapGpuVaFailConfigMax != 0) {
        if (gMapGpuVaFailConfigMax > gMapGpuVaFailConfigCount) {
            gMapGpuVaFailConfigCount++;
            return STATUS_UNSUCCESSFUL;
        }
    }

    mapGpuVA->PagingFenceValue = 1;
    return STATUS_PENDING;
}

NTSTATUS __stdcall D3DKMTReserveGpuVirtualAddress(IN OUT D3DDDI_RESERVEGPUVIRTUALADDRESS *reserveGpuVirtualAddress) {
    gLastCallReserveGpuVaArg = *reserveGpuVirtualAddress;
    reserveGpuVirtualAddress->VirtualAddress = reserveGpuVirtualAddress->MinimumAddress;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTQueryAdapterInfo(IN CONST D3DKMT_QUERYADAPTERINFO *queryAdapterInfo) {
    if (queryAdapterInfo == nullptr || queryAdapterInfo->hAdapter != ADAPTER_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    if (queryAdapterInfo->Type == KMTQAITYPE_UMDRIVERPRIVATE) {
        if (queryAdapterInfo->pPrivateDriverData == NULL) {
            return STATUS_INVALID_PARAMETER;
        }
        if (queryAdapterInfo->PrivateDriverDataSize == 0) {
            return STATUS_INVALID_PARAMETER;
        }
    }
    ADAPTER_INFO *adapterInfo = reinterpret_cast<ADAPTER_INFO *>(queryAdapterInfo->pPrivateDriverData);

    adapterInfo->GfxPlatform = gAdapterInfo.GfxPlatform;
    adapterInfo->SystemInfo = gAdapterInfo.SystemInfo;
    adapterInfo->SkuTable = gAdapterInfo.SkuTable;
    adapterInfo->WaTable = gAdapterInfo.WaTable;
    adapterInfo->CacheLineSize = 64;
    adapterInfo->MinRenderFreq = 350;
    adapterInfo->MaxRenderFreq = 1150;

    adapterInfo->SizeOfDmaBuffer = 32768;
    adapterInfo->GfxMemorySize = 2181038080;
    adapterInfo->SystemSharedMemory = 4249540608;
    adapterInfo->SystemVideoMemory = 0;

    adapterInfo->GfxPartition.Standard.Base = gAdapterInfo.GfxPartition.Standard.Base;
    adapterInfo->GfxPartition.Standard.Limit = gAdapterInfo.GfxPartition.Standard.Limit;
    adapterInfo->GfxPartition.Standard64KB.Base = gAdapterInfo.GfxPartition.Standard64KB.Base;
    adapterInfo->GfxPartition.Standard64KB.Limit = gAdapterInfo.GfxPartition.Standard64KB.Limit;

    adapterInfo->GfxPartition.SVM.Base = gAdapterInfo.GfxPartition.SVM.Base;
    adapterInfo->GfxPartition.SVM.Limit = gAdapterInfo.GfxPartition.SVM.Limit;
    adapterInfo->GfxPartition.Heap32[0].Base = gAdapterInfo.GfxPartition.Heap32[0].Base;
    adapterInfo->GfxPartition.Heap32[0].Limit = gAdapterInfo.GfxPartition.Heap32[0].Limit;
    adapterInfo->GfxPartition.Heap32[1].Base = gAdapterInfo.GfxPartition.Heap32[1].Base;
    adapterInfo->GfxPartition.Heap32[1].Limit = gAdapterInfo.GfxPartition.Heap32[1].Limit;
    adapterInfo->GfxPartition.Heap32[2].Base = gAdapterInfo.GfxPartition.Heap32[2].Base;
    adapterInfo->GfxPartition.Heap32[2].Limit = gAdapterInfo.GfxPartition.Heap32[2].Limit;
    adapterInfo->GfxPartition.Heap32[3].Base = gAdapterInfo.GfxPartition.Heap32[3].Base;
    adapterInfo->GfxPartition.Heap32[3].Limit = gAdapterInfo.GfxPartition.Heap32[3].Limit;

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTMakeResident(IN OUT D3DDDI_MAKERESIDENT *makeResident) {
    if (makeResident == nullptr || makeResident->hPagingQueue != PAGINGQUEUE_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    makeResident->PagingFenceValue = 0;
    return STATUS_PENDING;
}

static UINT totalPrivateSize = 0u;
static UINT gmmSize = 0u;
static void *gmmPtr = nullptr;
static UINT numberOfAllocsToReturn = 0u;

NTSTATUS __stdcall D3DKMTOpenResource(IN OUT D3DKMT_OPENRESOURCE *openResurce) {
    openResurce->hResource = RESOURCE_HANDLE;
    openResurce->pOpenAllocationInfo[0].hAllocation = ALLOCATION_HANDLE;
    openResurce->pOpenAllocationInfo[0].pPrivateDriverData = gmmPtr;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTOpenResourceFromNtHandle(IN OUT D3DKMT_OPENRESOURCEFROMNTHANDLE *openResurce) {
    openResurce->hResource = NT_RESOURCE_HANDLE;
    openResurce->pOpenAllocationInfo2[0].hAllocation = NT_ALLOCATION_HANDLE;
    openResurce->pOpenAllocationInfo2[0].pPrivateDriverData = gmmPtr;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTQueryResourceInfo(IN OUT D3DKMT_QUERYRESOURCEINFO *queryResourceInfo) {
    if (queryResourceInfo->hDevice != DEVICE_HANDLE || queryResourceInfo->hGlobalShare == INVALID_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    queryResourceInfo->TotalPrivateDriverDataSize = totalPrivateSize;
    queryResourceInfo->PrivateRuntimeDataSize = gmmSize;
    queryResourceInfo->NumAllocations = numberOfAllocsToReturn;

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTQueryResourceInfoFromNtHandle(IN OUT D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *queryResourceInfo) {
    if (queryResourceInfo->hDevice != DEVICE_HANDLE || queryResourceInfo->hNtHandle == INVALID_HANDLE) {
        return STATUS_INVALID_PARAMETER;
    }
    queryResourceInfo->TotalPrivateDriverDataSize = totalPrivateSize;
    queryResourceInfo->PrivateRuntimeDataSize = gmmSize;
    queryResourceInfo->NumAllocations = numberOfAllocsToReturn;

    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTLock2(IN OUT D3DKMT_LOCK2 *lock2) {
    if (lock2->hAllocation == 0 || lock2->hDevice == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    lock2->pData = (void *)65536;
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall D3DKMTUnlock2(IN CONST D3DKMT_UNLOCK2 *unlock2) {
    if (unlock2->hAllocation == 0 || unlock2->hDevice == 0) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

static size_t cpuFence = 0;

NTSTATUS __stdcall D3DKMTCreateSynchronizationObject2(IN OUT D3DKMT_CREATESYNCHRONIZATIONOBJECT2 *synchObject) {
    if (synchObject == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    synchObject->Info.MonitoredFence.FenceValueCPUVirtualAddress = &cpuFence;
    synchObject->Info.MonitoredFence.FenceValueGPUVirtualAddress = 3;
    synchObject->hSyncObject = 4;
    return STATUS_SUCCESS;
}

static D3DKMT_CREATEHWQUEUE createHwQueueData = {};

NTSTATUS __stdcall D3DKMTCreateHwQueue(IN OUT D3DKMT_CREATEHWQUEUE *createHwQueue) {
    createHwQueue->hHwQueueProgressFence = 1;
    createHwQueue->HwQueueProgressFenceCPUVirtualAddress = reinterpret_cast<void *>(2);
    createHwQueue->HwQueueProgressFenceGPUVirtualAddress = 3;
    createHwQueue->hHwQueue = 4;
    createHwQueueData = *createHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_DESTROYHWQUEUE destroyHwQueueData = {};

NTSTATUS __stdcall D3DKMTDestroyHwQueue(IN CONST D3DKMT_DESTROYHWQUEUE *destroyHwQueue) {
    destroyHwQueueData = *destroyHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_SUBMITCOMMANDTOHWQUEUE submitCommandToHwQueueData = {};

NTSTATUS __stdcall D3DKMTSubmitCommandToHwQueue(IN CONST D3DKMT_SUBMITCOMMANDTOHWQUEUE *submitCommandToHwQueue) {
    submitCommandToHwQueueData = *submitCommandToHwQueue;
    return STATUS_SUCCESS;
}

static D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroySynchronizationObjectData = {};

NTSTATUS __stdcall D3DKMTDestroySynchronizationObject(IN CONST D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *destroySynchronizationObject) {
    destroySynchronizationObjectData = *destroySynchronizationObject;
    return STATUS_SUCCESS;
}

#ifdef __cplusplus
}
#endif

NTSTATUS MockSetSizes(void *inGmmPtr, UINT inNumAllocsToReturn, UINT inGmmSize, UINT inTotalPrivateSize) {
    gmmSize = inGmmSize;
    gmmPtr = inGmmPtr;
    totalPrivateSize = inTotalPrivateSize;
    numberOfAllocsToReturn = inNumAllocsToReturn;
    return STATUS_SUCCESS;
}

NTSTATUS GetMockSizes(UINT &destroyAlloactionWithResourceHandleCalled, D3DKMT_DESTROYALLOCATION2 *&ptrDestroyAlloc) {
    destroyAlloactionWithResourceHandleCalled = DestroyAllocationWithResourceHandleCalled;
    ptrDestroyAlloc = &destroyalloc2;
    return NTSTATUS();
}

D3DKMT_HANDLE GetMockLastDestroyedResHandle() {
    return LastDestroyedResourceHandle;
}

void SetMockLastDestroyedResHandle(D3DKMT_HANDLE handle) {
    LastDestroyedResourceHandle = handle;
}

D3DKMT_CREATEDEVICE GetMockCreateDeviceParams() {
    return CreateDeviceParams;
}

void SetMockCreateDeviceParams(D3DKMT_CREATEDEVICE params) {
    CreateDeviceParams = params;
}

D3DKMT_CREATEALLOCATION *getMockAllocation() {
    return pallocation;
}

ADAPTER_INFO *getAdapterInfoAddress() {
    return &gAdapterInfo;
}

D3DDDI_MAPGPUVIRTUALADDRESS *getLastCallMapGpuVaArg() {
    return &gLastCallMapGpuVaArg;
}

D3DDDI_RESERVEGPUVIRTUALADDRESS *getLastCallReserveGpuVaArg() {
    return &gLastCallReserveGpuVaArg;
}

void setMapGpuVaFailConfig(uint32_t count, uint32_t max) {
    gMapGpuVaFailConfigCount = count;
    gMapGpuVaFailConfigMax = max;
}

D3DKMT_CREATECONTEXTVIRTUAL *getCreateContextData() {
    return &createContextData;
}

D3DKMT_CREATEHWQUEUE *getCreateHwQueueData() {
    return &createHwQueueData;
}

D3DKMT_DESTROYHWQUEUE *getDestroyHwQueueData() {
    return &destroyHwQueueData;
}

D3DKMT_SUBMITCOMMANDTOHWQUEUE *getSubmitCommandToHwQueueData() {
    return &submitCommandToHwQueueData;
}

D3DKMT_DESTROYSYNCHRONIZATIONOBJECT *getDestroySynchronizationObjectData() {
    return &destroySynchronizationObjectData;
}
