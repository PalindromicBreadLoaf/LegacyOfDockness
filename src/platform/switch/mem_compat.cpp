#include <cerrno>
#include <cstdio>
#include <cstring>
#include <malloc.h>
#include <vector>

#include <switch.h>

#include "lod/lod_mem_compat.h"
#include "lod/lod_switch.hpp"

extern "C" uint8_t* lod_switch_rdram_alloc(size_t allocation_size, size_t mem_size);
extern "C" void lod_switch_rdram_free(uint8_t* rdram);

namespace {

constexpr u64 kPageSize = 0x1000;

struct Backing {
    void* source;
    void* target;
    u64 size;
};

Handle g_process = INVALID_HANDLE;
u64 g_committed_bytes = 0;
u8* g_rdram = nullptr;
u64 g_rdram_size = 0;
VirtmemReservation* g_reservation = nullptr;
std::vector<Backing> g_backings;

u64 info(u64 type) {
    u64 value = 0;
    svcGetInfo(&value, type, CUR_PROCESS_HANDLE, 0);
    return value;
}

bool commit(u64 target, u64 size) {
    void* source = memalign(kPageSize, size);
    if (source == nullptr) {
        fprintf(stderr, "[rdram] no heap left for %llu MB of backing pages\n",
                static_cast<unsigned long long>(size / (1024 * 1024)));
        return false;
    }

    const u64 source_addr = reinterpret_cast<u64>(source);
    Result rc = svcMapProcessCodeMemory(g_process, target, source_addr, size);
    if (R_FAILED(rc)) {
        fprintf(stderr, "[rdram] svcMapProcessCodeMemory(0x%llX <- 0x%llX, 0x%llX) failed: 0x%X\n",
                static_cast<unsigned long long>(target),
                static_cast<unsigned long long>(source_addr),
                static_cast<unsigned long long>(size), rc);
        free(source);
        return false;
    }

    rc = svcSetProcessMemoryPermission(g_process, target, size, Perm_Rw);
    if (R_FAILED(rc)) {
        fprintf(stderr, "[rdram] svcSetProcessMemoryPermission(0x%llX, 0x%llX) failed: 0x%X\n",
                static_cast<unsigned long long>(target),
                static_cast<unsigned long long>(size), rc);
        svcUnmapProcessCodeMemory(g_process, target, source_addr, size);
        free(source);
        return false;
    }

    memset(reinterpret_cast<void*>(target), 0, size);
    g_backings.push_back({source, reinterpret_cast<void*>(target), size});
    g_committed_bytes += size;
    return true;
}

void* reserve(u64 size) {
    virtmemLock();
    void* candidate = virtmemFindCodeMemory(size, 0);
    if (candidate != nullptr) {
        g_reservation = virtmemAddReservation(candidate, size);
        if (g_reservation == nullptr) {
            candidate = nullptr;
        }
    }
    virtmemUnlock();
    return candidate;
}

}  // namespace

