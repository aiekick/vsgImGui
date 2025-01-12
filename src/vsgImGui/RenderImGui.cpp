/* <editor-fold desc="MIT License">

Copyright(c) 2021 Don Burns, Roland Hill and Robert Osfield.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsgImGui/RenderImGui.h>
#include <vsgImGui/implot.h>
#include <vsgImGui/ImGuizmo.h>
#include <vsgImGui/imnodes.h>

#include "../imgui/backends/imgui_impl_vulkan.h"

#include <vsg/io/Logger.h>
#include <vsg/vk/State.h>
#include <vsg/vk/SubmitCommands.h>

#include <foaw.h>
#include <Roboto_Medium.h>

#ifdef PLATFORM_WIN32
#include <Windows.h>
#endif

using namespace vsgImGui;

namespace vsgImGui
{
    void check_vk_result(VkResult err)
    {
        if (err == 0) return;

        vsg::error("[vulkan] Error: VkResult = ", err);
    }

    class ImGuiNode : public vsg::Inherit<vsg::Node, ImGuiNode>
    {
    public:
        ImGuiNode(RenderImGui::LegacyFunction in_func) :
            func(in_func) {}

        RenderImGui::LegacyFunction func;

        void accept(vsg::RecordTraversal&) const override
        {
            func();
        }
    };

} // namespace vsgImGui

RenderImGui::RenderImGui(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments)
{
    _init(window, useClearAttachments);
    _uploadFonts();
}

RenderImGui::RenderImGui(vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
                         vsg::ref_ptr<vsg::RenderPass> renderPass,
                         uint32_t minImageCount, uint32_t imageCount,
                         VkExtent2D imageSize, bool useClearAttachments)
{
    _init(device, queueFamily, renderPass, minImageCount, imageCount, imageSize, useClearAttachments);
    _uploadFonts();
}

RenderImGui::~RenderImGui()
{
    ImGui_ImplVulkan_Shutdown();
    ImNodes::DestroyContext();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void RenderImGui::add(const LegacyFunction& legacyFunc)
{
    addChild(ImGuiNode::create(legacyFunc));
}

void RenderImGui::_init(const vsg::ref_ptr<vsg::Window>& window, bool useClearAttachments)
{
    auto device = window->getOrCreateDevice();
    auto physicalDevice = device->getPhysicalDevice();

    uint32_t queueFamily = 0;
    std::tie(queueFamily, std::ignore) = physicalDevice->getQueueFamily(window->traits()->queueFlags, window->getSurface());
    auto queue = device->getQueue(queueFamily);

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(*physicalDevice,
                                              *(window->getSurface()),
                                              &capabilities);
    uint32_t imageCount = 3;
    imageCount =
        std::max(imageCount,
                 capabilities.minImageCount); // Vulkan spec requires
                                              // minImageCount to be 1 or greater
    if (capabilities.maxImageCount > 0)
        imageCount = std::min(
            imageCount,
            capabilities.maxImageCount); // Vulkan spec specifies 0 as being
                                         // unlimited number of images

    _init(device, queueFamily, window->getOrCreateRenderPass(), capabilities.minImageCount, imageCount, window->extent2D(), useClearAttachments);
}

void RenderImGui::_init(
    vsg::ref_ptr<vsg::Device> device, uint32_t queueFamily,
    vsg::ref_ptr<vsg::RenderPass> renderPass,
    uint32_t minImageCount, uint32_t imageCount,
    VkExtent2D imageSize, bool useClearAttachments)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImNodes::CreateContext();

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    for (auto& attachment : renderPass->attachments)
    {
        if (attachment.samples > samples) samples = attachment.samples;
    }

    // ImGui may change this later, but ensure the display
    // size is set to something, to prevent assertions
    // in ImGui::newFrame.
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = (float)imageSize.width;
    io.DisplaySize.y = (float)imageSize.height;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable ViewPort
    io.FontAllowUserScaling = true;              // activate zoom feature with ctrl + mousewheel
    io.ConfigWindowsMoveFromTitleBarOnly = false; // can move windows only with titlebar
#ifdef USE_DECORATIONS_FOR_RESIZE_CHILD_WINDOWS
    io.ConfigViewportsNoDecoration = false; // toujours mettre une frame au fenetre enfant
#endif

    _device = device;
    _queueFamily = queueFamily;
    _queue = _device->getQueue(_queueFamily);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = *(_device->getInstance());
    init_info.PhysicalDevice = *(_device->getPhysicalDevice());
    init_info.Device = *(_device);
    init_info.QueueFamily = _queueFamily;
    init_info.Queue = *(_queue); // ImGui doesn't use the queue so we shouldn't need to assign it, but it has an IM_ASSERT requiring it during debug build.
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.MSAASamples = samples;

    // Create Descriptor Pool
    vsg::DescriptorPoolSizes pool_sizes = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    uint32_t maxSets = 1000U * static_cast<uint32_t>(pool_sizes.size());
    _descriptorPool = vsg::DescriptorPool::create(_device, maxSets, pool_sizes);

    init_info.DescriptorPool = *(_descriptorPool);
    init_info.Allocator = nullptr;
    init_info.MinImageCount = std::max(minImageCount, 2u); // ImGui's Vulkan backend has an assert that requies MinImageCount to be 2 or more.
    init_info.ImageCount = imageCount;
    init_info.CheckVkResultFn = check_vk_result;

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    }
    
    ImGui_ImplVulkan_Init(&init_info, *renderPass);

    if (useClearAttachments)
    {
        // clear the depth buffer before view2 gets rendered
        VkClearValue clearValue{};
        clearValue.depthStencil = {1.0f, 0};
        VkClearAttachment attachment{VK_IMAGE_ASPECT_DEPTH_BIT, 1, clearValue};
        VkClearRect rect{VkRect2D{VkOffset2D{0, 0}, VkExtent2D{imageSize.width, imageSize.height}}, 0, 1};
        _clearAttachments = vsg::ClearAttachments::create(vsg::ClearAttachments::Attachments{attachment}, vsg::ClearAttachments::Rects{rect});
    }

    // load memory font file
    {
        static ImFontConfig icons_config;
        icons_config.OversampleH = 2;
        auto fontPtr = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_RM, 60.0f, &icons_config);
        if (fontPtr)
        {
            fontPtr->Scale = 0.25f;
        }
    }

    {
        static ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        static ImWchar icons_ranges_FOAW[] = {ICON_MIN_FOAW, ICON_MAX_FOAW, 0};
        auto fontPtr = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_FOAW, 50.0f, &icons_config, icons_ranges_FOAW);
        if (fontPtr)
        {
            fontPtr->Scale = 0.30f;
        }
    }

}

void RenderImGui::_uploadFonts()
{
    auto commandPool = vsg::CommandPool::create(_device, _queueFamily, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    auto fence = vsg::Fence::create(_device);

    uint64_t timeout = 1000000000;
    vsg::submitCommandsToQueue(commandPool, fence, timeout, _queue, [&](vsg::CommandBuffer& commandBuffer) {
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
    });

    VkResult result = fence->status();
    while (result == VK_NOT_READY)
    {
        result = fence->wait(timeout);
    }

    if (result != VK_SUCCESS)
    {
        vsg::error("RenderImGui::_uploadFonts(), fence->state() = ", result);
    }

    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

static bool ImGui_UpdateMouseCursor()
{
#ifdef PLATFORM_WIN32
    // code from https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_win32.cpp
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return false;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        ::SetCursor(nullptr);
    }
    else
    {
        // Show OS mouse cursor
        LPTSTR win32_cursor = IDC_ARROW;
        switch (imgui_cursor)
        {
        case ImGuiMouseCursor_Arrow: win32_cursor = IDC_ARROW; break;
        case ImGuiMouseCursor_TextInput: win32_cursor = IDC_IBEAM; break;
        case ImGuiMouseCursor_ResizeAll: win32_cursor = IDC_SIZEALL; break;
        case ImGuiMouseCursor_ResizeEW: win32_cursor = IDC_SIZEWE; break;
        case ImGuiMouseCursor_ResizeNS: win32_cursor = IDC_SIZENS; break;
        case ImGuiMouseCursor_ResizeNESW: win32_cursor = IDC_SIZENESW; break;
        case ImGuiMouseCursor_ResizeNWSE: win32_cursor = IDC_SIZENWSE; break;
        case ImGuiMouseCursor_Hand: win32_cursor = IDC_HAND; break;
        case ImGuiMouseCursor_NotAllowed: win32_cursor = IDC_NO; break;
        }
        ::SetCursor(::LoadCursor(nullptr, win32_cursor));
    }
#endif // PLATFORM_WIN32
    return true;
}

void RenderImGui::accept(vsg::RecordTraversal& rt) const
{
    auto& commandBuffer = *(rt.getState()->_commandBuffer);
    if (_device.get() != commandBuffer.getDevice()) return;

    // record all the ImGui commands to ImDrawData container
    ImGui_ImplVulkan_NewFrame();
    ImGui_UpdateMouseCursor();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    // traverse children
    traverse(rt);

    ImGui::EndFrame();
    ImGui::Render();

    // if ImDrawData has been recorded then we need to clear to frame buffer and do the final record to Vulkan command buffer.
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data && draw_data->CmdListsCount > 0)
    {
        if (_clearAttachments) _clearAttachments->record(commandBuffer);

        if (draw_data)
            ImGui_ImplVulkan_RenderDrawData(draw_data, &(*commandBuffer));
    }
}
