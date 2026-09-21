#include "lod/lod_switch.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <unistd.h>

#include <switch.h>

#ifndef LOD_SWITCH_HEAP_MB
#define LOD_SWITCH_HEAP_MB 0
#endif

extern "C" {
u32 __nx_applet_type = AppletType_Application;
size_t __nx_heap_size = static_cast<size_t>(LOD_SWITCH_HEAP_MB) * 1024 * 1024;
alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);
extern char* fake_heap_start;
extern char* fake_heap_end;
}

constexpr u64 kHeapSizeAlign = 0x200000;

namespace {

struct HeapSetup {
    u64 region_base;
    u64 original_end;
    u64 final_end;
    u64 usable_start;
    u64 loader_end;
    bool had_override;
    bool shrunk;
    Result shrink_rc;
};
HeapSetup g_heap{};

u64 scan_heap(u64 region_base, u64* loader_end) {
    u64 end = region_base;
    *loader_end = region_base;

    for (u64 cursor = region_base;;) {
        MemoryInfo info = {};
        u32 page_info = 0;
        if (R_FAILED(svcQueryMemory(&info, &page_info, cursor)) || info.type != MemType_Heap) {
            break;
        }

        const u64 chunk_end = info.addr + info.size;
        if (chunk_end <= cursor) {
            break;
        }
        end = chunk_end;
        if (info.attr != 0 || info.perm != Perm_Rw) {
            *loader_end = chunk_end;
        }
        cursor = chunk_end;
    }

    return end;
}

std::filesystem::path g_config_directory;
std::filesystem::path g_asset_directory;
bool g_romfs_up = false;
bool g_socket_up = false;
bool g_nxlink_up = false;
FILE* g_log_file = nullptr;
bool g_platform_up = false;

void flush_log() {
    std::fflush(stdout);
    std::fflush(stderr);
    if (g_log_file != nullptr) {
        std::fflush(g_log_file);
    }
}
u64 g_core_mask = 0;
AppletFocusState g_focus_state = AppletFocusState_InFocus;
AppletOperationMode g_operation_mode = AppletOperationMode_Handheld;
void (*g_crash_reporter)(void*) = nullptr;

const char* exception_desc_name(u32 desc) {
    switch (desc) {
        case ThreadExceptionDesc_InstructionAbort: return "instruction abort";
        case ThreadExceptionDesc_MisalignedPC: return "misaligned PC";
        case ThreadExceptionDesc_MisalignedSP: return "misaligned SP";
        case ThreadExceptionDesc_SError: return "SError";
        case ThreadExceptionDesc_BadSVC: return "bad SVC";
        case ThreadExceptionDesc_Trap: return "trap";
        case ThreadExceptionDesc_Other: return "data abort";
        default: return "unknown";
    }
}

void open_log(const std::filesystem::path& log_path) {
    if (R_SUCCEEDED(socketInitializeDefault())) {
        g_socket_up = true;
        if (nxlinkStdio() >= 0) {
            g_nxlink_up = true;
            setvbuf(stdout, nullptr, _IONBF, 0);
            setvbuf(stderr, nullptr, _IONBF, 0);
            return;
        }
    }

    g_log_file = std::fopen(log_path.string().c_str(), "w");
    if (g_log_file == nullptr) {
        return;
    }

    const int log_fd = fileno(g_log_file);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

void report_memory(const char* label) {
    u64 total = 0;
    u64 used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    fprintf(stderr,
            "[SWITCH] memory %s: %llu MB used of %llu MB, heap %llu MB, "
            "rdram committed beyond base %llu MB\n",
            label,
            static_cast<unsigned long long>(used / (1024 * 1024)),
            static_cast<unsigned long long>(total / (1024 * 1024)),
            static_cast<unsigned long long>(
                (fake_heap_end - fake_heap_start) / (1024 * 1024)),
            static_cast<unsigned long long>(lod::sw::committed_rdram_bytes() / (1024 * 1024)));
}

void restore_heap() {
    if (!g_heap.shrunk) {
        return;
    }

    void* addr = nullptr;
    const u64 original_size = g_heap.original_end - g_heap.region_base;
    const Result rc = svcSetHeapSize(&addr, original_size);
    fprintf(stderr, "[SWITCH] heap restored to %llu MB for the loader: %s (0x%X)\n",
            static_cast<unsigned long long>(original_size / (1024 * 1024)),
            R_SUCCEEDED(rc) ? "ok" : "failed", rc);
    if (R_SUCCEEDED(rc)) {
        g_heap.shrunk = false;
    }
}

void report_heap_setup() {
    fprintf(stderr,
            "[SWITCH] heap: override=%s base=0x%llX loader=%llu MB usable=0x%llX "
            "%llu MB (was %llu MB), requested cap %llu MB\n",
            g_heap.had_override ? "yes" : "no",
            static_cast<unsigned long long>(g_heap.region_base),
            static_cast<unsigned long long>((g_heap.loader_end - g_heap.region_base) /
                                            (1024 * 1024)),
            static_cast<unsigned long long>(g_heap.usable_start),
            static_cast<unsigned long long>((g_heap.final_end - g_heap.usable_start) /
                                            (1024 * 1024)),
            static_cast<unsigned long long>((g_heap.original_end - g_heap.usable_start) /
                                            (1024 * 1024)),
            static_cast<unsigned long long>(__nx_heap_size / (1024 * 1024)));
    if (g_heap.had_override && !g_heap.shrunk) {
        fprintf(stderr,
                "[SWITCH] heap was not shrunk (rc=0x%X): rdram can only use what the "
                "loader left free\n",
                g_heap.shrink_rc);
    }
}

constexpr int kCoreFloating = -1;

int preferred_core_for(const std::string& name) {
    if (name.rfind("[Game]", 0) == 0 || name == "Game Start Thread") {
        return 1;  // recompiled game code
    }
    if (name == "Gfx Thread" || name == "RT64 Workload" || name == "RT64 Present") {
        return 2;  // graphics submission
    }
    if (name == "Main Thread" || name == "SP Task Thread" ||
        name == "VI Thread" || name == "Timer Thread") {
        return 0;  // applet pump, audio RSP and frame timing
    }
    return kCoreFloating;
}

}  // namespace

extern "C" void __libnx_initheap(void) {
    void* addr = nullptr;
    const u64 wanted_heap = __nx_heap_size;

    g_heap.had_override = envHasHeapOverride();
    if (!g_heap.had_override) {
        u64 available = 0;
        u64 used = 0;
        svcGetInfo(&available, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
        svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
        const u64 free_heap = available > used + 0x200000
                                  ? (available - used - 0x200000) & ~(kHeapSizeAlign - 1)
                                  : 0;
        const u64 heap_size = wanted_heap != 0 ? wanted_heap : free_heap;
        if (R_FAILED(svcSetHeapSize(&addr, heap_size))) {
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed));
        }
        g_heap.region_base = reinterpret_cast<u64>(addr);
        g_heap.usable_start = g_heap.region_base;
        g_heap.original_end = g_heap.region_base + heap_size;
        g_heap.final_end = g_heap.original_end;
        fake_heap_start = static_cast<char*>(addr);
        fake_heap_end = fake_heap_start + heap_size;
        return;
    }

