#include "pch.h"
#include "BackBuffer.h"
#include "CameraController.h"
#include "CameraInput.h"
#include "CommandQueue.h"
#include "DearImGui.h"
#include "DebugLayer.h"
#include "DepthStencilBuffer.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightHeap.h"
#include "LightLinkedList.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "ShaderInterop.h"
#include "SwapChain.h"
#include "Utilities.h"
#include "Window.h"

#include <format>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <pix3.h>
#include <stb_image.h>
#include <tiny_gltf.h>

struct DebugSettings
{
    bool ShowLightBoundaries = false;
    bool ShowLightBufferAverage = false;
    bool AnimateLights = true;
};

static Scene LoadGltfScene(ResourceManager& resources, const std::string& filePath)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string gltfError;
    std::string gltfWarning;
    printf("Parsing glTF file...\n");
    bool success = loader.LoadASCIIFromFile(&model, &gltfError, &gltfWarning, filePath);

    if (!gltfError.empty())
        printf("glTF parsing error: %s\n", gltfError.c_str());

    if (!gltfWarning.empty())
        printf("glTF parsing warning: %s\n", gltfWarning.c_str());

    if (!success)
    {
        printf("glTF parsing failed.\n");
        exit(1);
    }

    printf("Loading glTF scene...\n");
    return Scene(resources, model);
}

static Texture LoadHdr(ResourceManager& resources, std::string filePath)
{
    int width;
    int height;
    int channels;
    // Forcing 4 channels because R32G32B32 cannot be accessed via UAV which is required for mipmap chain generation.
    float* data = stbi_loadf(filePath.c_str(), &width, &height, &channels, 4);
    channels = 4;
    Texture texture(resources, std::format(L"{}", filePath), std::span(data, width * height * channels), uint2((uint32_t)width, (uint32_t)height), channels);
    stbi_image_free(data);
    return texture;
}

