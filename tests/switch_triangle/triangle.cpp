#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include <sys/stat.h>

#include <switch.h>

#include "plume_render_interface.h"

#include "shaders/triangleVert.hlsl.spirv.h"
#include "shaders/triangleFrag.hlsl.spirv.h"

extern "C" {
u32 __nx_applet_type = AppletType_Application;
}

namespace plume {
    extern std::unique_ptr<RenderInterface> CreateVulkanInterface();
}

using namespace plume;

namespace {

constexpr uint32_t BufferCount = 3;
constexpr RenderFormat SwapChainFormat = RenderFormat::R8G8B8A8_UNORM;
constexpr int FrameCount = 600;

FILE *g_log = nullptr;
bool g_socket_up = false;

void log_open() {
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/lodrecomp", 0777);

    bool nxlink_up = false;
    if (R_SUCCEEDED(socketInitializeDefault())) {
        g_socket_up = true;
        nxlink_up = nxlinkStdio() >= 0;
    }

    if (!nxlink_up) {
        freopen("sdmc:/switch/lodrecomp/triangle_stderr.log", "w", stderr);
    }

    g_log = fopen("sdmc:/switch/lodrecomp/triangle.log", "w");
}

void log_close() {
    if (g_log != nullptr) {
        fclose(g_log);
        g_log = nullptr;
    }
    if (g_socket_up) {
        socketExit();
    }
}

void log_line(const char *format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    printf("%s\n", line);
    if (g_log != nullptr) {
        fprintf(g_log, "%s\n", line);
        fflush(g_log);
    }
}

void log_capabilities(RenderDevice *device) {
    const RenderDeviceDescription &description = device->getDescription();
    const RenderDeviceCapabilities &caps = device->getCapabilities();

    log_line("device: %s (type %d, vendor 0x%X, driver 0x%llX)", description.name.c_str(),
        int(description.type), uint32_t(description.vendor), (unsigned long long)(description.driverVersion));
    log_line("caps: descriptorIndexing=%d scalarBlockLayout=%d bufferDeviceAddress=%d",
        caps.descriptorIndexing, caps.scalarBlockLayout, caps.bufferDeviceAddress);
    log_line("caps: sampleLocations=%d resolveRegion=%d resolveModes=%d triangleFan=%d dynamicDepthBias=%d",
        caps.sampleLocations, caps.resolveRegion, caps.resolveModes, caps.triangleFan, caps.dynamicDepthBias);
    log_line("caps: presentWait=%d displayTiming=%d samplerMirrorClampToEdge=%d geometryShader=%d",
        caps.presentWait, caps.displayTiming, caps.samplerMirrorClampToEdge, caps.geometryShader);
    log_line("caps: uma=%d gpuUploadHeap=%d queryPools=%d preferHDR=%d maxTextureSize=%llu",
        caps.uma, caps.gpuUploadHeap, caps.queryPools, caps.preferHDR,
        (unsigned long long)(caps.maxTextureSize));
}

struct Triangle {
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderCommandQueue> commandQueue;
    std::unique_ptr<RenderCommandList> commandList;
    std::unique_ptr<RenderCommandFence> fence;
    std::unique_ptr<RenderSwapChain> swapChain;
    std::unique_ptr<RenderCommandSemaphore> acquireSemaphore;
    std::vector<std::unique_ptr<RenderCommandSemaphore>> releaseSemaphores;
    std::vector<std::unique_ptr<RenderFramebuffer>> framebuffers;
    std::unique_ptr<RenderPipeline> pipeline;
    std::unique_ptr<RenderPipelineLayout> pipelineLayout;
    std::unique_ptr<RenderBuffer> vertexBuffer;
    RenderVertexBufferView vertexBufferView;
    RenderInputSlot inputSlot;

    void createFramebuffers() {
        framebuffers.clear();

        for (uint32_t i = 0; i < swapChain->getTextureCount(); i++) {
            const RenderTexture *colorAttachment = swapChain->getTexture(i);
            RenderFramebufferDesc framebufferDesc;
            framebufferDesc.colorAttachments = &colorAttachment;
            framebufferDesc.colorAttachmentsCount = 1;
            framebuffers.emplace_back(device->createFramebuffer(framebufferDesc));
        }
    }

    void createPipeline() {
        RenderPipelineLayoutDesc layoutDesc;
        layoutDesc.allowInputLayout = true;
        pipelineLayout = device->createPipelineLayout(layoutDesc);

        std::unique_ptr<RenderShader> vertexShader = device->createShader(triangleVertBlobSPIRV,
            sizeof(triangleVertBlobSPIRV), "VSMain", RenderShaderFormat::SPIRV);
        std::unique_ptr<RenderShader> pixelShader = device->createShader(triangleFragBlobSPIRV,
            sizeof(triangleFragBlobSPIRV), "PSMain", RenderShaderFormat::SPIRV);

        inputSlot = RenderInputSlot(0, sizeof(float) * 7);
        const std::vector<RenderInputElement> inputElements = {
            RenderInputElement("POSITION", 0, 0, RenderFormat::R32G32B32_FLOAT, 0, 0),
            RenderInputElement("COLOR", 0, 1, RenderFormat::R32G32B32A32_FLOAT, 0, sizeof(float) * 3)
        };

        RenderGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.inputSlots = &inputSlot;
        pipelineDesc.inputSlotsCount = 1;
        pipelineDesc.inputElements = inputElements.data();
        pipelineDesc.inputElementsCount = uint32_t(inputElements.size());
        pipelineDesc.pipelineLayout = pipelineLayout.get();
        pipelineDesc.vertexShader = vertexShader.get();
        pipelineDesc.pixelShader = pixelShader.get();
        pipelineDesc.renderTargetFormat[0] = SwapChainFormat;
        pipelineDesc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        pipelineDesc.renderTargetCount = 1;
        pipelineDesc.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
        pipeline = device->createGraphicsPipeline(pipelineDesc);
    }