extern "C" uint8_t* lod_switch_rdram_alloc(size_t allocation_size, size_t mem_size) {
    g_process = envGetOwnProcessHandle();
    fprintf(stderr, "[rdram] process handle 0x%X, MapProcessCodeMemory=%s SetProcessMemoryPermission=%s\n",
            g_process,
            envIsSyscallHinted(0x77) ? "yes" : "no",
            envIsSyscallHinted(0x73) ? "yes" : "no");
    fprintf(stderr,
            "[rdram] address space: aslr 0x%llX+0x%llX stack 0x%llX+0x%llX alias 0x%llX+0x%llX "
            "heap 0x%llX+0x%llX, system resource %llu MB\n",
            static_cast<unsigned long long>(info(InfoType_AslrRegionAddress)),
            static_cast<unsigned long long>(info(InfoType_AslrRegionSize)),
            static_cast<unsigned long long>(info(InfoType_StackRegionAddress)),
            static_cast<unsigned long long>(info(InfoType_StackRegionSize)),
            static_cast<unsigned long long>(info(InfoType_AliasRegionAddress)),
            static_cast<unsigned long long>(info(InfoType_AliasRegionSize)),
            static_cast<unsigned long long>(info(InfoType_HeapRegionAddress)),
            static_cast<unsigned long long>(info(InfoType_HeapRegionSize)),
            static_cast<unsigned long long>(info(InfoType_SystemResourceSizeTotal) / (1024 * 1024)));

    void* base = reserve(allocation_size);
    if (base == nullptr) {
        fprintf(stderr, "[rdram] no %llu MB of free code-memory address space\n",
                static_cast<unsigned long long>(allocation_size / (1024 * 1024)));
        return nullptr;
    }

    g_rdram = static_cast<u8*>(base);
    g_rdram_size = allocation_size;
    if (!commit(reinterpret_cast<u64>(base), mem_size)) {
        lod_switch_rdram_free(g_rdram);
        return nullptr;
    }

    fprintf(stderr, "[rdram] reserved %llu MB at %p, base %llu MB committed from the heap\n",
            static_cast<unsigned long long>(allocation_size / (1024 * 1024)), base,
            static_cast<unsigned long long>(mem_size / (1024 * 1024)));
    return g_rdram;
}

extern "C" void lod_switch_rdram_free(uint8_t* rdram) {
    if (rdram != g_rdram || g_rdram == nullptr) {
        return;
    }

    for (auto it = g_backings.rbegin(); it != g_backings.rend(); ++it) {
        if (R_SUCCEEDED(svcUnmapProcessCodeMemory(g_process, reinterpret_cast<u64>(it->target),
                                                  reinterpret_cast<u64>(it->source), it->size))) {
            free(it->source);
        }
    }
    g_backings.clear();
    g_committed_bytes = 0;

    if (g_reservation != nullptr) {
        virtmemLock();
        virtmemRemoveReservation(g_reservation);
        virtmemUnlock();
        g_reservation = nullptr;
    }
    g_rdram = nullptr;
    g_rdram_size = 0;
}

int mprotect(void* addr, size_t len, int prot) {
    if (prot != (PROT_READ | PROT_WRITE)) {
        errno = ENOTSUP;
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    const u64 start = reinterpret_cast<u64>(addr) & ~(kPageSize - 1);
    const u64 end = (reinterpret_cast<u64>(addr) + len + kPageSize - 1) & ~(kPageSize - 1);

    const u64 rdram = reinterpret_cast<u64>(g_rdram);
    if (g_rdram == nullptr || start < rdram || end > rdram + g_rdram_size) {
        fprintf(stderr, "[mem_compat] 0x%llX..0x%llX is outside the rdram reservation\n",
                static_cast<unsigned long long>(start), static_cast<unsigned long long>(end));
        errno = EINVAL;
        return -1;
    }

    for (u64 cursor = start; cursor < end;) {
        MemoryInfo region = {};
        u32 page_info = 0;
        if (R_FAILED(svcQueryMemory(&region, &page_info, cursor))) {
            errno = EINVAL;
            return -1;
        }

        const u64 region_end = region.addr + region.size;
        if (region_end <= cursor) {
            errno = EINVAL;
            return -1;
        }

        const u64 chunk_end = region_end < end ? region_end : end;
        if (region.type == MemType_Unmapped && !commit(cursor, chunk_end - cursor)) {
            fprintf(stderr, "[mem_compat] commit failed at 0x%llX (%llu MB backed so far)\n",
                    static_cast<unsigned long long>(cursor),
                    static_cast<unsigned long long>(g_committed_bytes / (1024 * 1024)));
            errno = ENOMEM;
            return -1;
        }
        cursor = chunk_end;
    }

    return 0;
}

uint64_t lod::sw::committed_rdram_bytes() {
    return g_committed_bytes;
}