    u64 region_base = 0;
    svcGetInfo(&region_base, InfoType_HeapRegionAddress, CUR_PROCESS_HANDLE, 0);

    u64 loader_end = region_base;
    const u64 heap_end = scan_heap(region_base, &loader_end);
    const u64 override_start = reinterpret_cast<u64>(envGetHeapOverrideAddr());
    const u64 override_end = override_start + envGetHeapOverrideSize();
    u64 usable_start = override_start > loader_end ? override_start : loader_end;

    g_heap.region_base = region_base;
    g_heap.original_end = heap_end;
    g_heap.final_end = heap_end;
    g_heap.loader_end = loader_end;
    g_heap.usable_start = usable_start;

    if (wanted_heap != 0 && usable_start < heap_end) {
        u64 shrunk_size = usable_start - region_base + wanted_heap;
        shrunk_size = (shrunk_size + kHeapSizeAlign - 1) & ~(kHeapSizeAlign - 1);
        if (region_base + shrunk_size < heap_end) {
            g_heap.shrink_rc = svcSetHeapSize(&addr, shrunk_size);
            if (R_SUCCEEDED(g_heap.shrink_rc)) {
                g_heap.final_end = reinterpret_cast<u64>(addr) + shrunk_size;
                g_heap.shrunk = true;
            }
        }
    }

