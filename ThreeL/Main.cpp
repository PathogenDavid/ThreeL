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

static std::wstring GetExtraWindowTitleInfo()
{
    std::wstring title = L"";

#ifndef NDEBUG
    title += L"Checked";
#endif

    if (DebugLayer::IsEnabled())
    {
        if (title.size() > 0) { title += L"/"; }
        title += L"DebugLayer";
    }

    if (GetModuleHandleW(L"WinPixGpuCapturer.dll") != NULL)
    {
        if (title.size() > 0) { title += L"/"; }
        title += L"PIX";
    }

    return title.size() == 0 ? title : L" (" + title + L")";
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
    windowTitle += GetExtraWindowTitleInfo();
    Window window(windowTitle.c_str(), 1280, 720);

    // Ensure PIX targets our main window and disable the HUD since it overlaps with our menu bar
    PIXSetTargetWindow(window.GetHwnd());
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
    uint2 screenSize = uint2::Zero;
    float2 screenSizeF = (float2)screenSize;
    DepthStencilBuffer depthBuffer(graphics, L"Depth Buffer", swapChain.Size(), DXGI_FORMAT_D32_FLOAT);

    //-----------------------------------------------------------------------------------------------------------------
    // Misc initialization
    //-----------------------------------------------------------------------------------------------------------------
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

    // Initiate final resource uploads and wait for them to complete
    resources.Finish();
    graphics.GetUploadQueue().Flush();

    window.Show();
    PIXEndEvent();

    //-----------------------------------------------------------------------------------------------------------------
    // Main loop
    //-----------------------------------------------------------------------------------------------------------------
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

        //-------------------------------------------------------------------------------------------------------------
        // Resize screen-dependent resources
        //-------------------------------------------------------------------------------------------------------------
        if (screenSize != swapChain.Size())
        {
            // Flush the GPU to ensure there's no outstanding usage of the resources
            graphics.WaitForGpuIdle();
            screenSize = swapChain.Size();
            screenSizeF = (float2)screenSize;

            depthBuffer.Resize(screenSize);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Frame setup
        //-------------------------------------------------------------------------------------------------------------
        GraphicsContext context(graphics.GetGraphicsQueue(), resources.PbrRootSignature, resources.PbrBlendOffSingleSided);
        {
            PIXScopedEvent(&context, 0, "Frame setup");
            context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
            context.Clear(swapChain, 0.2f, 0.2f, 0.2f, 1.f);
            context.Clear(depthBuffer);
            context.SetRenderTarget(swapChain, depthBuffer.View());

            D3D12_VIEWPORT viewport = { 0.f, 0.f, screenSizeF.x, screenSizeF.y, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
            context->RSSetViewports(1, &viewport);
            D3D12_RECT scissor = { 0, 0, (LONG)screenSize.x, (LONG)screenSize.y };
            context->RSSetScissorRects(1, &scissor);
        }

        ShaderInterop::PerFrameCb perFrame =
        {
            .ViewProjectionTransform = camera.GetViewTransform()
                * float4x4::MakePerspectiveTransformReverseZ(Math::Deg2Rad(cameraFovDegrees) , screenSizeF.x / screenSizeF.y, 0.0001f),
            .EyePosition = camera.GetPosition(),
        };

        //-------------------------------------------------------------------------------------------------------------
        // Start PBR material passes
        //-------------------------------------------------------------------------------------------------------------
        context->SetGraphicsRootSignature(resources.PbrRootSignature);
        context->SetGraphicsRoot32BitConstants(ShaderInterop::Pbr::RpPerFrameCb, sizeof(perFrame) / sizeof(UINT), &perFrame, 0);
        context->SetGraphicsRootShaderResourceView(ShaderInterop::Pbr::RpMaterialHeap, resources.PbrMaterials.BufferGpuAddress());
        context->SetGraphicsRootDescriptorTable(ShaderInterop::Pbr::RpSamplerHeap, graphics.GetSamplerHeap().GetGpuHeap()->GetGPUDescriptorHandleForHeapStart());
        context->SetGraphicsRootDescriptorTable(ShaderInterop::Pbr::RpBindlessHeap, graphics.GetResourceDescriptorManager().GetGpuHeap()->GetGPUDescriptorHandleForHeapStart());

        //-------------------------------------------------------------------------------------------------------------
        // Opaque Pass
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 1, "Opaque pass");
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

                    context->SetPipelineState(material.PipelineStateObject());
                    context->IASetVertexBuffers(MeshInputSlot::Position, 1, &primitive.Positions());
                    context->IASetVertexBuffers(MeshInputSlot::Normal, 1, &primitive.Normals());
                    context->IASetVertexBuffers(MeshInputSlot::Uv0, 1, &primitive.Uvs());

                    if (primitive.IsIndexed())
                    {
                        context->IASetIndexBuffer(&primitive.Indices());
                        context->DrawIndexedInstanced(primitive.VertexOrIndexCount(), 1, 0, 0, 0);
                    }
                    else
                    {
                        context->DrawInstanced(primitive.VertexOrIndexCount(), 1, 0, 0);
                    }
                }
            }
        }

        //-------------------------------------------------------------------------------------------------------------
        // Transparents Pass
        //-------------------------------------------------------------------------------------------------------------
        //TODO: Transparents pass

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
                    if (ImGui::BeginMenu("Settings"))
                    {
                        ImGui::SliderFloat("Mouse sensitivity", &cameraInput.m_MouseSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Controller sensitivity", &cameraInput.m_ControllerLookSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Camera Fov", &cameraFovDegrees, 10.f, 140.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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

#if false
            ImGui::ShowDemoWindow();

            //TODO: Put these in the order they are in the descriptor table
            ImGui::Begin("Loaded Textures");
            {
                ImGui::Image((void*)environmentHdr.SrvHandle().GetResidentHandle().ptr, ImVec2(128, 128));
                int i = 1;
                for (auto& texture : scene.m_TextureCache)
                {
                    if (i < 5) { ImGui::SameLine(); }
                    else { i = 0; }
                    UINT64 handle = texture.second->SrvHandle().GetResidentHandle().ptr;
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

            swapChain.Present();
        }

        //-------------------------------------------------------------------------------------------------------------
        // Housekeeping
        //-------------------------------------------------------------------------------------------------------------
        PIXScopedEvent(99, "Housekeeping");
        graphics.GetUploadQueue().Cleanup();

        lastTimestamp.QuadPart = timestamp.QuadPart;
        frameNumber++;
    }

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