static int MainImpl()
{
    // Set working directory to app directory so we can easily get at our assets
    SetWorkingDirectoryToAppDirectory();

    //-----------------------------------------------------------------------------------------------------------------
    // Initialize graphics
    //-----------------------------------------------------------------------------------------------------------------
    GraphicsCore graphics;

    PIXBeginEvent(0, "Initialization");

    std::wstring windowTitle = L"ThreeL";
    windowTitle += DebugLayer::GetExtraWindowTitleInfo();
    Window window(windowTitle.c_str(), 1280, 720);

    WndProcHandle wndProcHandle = window.AddWndProc([&](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> std::optional<LRESULT>
        {
            switch (message)
            {
                // Prevent the user from making the window super tiny
                // (This mainly saves us from having to do something sensible in response to a buffer or half-buffer being 0-sized.)
                case WM_GETMINMAXINFO:
                {
                    MINMAXINFO* info = (MINMAXINFO*)lParam;
                    info->ptMinTrackSize = { 200, 200 };
                    return 0;
                }
                default:
                    return { };
            }
        });

    // Ensure PIX targets our main window and disable the HUD since it overlaps with our menu bar
    PIXSetTargetWindow(window.Hwnd());
    PIXSetHUDOptions(PIX_HUD_SHOW_ON_NO_WINDOWS);

    SwapChain swapChain(graphics, window);

    ResourceManager resources(graphics);

    //-----------------------------------------------------------------------------------------------------------------
    // Load environment HDR
    //-----------------------------------------------------------------------------------------------------------------
    Texture environmentHdr = LoadHdr(resources, "Assets/ninomaru_teien_1k.hdr");
    Assert(environmentHdr.BindlessIndex() == 0); // We currently hard-code the environment HDR to be bindless slot 0 in the shader

    //-----------------------------------------------------------------------------------------------------------------
    // Load glTF model
    //-----------------------------------------------------------------------------------------------------------------
    Scene scene = LoadGltfScene(resources,
        // Note: If you change this be sure to change the default camera location below
        //"Assets/MetalRoughSpheres/MetalRoughSpheres.gltf"
        "Assets/Sponza/Sponza.gltf"
    );
    printf("Done.\n");

    //-----------------------------------------------------------------------------------------------------------------
    // Allocate screen-dependent resources
    //-----------------------------------------------------------------------------------------------------------------
    uint2 screenSize = swapChain.Size();
    float2 screenSizeF = (float2)screenSize;
    DepthStencilBuffer depthBuffer(graphics, L"Depth Buffer", screenSize, DEPTH_BUFFER_FORMAT);

    std::vector<DepthStencilBuffer> downsampledDepthBuffers;
    for (int i = 2; i <= 8; i *= 2)
    {
        std::wstring name = std::format(L"Depth Buffer (1/{})", i);
        downsampledDepthBuffers.emplace_back(graphics, name, screenSize / i, DEPTH_BUFFER_FORMAT);
    }

    //-----------------------------------------------------------------------------------------------------------------
    // Allocate lighting resources
    //-----------------------------------------------------------------------------------------------------------------
    LightHeap lightHeap(graphics);
    std::vector<ShaderInterop::LightInfo> lights;
    lights.reserve(LightHeap::MAX_LIGHTS);

    LightLinkedList lightLinkedList(resources, screenSize);
    uint32_t lightLinkedListShift = 3; // 0 = 1/1, 1 = 1/2, 2 = 1/4, 3 = 1/8
    uint32_t lightLinkLimit = LightLinkedList::MAX_LIGHT_LINKS;

    lights.push_back
    ({
        .Position = float3::Zero,
        .Range = 1.f,
        .Color = float3::One,
        .Intensity = 1.f,
    });
    float light0Hover = 0.f;

    lights.push_back
    ({
        .Position = float3(-4.35f, 1.f, 0.f),
        .Range = 2.5f,
        .Color = float3::UnitX,
        .Intensity = 2.f,
    });
    float3 light1Center = lights[1].Position;
    float3 light1Offset = float3::UnitX;

    //-----------------------------------------------------------------------------------------------------------------
    // Misc initialization
    //-----------------------------------------------------------------------------------------------------------------
    DebugSettings debugSettings = DebugSettings();
    PresentMode presentMode = PresentMode::Vsync;
    DearImGui dearImGui(graphics, window);

    // Time keeping
    LARGE_INTEGER performanceFrequency;
    double performanceFrequencyInverse;
    AssertWinError(QueryPerformanceFrequency(&performanceFrequency));
    performanceFrequencyInverse = 1.0 / (double)performanceFrequency.QuadPart;

    LARGE_INTEGER lastTimestamp;
    AssertWinError(QueryPerformanceCounter(&lastTimestamp));

    float frameTimes[200] = {};
    uint64_t frameNumber = 0;

    // Camera stuff
    CameraInput cameraInput(window);
    CameraController camera;
    float cameraFovDegrees = 45.f;

    // Start the camera in a sensible location for Sponza
    camera.WarpTo(float3(4.35f, 1.f, 0.f), 0.f, Math::HalfPi);

    // Per-frame constant buffer
    // This is a pretty heavyweight abstraction for a constant buffer, ideally we should have a little bump allocator for this sort of thing
    FrequentlyUpdatedResource perFrameCbResource(graphics, DescribeBufferResource(sizeof(ShaderInterop::PerFrameCb)), L"Per-frame constant buffer");

    // Initiate final resource uploads and wait for them to complete
    resources.Finish();
    graphics.UploadQueue().Flush();

    window.Show();
    PIXEndEvent();

    OutputDebugStringW(L"Initialization complete.\n");

    //-----------------------------------------------------------------------------------------------------------------
    // Main loop
    //-----------------------------------------------------------------------------------------------------------------
    auto BindPbrRootSignature = [&](GraphicsContext& context, D3D12_GPU_VIRTUAL_ADDRESS perFrame)
        {
            context->SetGraphicsRootSignature(resources.PbrRootSignature);
            context->SetGraphicsRootConstantBufferView(ShaderInterop::Pbr::RpPerFrameCb, perFrame);
            context->SetGraphicsRootShaderResourceView(ShaderInterop::Pbr::RpMaterialHeap, resources.PbrMaterials.BufferGpuAddress());
            
            //TODO: These are bound even during the depth pre-pass even though they aren't valid at that point. Maybe it should have its own separate root signature?
            context->SetGraphicsRootShaderResourceView(ShaderInterop::Pbr::RpLightHeap, lightHeap.BufferGpuAddress());
            context->SetGraphicsRootShaderResourceView(ShaderInterop::Pbr::RpLightLinksHeap, lightLinkedList.LightLinksHeapGpuAddress());
            context->SetGraphicsRootShaderResourceView(ShaderInterop::Pbr::RpFirstLightLinkBuffer, lightLinkedList.FirstLightLinkBufferGpuAddress());

            context->SetGraphicsRootDescriptorTable(ShaderInterop::Pbr::RpSamplerHeap, graphics.SamplerHeap().GpuHeap()->GetGPUDescriptorHandleForHeapStart());
            context->SetGraphicsRootDescriptorTable(ShaderInterop::Pbr::RpBindlessHeap, graphics.ResourceDescriptorManager().GpuHeap()->GetGPUDescriptorHandleForHeapStart());
        };

    while (Window::ProcessMessages())
    {
        PIXScopedEvent(0, "Frame %lld", frameNumber);

        LARGE_INTEGER timestamp;
        QueryPerformanceCounter(&timestamp);
        float deltaTime = (float)(double(timestamp.QuadPart - lastTimestamp.QuadPart) * performanceFrequencyInverse); // seconds
        float deltaTimeMs = deltaTime * 1000.f;
        frameTimes[frameNumber % std::size(frameTimes)] = deltaTimeMs;

        dearImGui.NewFrame();

        cameraInput.StartFrame();
        camera.ApplyMovement(cameraInput.MoveVector() * deltaTime * 3.f, cameraInput.LookVector() * deltaTime);

        // Animate lights
        if (debugSettings.AnimateLights)
        {
            lights[0].Position.y = 0.1f + (1.f + std::sin(light0Hover += deltaTime * 2.f)) * 0.25f;
            light1Offset = Quaternion(float3::UnitY, -deltaTime) * light1Offset;
            lights[1].Position = light1Center + light1Offset;
        }

        //-------------------------------------------------------------------------------------------------------------
        // Resize screen-dependent resources
        //-------------------------------------------------------------------------------------------------------------
        if ((screenSize != swapChain.Size()).Any())
        {
            // Flush the GPU to ensure there's no outstanding usage of the resources
            graphics.WaitForGpuIdle();
            screenSize = swapChain.Size();
            screenSizeF = (float2)screenSize;

            depthBuffer.Resize(screenSize);

            int divisor = 2;
            for (DepthStencilBuffer& depthBuffer : downsampledDepthBuffers)
            {
                depthBuffer.Resize(screenSize / divisor);
                divisor *= 2;
            }

            lightLinkedList.Resize(screenSize);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Frame setup
        //-------------------------------------------------------------------------------------------------------------
        GpuSyncPoint lightUpdateSyncPoint;
        GraphicsContext context(graphics.GraphicsQueue(), resources.PbrRootSignature, resources.PbrBlendOffSingleSided);
        {
            PIXScopedEvent(&context, 0, "Frame #%lld setup", frameNumber);
            context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_RENDER_TARGET);
            context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            context.Clear(swapChain, 0.2f, 0.2f, 0.2f, 1.f);
            context.Clear(depthBuffer);

            lightUpdateSyncPoint = lightHeap.Update(lights);
        }

        float4x4 perspectiveTransform = float4x4::MakePerspectiveTransformReverseZ(Math::Deg2Rad(cameraFovDegrees) , screenSizeF.x / screenSizeF.y, 0.0001f);
        ShaderInterop::PerFrameCb perFrame =
        {
            .ViewProjectionTransform = camera.ViewTransform() * perspectiveTransform,
            .EyePosition = camera.Position(),
            .LightCount = std::min((uint32_t)lights.size(), LightHeap::MAX_LIGHTS),
            .LightLinkedListBufferWidth = screenSize.x >> lightLinkedListShift,
            .LightLinkedListBufferShift = lightLinkedListShift,
        };
        perFrame.ViewProjectionTransformInverse = perFrame.ViewProjectionTransform.Inverted();

        GpuSyncPoint perFrameCbSyncPoint = perFrameCbResource.Update(perFrame);
        D3D12_GPU_VIRTUAL_ADDRESS perFrameCbAddress = perFrameCbResource.Current()->GetGPUVirtualAddress();
        graphics.GraphicsQueue().AwaitSyncPoint(perFrameCbSyncPoint); // (Part of why FrequentlyUpdatedResource isn't the ideal abstraction here)

        //-------------------------------------------------------------------------------------------------------------
        // Depth pre-pass
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 1, "Depth pre-pass");
            context.SetRenderTarget(depthBuffer.View());
            context.SetFullViewportScissor(screenSize);
            BindPbrRootSignature(context, perFrameCbAddress);

            for (const SceneNode& node : scene)
            {
                // Skip unrenderable nodes (IE: nodes without meshes)
                if (!node.IsValid()) { continue; }

                ShaderInterop::PerNodeCb perNode =
                {
                    .Transform = node.WorldTransform(),
                    .NormalTransform = node.NormalTransform(),
                };

                PIXScopedEvent(&context, 1, "Node '%s'", node.Name().c_str());
                for (const MeshPrimitive& primitive : node)
                {
                    if (primitive.Material().IsTransparent())
                    {
                        continue;
                    }

                    PbrMaterial material = primitive.Material();
                    perNode.MaterialId = material.MaterialId();
                    perNode.TangentsIndex = primitive.TangentsBufferIndex();
                    perNode.ColorsIndex = primitive.ColorsBufferIndex();
                    context->SetGraphicsRoot32BitConstants(ShaderInterop::Pbr::RpPerNodeCb, sizeof(perNode) / sizeof(UINT), &perNode, 0);

                    context->SetPipelineState(material.IsDoubleSided() ? resources.DepthOnlyDoubleSided : resources.DepthOnlySingleSided);
                    context->IASetVertexBuffers(MeshInputSlot::Position, 1, &primitive.Positions());
                    context->IASetVertexBuffers(MeshInputSlot::Uv0, 1, &primitive.Uvs());

                    if (primitive.IsIndexed())
                    {
                        context->IASetIndexBuffer(&primitive.Indices());
                        context.DrawIndexedInstanced(primitive.VertexOrIndexCount(), 1);
                    }
                    else
                    {
                        context.DrawInstanced(primitive.VertexOrIndexCount(), 1);
                    }
                }
            }
        }

        //-------------------------------------------------------------------------------------------------------------
        // Downsample depth buffer
        //-------------------------------------------------------------------------------------------------------------
        context.Flush();
        {
            PIXScopedEvent(&context, 1, "Downsample depth");

            DepthStencilBuffer* previousBuffer = &depthBuffer;
            context->SetGraphicsRootSignature(resources.DepthDownsampleRootSignature);
            context->SetPipelineState(resources.DepthDownsample);

            for (DepthStencilBuffer& depthBuffer : downsampledDepthBuffers)
            {
                context->SetGraphicsRootDescriptorTable(ShaderInterop::DepthDownsample::RpInputDepthBuffer, previousBuffer->DepthShaderResourceView().ResidentHandle());
                context.SetRenderTarget(depthBuffer.View());
                context.SetFullViewportScissor(depthBuffer.Size());

                context.TransitionResource(*previousBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
                context.DrawInstanced(3, 1);

                context.TransitionResource(*previousBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
                previousBuffer = &depthBuffer;
            }

            context.TransitionResource(*previousBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Fill light linked list
        //-------------------------------------------------------------------------------------------------------------
        context.Flush();
        {
            PIXScopedEvent(&context, 2, "Fill light linked list");
            graphics.GraphicsQueue().AwaitSyncPoint(lightUpdateSyncPoint);
            DepthStencilBuffer& lightLinkedListDepthBuffer = lightLinkedListShift == 0 ? depthBuffer : downsampledDepthBuffers[lightLinkedListShift - 1];
            lightLinkedList.FillLights
            (
                context,
                lightHeap,
                (uint32_t)lights.size(),
                lightLinkLimit,
                perFrameCbAddress,
                perFrame.LightLinkedListBufferShift,
                lightLinkedListDepthBuffer,
                screenSize,
                perspectiveTransform
            );
        }

        //-------------------------------------------------------------------------------------------------------------
        // Opaque pass
        //-------------------------------------------------------------------------------------------------------------
        context.Flush();
        {
            PIXScopedEvent(&context, 2, "Opaque pass");
            context.SetRenderTarget(swapChain, depthBuffer.ReadOnlyView());
            context.SetFullViewportScissor(screenSize);
            BindPbrRootSignature(context, perFrameCbAddress);

            for (const SceneNode& node : scene)
            {
                // Skip unrenderable nodes (IE: nodes without meshes)
                if (!node.IsValid()) { continue; }

                ShaderInterop::PerNodeCb perNode =
                {
                    .Transform = node.WorldTransform(),
                    .NormalTransform = node.NormalTransform(),
                };

                PIXScopedEvent(&context, 2, "Node '%s'", node.Name().c_str());
                for (const MeshPrimitive& primitive : node)
                {
                    if (primitive.Material().IsTransparent())
                    {
                        continue;
                    }

                    PbrMaterial material = primitive.Material();
                    perNode.MaterialId = material.MaterialId();
                    perNode.TangentsIndex = primitive.TangentsBufferIndex();
                    perNode.ColorsIndex = primitive.ColorsBufferIndex();
                    context->SetGraphicsRoot32BitConstants(ShaderInterop::Pbr::RpPerNodeCb, sizeof(perNode) / sizeof(UINT), &perNode, 0);

                    if (debugSettings.ShowLightBoundaries)
                    { context->SetPipelineState(material.IsDoubleSided() ? resources.PbrLightDebugDoubleSided : resources.PbrLightDebugSingleSided); }
                    else
                    { context->SetPipelineState(material.PipelineStateObject()); }
                    context->IASetVertexBuffers(MeshInputSlot::Position, 1, &primitive.Positions());
                    context->IASetVertexBuffers(MeshInputSlot::Normal, 1, &primitive.Normals());
                    context->IASetVertexBuffers(MeshInputSlot::Uv0, 1, &primitive.Uvs());

                    if (primitive.IsIndexed())
                    {
                        context->IASetIndexBuffer(&primitive.Indices());
                        context.DrawIndexedInstanced(primitive.VertexOrIndexCount(), 1);
                    }
                    else
                    {
                        context.DrawInstanced(primitive.VertexOrIndexCount(), 1);
                    }
                }
            }
        }

        //-------------------------------------------------------------------------------------------------------------
        // Transparents pass
        //-------------------------------------------------------------------------------------------------------------
        context.Flush();
        //TODO: Transparents pass

        //-------------------------------------------------------------------------------------------------------------
        // Debug overlays
        //-------------------------------------------------------------------------------------------------------------
        if (debugSettings.ShowLightBufferAverage)
        {
            PIXScopedEvent(&context, 4, "Debug overlay");
            context.SetRenderTarget(swapChain);
            context.SetFullViewportScissor(screenSize);
            lightLinkedList.DrawDebugOverlay(context, lightHeap, perFrameCbAddress);
        }

        //-------------------------------------------------------------------------------------------------------------
        // UI
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 4, "UI");
            ImGuiStyle& style = ImGui::GetStyle();

            // Submit main dockspace and menu bar
            {
                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->WorkPos);
                ImGui::SetNextWindowSize(viewport->WorkSize);
                ImGui::SetNextWindowViewport(viewport->ID);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
                ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
                flags |= ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
                ImGui::Begin("MainDockSpaceViewportWindow", nullptr, flags);
                ImGui::PopStyleVar(3);

                ImGui::DockSpace(0xC0FFEEEE, ImVec2(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode);

                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::BeginMenu("Debug"))
                    {
                        if (ImGui::Button("Reset all"))
                            debugSettings = DebugSettings();

                        ImGui::Checkbox("Light boundaries", &debugSettings.ShowLightBoundaries);
                        ImGui::Checkbox("Light buffer average colors", &debugSettings.ShowLightBufferAverage);

                        ImGui::Separator();

                        ImGui::Checkbox("Animate lights", &debugSettings.AnimateLights);

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Settings"))
                    {
                        ImGui::SliderFloat("Mouse sensitivity", &cameraInput.m_MouseSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Controller sensitivity", &cameraInput.m_ControllerLookSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Camera Fov", &cameraFovDegrees, 10.f, 140.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                        ImGui::Combo("Sync Mode", (int*)&presentMode, "Vsync\0NoSync\0Tearing\0");
                        ImGui::EndMenu();
                    }

                    // Submit frame time indicator
                    {
                        float graphWidth = 200.f + style.FramePadding.x * 2.f;
                        float frameTimeWidth = ImGui::CalcTextSize("Frame time: 9999.99 ms").x; // Use a static string so the position is stable

                        ImGui::SetCursorPosX(screenSizeF.x - graphWidth - frameTimeWidth);
                        ImGui::Text("Frame time: %.2f ms", deltaTimeMs);
                        ImGui::SetCursorPosX(screenSizeF.x - graphWidth);
                        ImGui::PlotLines
                        (
                            "##FrameTimeGraph",
                            frameTimes,
                            (int)std::size(frameTimes),
                            (int)(frameNumber % std::size(frameTimes)),
                            nullptr,
                            FLT_MAX, FLT_MAX,
                            ImVec2(200.f, ImGui::GetCurrentWindow()->MenuBarHeight())
                        );
                    }

                    ImGui::EndMenuBar();
                }

                ImGui::End();
            }

            ImGui::SetNextWindowSize(ImVec2(250.f, 0.f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Light linked list settings"))
            {
                ImGui::PushItemWidth(-FLT_MIN);
                char comboTemp[128];
                uint2 size = screenSize >> lightLinkedListShift;
                snprintf(comboTemp, sizeof(comboTemp), "1/%d (%dx%d)", (int)std::pow(2, lightLinkedListShift), size.x, size.y);
                ImGui::TextUnformatted("Buffer size");
                if (ImGui::BeginCombo("##lightLinkedListShiftCombo", comboTemp))
                {
                    for (uint32_t div = 1, i = 0; i <= downsampledDepthBuffers.size(); div *= 2, i++)
                    {
                        size = screenSize >> i;
                        snprintf(comboTemp, sizeof(comboTemp), "1/%d (%dx%d)", div, size.x, size.y);
                        bool isSelected = lightLinkedListShift == i;
                        if (ImGui::Selectable(comboTemp, isSelected))
                        {
                            lightLinkedListShift = i;
                        }

                        if (isSelected) { ImGui::SetItemDefaultFocus(); }
                    }
                    ImGui::EndCombo();
                }

                {
                    double size = (lightLinkLimit * ShaderInterop::SizeOfLightLink) / 1024.0;
                    const char* sizeUnits = "KB";
                    if (size > 1024.0)
                    {
                        size /= 1024.0;
                        sizeUnits = "MB";
                    }
                    ImGui::Text("Light links limit (%.2f %s)", size, sizeUnits);
                    ImGui::SliderInt("##lightLinksLimit", &lightLinkLimit, 0, LightLinkedList::MAX_LIGHT_LINKS, "%u", ImGuiSliderFlags_AlwaysClamp);
                }

                ImGui::PopItemWidth();
            }
            ImGui::End();

#if false
            ImGui::ShowDemoWindow();

            //TODO: Put these in the order they are in the descriptor table
            ImGui::Begin("Loaded Textures");
            {
                ImGui::Image((void*)environmentHdr.SrvHandle().ResidentHandle().ptr, ImVec2(128, 128));
                int i = 1;
                for (auto& texture : scene.m_TextureCache)
                {
                    if (i < 5) { ImGui::SameLine(); }
                    else { i = 0; }
                    UINT64 handle = texture.second->SrvHandle().ResidentHandle().ptr;
                    ImGui::Image((void*)handle, ImVec2(128, 128));
                    i++;
                }
            }
            ImGui::End();
#endif

            // Show controls hint
            {
                std::string controlsHint = std::format("Move with {}, click+drag look around or use Xbox controller.", cameraInput.WasdName());
                float padding = 5.f;
                float wrapWidth = screenSizeF.x - padding * 2.f;
                ImVec2 hintSize = ImGui::CalcTextSize(controlsHint, wrapWidth);
                ImVec2 position = ImVec2(padding, screenSize.y - hintSize.y - padding);

                ImGui::GetForegroundDrawList()->AddText
                (
                    nullptr, 0.f,
                    ImVec2(position.x - 1.f, position.y + 1.f),
                    IM_COL32(0, 0, 0, 255),
                    controlsHint.data(), controlsHint.data() + controlsHint.size(),
                    wrapWidth
                );

                ImGui::GetForegroundDrawList()->AddText
                (
                    nullptr, 0.f,
                    position,
                    IM_COL32(255, 255, 255, 255),
                    controlsHint.data(), controlsHint.data() + controlsHint.size(),
                    wrapWidth
                );
            }

            context.SetRenderTarget(swapChain);
            dearImGui.Render(context);
            dearImGui.RenderViewportWindows();
        }

        //-------------------------------------------------------------------------------------------------------------
        // Present
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(98, "Present");
            context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_PRESENT);
            context.Finish();

            swapChain.Present(presentMode);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Housekeeping
        //-------------------------------------------------------------------------------------------------------------
        PIXScopedEvent(99, "Housekeeping");
        graphics.UploadQueue().Cleanup();

        lastTimestamp.QuadPart = timestamp.QuadPart;
        frameNumber++;
    }

    // Cleanup
    OutputDebugStringW(L"Main loop exited, cleaning up...\n");
    window.RemoveWndProc(wndProcHandle);

    // Ensure the GPU is done with all our resources before they're implicitly destroyed by their destructors
    // If you get a deadlock here it's probably because you had the debug layer enabled and the PIX capture library loaded at the same time
    graphics.WaitForGpuIdle();
    return 0;
}

int main()
{
    int result = MainImpl();
    DebugLayer::ReportLiveObjects();
    return result;
}