    if (usable_start >= g_heap.final_end) {
        usable_start = override_start;
        g_heap.usable_start = override_start;
        g_heap.final_end = override_end;
    }

    if (g_heap.final_end > override_end) {
        g_heap.final_end = override_end;
    }

    fake_heap_start = reinterpret_cast<char*>(usable_start);
    fake_heap_end = reinterpret_cast<char*>(g_heap.final_end);
}

void lod::sw::platform_init() {
    g_config_directory = std::filesystem::path("sdmc:/switch/lodrecomp");

    std::error_code ec;
    std::filesystem::create_directories(g_config_directory, ec);

    open_log(g_config_directory / "LodRecomp.log");

    g_romfs_up = R_SUCCEEDED(romfsInit());
    g_asset_directory = g_romfs_up ? std::filesystem::path("romfs:/")
                                   : g_config_directory;

    svcGetInfo(&g_core_mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
    g_focus_state = appletGetFocusState();
    g_operation_mode = appletGetOperationMode();

    place_current_thread("Main Thread");

    g_platform_up = true;
    std::atexit(flush_log);
    ::at_quick_exit(flush_log);

    fprintf(stderr, "[SWITCH] config=%s assets=%s romfs=%s output=%s cores=0x%llX mode=%s\n",
            g_config_directory.string().c_str(),
            g_asset_directory.string().c_str(),
            g_romfs_up ? "ok" : "unavailable",
            g_nxlink_up ? "nxlink" : (g_log_file != nullptr ? "sdmc log" : "none"),
            static_cast<unsigned long long>(g_core_mask),
            g_operation_mode == AppletOperationMode_Console ? "docked" : "handheld");
    if (ec) {
        fprintf(stderr, "[SWITCH] failed to create %s: %s\n",
                g_config_directory.string().c_str(), ec.message().c_str());
    }
    report_heap_setup();
    report_memory("at startup");
}

void lod::sw::platform_exit() {
    if (!g_platform_up) {
        return;
    }
    g_platform_up = false;

    report_memory("at shutdown");
    restore_heap();

    if (g_romfs_up) {
        romfsExit();
        g_romfs_up = false;
    }
    if (g_log_file != nullptr) {
        std::fflush(stdout);
        std::fflush(stderr);
        std::fclose(g_log_file);
        g_log_file = nullptr;
    }
    if (g_socket_up) {
        socketExit();
        g_socket_up = false;
        g_nxlink_up = false;
    }
}

const std::filesystem::path& lod::sw::config_directory() {
    return g_config_directory;
}

const std::filesystem::path& lod::sw::asset_directory() {
    return g_asset_directory;
}

std::filesystem::path lod::sw::rom_path() {
    return g_config_directory / "rom.z64";
}

const std::vector<std::filesystem::path>& lod::sw::rom_search_directories() {
    static const std::vector<std::filesystem::path> directories = {
        g_config_directory,
        "sdmc:/roms/n64",
        "sdmc:/roms",
        "sdmc:/switch",
        "sdmc:/",
    };
    return directories;
}

void* lod::sw::native_window() {
    return nwindowGetDefault();
}

void lod::sw::place_current_thread(const std::string& thread_name) {
    int core = preferred_core_for(thread_name);
    if (core != kCoreFloating && ((g_core_mask >> core) & 1) == 0) {
        core = kCoreFloating;
    }

    const s32 requested_core = core == kCoreFloating ? -1 : core;
    const u32 requested_mask = core == kCoreFloating ? static_cast<u32>(g_core_mask)
                                                     : (1u << core);

    Handle handle = CUR_THREAD_HANDLE;
    if (R_FAILED(svcSetThreadCoreMask(handle, requested_core, requested_mask))) {
        fprintf(stderr, "[THREAD] %s: failed to set core mask 0x%X\n",
                thread_name.c_str(), requested_mask);
        return;
    }

    s32 actual_core = -1;
    u64 actual_mask = 0;
    svcGetThreadCoreMask(&actual_core, &actual_mask, handle);
    fprintf(stderr, "[THREAD] %s -> core %ld (mask 0x%llX)\n",
            thread_name.c_str(), static_cast<long>(actual_core),
            static_cast<unsigned long long>(actual_mask));
}

extern "C" void rt64_switch_place_thread(const char* name) {
    lod::sw::place_current_thread(name);
}

namespace {

std::mutex g_detached_lock;
std::vector<Thread*> g_detached;

bool thread_exited(Thread* thread) {
    s32 index = 0;
    return R_SUCCEEDED(svcWaitSynchronization(&index, &thread->handle, 1, 0));
}

}  // namespace

extern "C" int __syscall_thread_detach(pthread_t thread) {
    std::lock_guard<std::mutex> guard(g_detached_lock);

    auto exited = std::remove_if(g_detached.begin(), g_detached.end(), [](Thread* candidate) {
        if (!thread_exited(candidate)) {
            return false;
        }
        threadClose(candidate);
        free(candidate);
        return true;
    });
    g_detached.erase(exited, g_detached.end());

    g_detached.push_back(reinterpret_cast<Thread*>(thread));
    return 0;
}

bool lod::sw::applet_pump() {
    if (!appletMainLoop()) {
        return false;
    }

    AppletFocusState focus = appletGetFocusState();
    if (focus != g_focus_state) {
        fprintf(stderr, "[SWITCH] focus state changed: %d -> %d\n",
                static_cast<int>(g_focus_state), static_cast<int>(focus));
        g_focus_state = focus;
    }

    AppletOperationMode mode = appletGetOperationMode();
    if (mode != g_operation_mode) {
        fprintf(stderr, "[SWITCH] operation mode changed: %s -> %s\n",
                g_operation_mode == AppletOperationMode_Console ? "docked" : "handheld",
                mode == AppletOperationMode_Console ? "docked" : "handheld");
        g_operation_mode = mode;
    }

    return true;
}

void lod::sw::show_error_message(const char* message) {
    flush_log();

    ErrorApplicationConfig config;
    if (R_FAILED(errorApplicationCreate(&config, message, nullptr))) {
        fprintf(stderr, "[SWITCH] could not create the error dialog for: %s\n", message);
        return;
    }
    errorApplicationShow(&config);
}

void lod::sw::set_crash_reporter(void (*reporter)(void*)) {
    g_crash_reporter = reporter;
}

extern "C" void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    fprintf(stderr, "\n[CRASH] Horizon exception: %s (0x%03X)\n",
            exception_desc_name(ctx->error_desc), ctx->error_desc);
    fprintf(stderr, "  pc=0x%016llX lr=0x%016llX sp=0x%016llX far=0x%016llX esr=0x%08X\n",
            static_cast<unsigned long long>(ctx->pc.x),
            static_cast<unsigned long long>(ctx->lr.x),
            static_cast<unsigned long long>(ctx->sp.x),
            static_cast<unsigned long long>(ctx->far.x),
            ctx->esr);
    for (int i = 0; i + 3 < 29; i += 4) {
        fprintf(stderr, "  x%-2d=0x%016llX x%-2d=0x%016llX x%-2d=0x%016llX x%-2d=0x%016llX\n",
                i, static_cast<unsigned long long>(ctx->cpu_gprs[i].x),
                i + 1, static_cast<unsigned long long>(ctx->cpu_gprs[i + 1].x),
                i + 2, static_cast<unsigned long long>(ctx->cpu_gprs[i + 2].x),
                i + 3, static_cast<unsigned long long>(ctx->cpu_gprs[i + 3].x));
    }
    fprintf(stderr, "  x28=0x%016llX\n",
            static_cast<unsigned long long>(ctx->cpu_gprs[28].x));

    if (g_crash_reporter != nullptr) {
        g_crash_reporter(reinterpret_cast<void*>(ctx->far.x));
    }

    lod::sw::log_fatal("terminating after an unhandled CPU exception");
}

void lod::sw::log_fatal(const char* message) {
    fprintf(stderr, "[FATAL] %s\n", message);
    report_memory("at fault");
    flush_log();
}
