#include <cstdio>
#include <cstring>

#include <switch.h>

#include <SDL2/SDL.h>

#include "librecomp/game.hpp"
#include "ultramodern/ultramodern.hpp"

extern "C" {
u32 __nx_applet_type = AppletType_Application;
}

namespace {

bool g_socket_up = false;
bool g_console_up = false;

void output_init() {
    if (R_SUCCEEDED(socketInitializeDefault())) {
        g_socket_up = true;
        if (nxlinkStdio() >= 0) {
            return;
        }
    }
    consoleInit(nullptr);
    g_console_up = true;
}

void output_flush() {
    if (g_console_up) {
        consoleUpdate(nullptr);
    }
}

void output_exit() {
    if (g_console_up) {
        printf("\nPress + to exit.\n");
        consoleUpdate(nullptr);

        PadState pad;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&pad);
        while (appletMainLoop()) {
            padUpdate(&pad);
            if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
                break;
            }
            consoleUpdate(nullptr);
        }
        consoleExit(nullptr);
    }
    if (g_socket_up) {
        socketExit();
    }
}

void report_romfs() {
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        printf("romfsInit: FAILED (0x%08x)\n", rc);
        return;
    }

    printf("romfsInit: ok\n");

    FILE* f = std::fopen("romfs:/bringup.txt", "r");
    if (f == nullptr) {
        printf("  romfs:/bringup.txt: not found\n");
    } else {
        char line[128] = {};
        if (std::fgets(line, sizeof(line), f) != nullptr) {
            line[strcspn(line, "\r\n")] = '\0';
            printf("  romfs:/bringup.txt: \"%s\"\n", line);
        }
        std::fclose(f);
    }

    romfsExit();
}

void report_memory() {
    u64 total = 0;
    u64 used = 0;
    svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
    svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
    printf("memory: %llu MB total, %llu MB used\n",
           (unsigned long long)(total / (1024 * 1024)),
           (unsigned long long)(used / (1024 * 1024)));
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    output_init();

    printf("=== LodRecomp Switch bring-up ===\n");

    report_romfs();
    report_memory();

    const recomp::Version& version = recomp::get_project_version();
    printf("librecomp: project version %s\n", version.to_string().c_str());

    ultramodern::sleep_milliseconds(1);
    printf("ultramodern: sleep_milliseconds ok\n");

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init: FAILED (%s)\n", SDL_GetError());
    } else {
        printf("SDL_Init(AUDIO|GAMECONTROLLER): ok, %d controller(s)\n",
               SDL_NumJoysticks());
        SDL_Quit();
    }

    printf("=== switch_bringup PASSED ===\n");
    output_flush();

    output_exit();
    return 0;
}
