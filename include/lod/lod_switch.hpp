#pragma once

#ifdef __SWITCH__

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace lod::sw {

void platform_init();
void platform_exit();

const std::filesystem::path& config_directory();
const std::filesystem::path& asset_directory();
std::filesystem::path rom_path();
const std::vector<std::filesystem::path>& rom_search_directories();

void* native_window();

void place_current_thread(const std::string& thread_name);

bool applet_pump();

uint64_t committed_rdram_bytes();

void log_fatal(const char* message);

void show_error_message(const char* message);

void set_crash_reporter(void (*reporter)(void* fault_addr));

}  // namespace lod::sw

#endif  // __SWITCH__