    void createVertexBuffer() {
        const float vertices[] = {
             0.0f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f, 1.0f
        };

        vertexBuffer = device->createBuffer(RenderBufferDesc::VertexBuffer(sizeof(vertices), RenderHeapType::UPLOAD));
        void *bufferData = vertexBuffer->map();
        memcpy(bufferData, vertices, sizeof(vertices));
        vertexBuffer->unmap();
        vertexBufferView = RenderVertexBufferView(vertexBuffer.get(), sizeof(vertices));
    }

    bool create(RenderInterface *renderInterface) {
        device = renderInterface->createDevice();
        if (device == nullptr) {
            log_line("FAIL: createDevice");
            return false;
        }

        log_capabilities(device.get());

        commandQueue = device->createCommandQueue(RenderCommandListType::DIRECT);
        fence = device->createCommandFence();
        swapChain = commandQueue->createSwapChain(RenderSwapChainDesc(nwindowGetDefault(), SwapChainFormat, BufferCount));
        if (!swapChain->resize()) {
            log_line("FAIL: swap chain resize");
            return false;
        }

        log_line("swap chain: %ux%u, %u textures, vsync=%d", swapChain->getWidth(), swapChain->getHeight(),
            swapChain->getTextureCount(), swapChain->isVsyncEnabled());

        commandList = commandQueue->createCommandList();
        acquireSemaphore = device->createCommandSemaphore();
        createFramebuffers();
        createPipeline();
        createVertexBuffer();
        return true;
    }

    bool render() {
        if (swapChain->needsResize()) {
            framebuffers.clear();
            if (!swapChain->resize()) {
                return true;
            }

            createFramebuffers();
            log_line("resized to %ux%u", swapChain->getWidth(), swapChain->getHeight());
        }

        uint32_t textureIndex = 0;
        if (!swapChain->acquireTexture(acquireSemaphore.get(), &textureIndex)) {
            log_line("FAIL: acquireTexture");
            return false;
        }

        RenderTexture *swapChainTexture = swapChain->getTexture(textureIndex);
        const uint32_t width = swapChain->getWidth();
        const uint32_t height = swapChain->getHeight();

        commandList->begin();
        commandList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::COLOR_WRITE));
        commandList->setFramebuffer(framebuffers[textureIndex].get());
        commandList->setViewports(RenderViewport(0.0f, 0.0f, float(width), float(height)));
        commandList->setScissors(RenderRect(0, 0, width, height));
        commandList->clearColor(0, RenderColor(0.0f, 0.0f, 0.2f, 1.0f));
        commandList->setGraphicsPipelineLayout(pipelineLayout.get());
        commandList->setPipeline(pipeline.get());
        commandList->setVertexBuffers(0, &vertexBufferView, 1, &inputSlot);
        commandList->drawInstanced(3, 1, 0, 0);
        commandList->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(swapChainTexture, RenderTextureLayout::PRESENT));
        commandList->end();

        while (releaseSemaphores.size() < swapChain->getTextureCount()) {
            releaseSemaphores.emplace_back(device->createCommandSemaphore());
        }

        const RenderCommandList *submitList = commandList.get();
        RenderCommandSemaphore *waitSemaphore = acquireSemaphore.get();
        RenderCommandSemaphore *signalSemaphore = releaseSemaphores[textureIndex].get();
        commandQueue->executeCommandLists(&submitList, 1, &waitSemaphore, 1, &signalSemaphore, 1, fence.get());

        const bool presented = swapChain->present(textureIndex, &signalSemaphore, 1);
        commandQueue->waitForCommandFence(fence.get());
        if (!presented) {
            log_line("FAIL: present");
            return false;
        }

        return true;
    }
};

}  // namespace

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    setenv("NVK_DEBUG", "errors", 1);

    log_open();
    log_line("=== switch_triangle ===");

    std::unique_ptr<RenderInterface> renderInterface = CreateVulkanInterface();
    if (renderInterface == nullptr) {
        log_line("FAIL: CreateVulkanInterface");
        log_close();
        return 1;
    }

    log_line("instance: ok, %zu device(s)", renderInterface->getDeviceNames().size());

    bool passed = false;
    {
        Triangle triangle;
        if (triangle.create(renderInterface.get())) {
            PadState pad;
            padConfigureInput(1, HidNpadStyleSet_NpadStandard);
            padInitializeDefault(&pad);

            AppletOperationMode lastMode = appletGetOperationMode();
            log_line("operation mode: %s", (lastMode == AppletOperationMode_Console) ? "docked" : "handheld");

            int frame = 0;
            passed = true;
            for (; frame < FrameCount && appletMainLoop(); frame++) {
                padUpdate(&pad);
                if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
                    break;
                }

                const AppletOperationMode mode = appletGetOperationMode();
                if (mode != lastMode) {
                    log_line("operation mode changed to %s at frame %d",
                        (mode == AppletOperationMode_Console) ? "docked" : "handheld", frame);
                    lastMode = mode;
                }

                if (!triangle.render()) {
                    passed = false;
                    break;
                }
            }

            log_line("presented %d frame(s)", frame);
        }
    }

    log_line(passed ? "=== switch_triangle PASSED ===" : "=== switch_triangle FAILED ===");
    log_close();
    return passed ? 0 : 1;
}
