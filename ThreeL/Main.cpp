#include "pch.h"

#include "AssetLoading.h"
#include "BackBuffer.h"
#include "CameraController.h"
#include "CameraInput.h"
#include "CommandQueue.h"
#include "ComputeContext.h"
#include "DearImGui.h"
#include "DebugLayer.h"
#include "DepthStencilBuffer.h"
#include "FrameStatistics.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightHeap.h"
#include "LightLinkedList.h"
#include "ParticleSystem.h"
#include "ParticleSystemDefinition.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "ShaderInterop.h"
#include "Stopwatch.h"
#include "SwapChain.h"
#include "Utilities.h"
#include "Window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <iostream>
#include <pix3.h>
#include <random>

using ShaderInterop::LightLinkedListDebugMode;

struct DebugSettings
{
    bool ShowLightBoundaries = false;
    LightLinkedListDebugMode OverlayMode = LightLinkedListDebugMode::None;
    float OverlayAlpha = 0.25f;
    bool AnimateLights = true;
};

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

    // Ensure PIX targets our main window and disable the HUD since it overlaps with our menu bar
    PIXSetTargetWindow(window.Hwnd());
    PIXSetHUDOptions(PIX_HUD_SHOW_ON_NO_WINDOWS);

    SwapChain swapChain(graphics, window);

    ResourceManager resources(graphics);

    //-----------------------------------------------------------------------------------------------------------------
    // Load glTF model
    //-----------------------------------------------------------------------------------------------------------------
    Scene scene = LoadGltfScene
    (
        resources,
        // Sponza isn't centered for some reason, so we manually center it on the XZ plane to make spawning random lights easier
        "Assets/Sponza/Sponza.gltf", float4x4::MakeTranslation(0.531749f, 0.f, 0.253336f)
    );
    printf("Done.\n");

    //-----------------------------------------------------------------------------------------------------------------
    // Allocate depth buffers
    //-----------------------------------------------------------------------------------------------------------------
    uint2 screenSize = swapChain.Size();
    float2 screenSizeF = (float2)screenSize;
    DepthStencilBuffer depthBuffer(graphics, L"Depth Buffer", screenSize, DEPTH_BUFFER_FORMAT);

    std::vector<DepthStencilBuffer> downsampledDepthBuffers;
    for (int div = 2, shift = 1; div <= 8; div *= 2, shift++)
    {
        std::wstring name = std::format(L"Depth Buffer (1/{})", div);
        downsampledDepthBuffers.emplace_back(graphics, name, LightLinkedList::ScreenSizeToLllBufferSize(screenSize, shift), DEPTH_BUFFER_FORMAT);
    }

    //-----------------------------------------------------------------------------------------------------------------
    // Allocate lighting resources
    //-----------------------------------------------------------------------------------------------------------------
    LightHeap lightHeap(graphics);
    Texture lightSprite = LoadTexture(resources, "Assets/LightSprite.png");
    bool showLightSprites = false;
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

    std::mt19937 randomGenerator(3226);

    while (lights.size() < LightHeap::MAX_LIGHTS)
    {
        lights.push_back
        ({
            .Position = float3
            (
                std::uniform_real_distribution(-10.83f, 10.83f)(randomGenerator),
                std::uniform_real_distribution(0.f, 6.91f)(randomGenerator),
                std::uniform_real_distribution(-5.43f, 5.43f)(randomGenerator)
            ),
            .Range = std::uniform_real_distribution(0.1f, 2.5f)(randomGenerator),
            .Color = ImGui::ColorConvertHSVtoRGB
            (
                float3
                (
                    std::uniform_real_distribution(0.f, 1.f)(randomGenerator),
                    std::uniform_real_distribution(0.f, 1.f)(randomGenerator),
                    std::uniform_real_distribution(0.5f, 1.f)(randomGenerator)
                )
            ),
            .Intensity = std::uniform_real_distribution(0.5f, 3.f)(randomGenerator),
        });
    }

    //-----------------------------------------------------------------------------------------------------------------
    // Particle system initialization
    //-----------------------------------------------------------------------------------------------------------------
    bool showParticleSystemEditor = false;
    bool showParticleSystemGizmo = true;
    ParticleSystemDefinition smokeDefinition(resources);
    smokeDefinition.SpawnRate = 3.5f;
    smokeDefinition.MaxSize = 0.2f;

    smokeDefinition.FadeOutTime = 2.f;
    smokeDefinition.LifeMin = 70.f;
    smokeDefinition.LifeMax = 70.5f;

    smokeDefinition.AngularVelocityMin = -0.2f;
    smokeDefinition.AngularVelocityMax = 0.2f;

    smokeDefinition.VelocityDirectionVariance = float3(0.045f, 0.f, 0.045f);
    smokeDefinition.VelocityDirectionBias = float3(-0.03f, 1.f, -0.1f);
    smokeDefinition.VelocityMagnitudeMin = 0.15f;
    smokeDefinition.VelocityMagnitudeMax = 0.15f;
    
    smokeDefinition.SpawnPointVariance = float3(0.05f, 0.f, 0.05f);

    smokeDefinition.BaseColor = float3::One;
    smokeDefinition.MinShade = 0.75f;
    smokeDefinition.MaxShade = 1.f;

    for (int i = 1; i <= 8; i++)
    {
        smokeDefinition.AddParticleTexture(std::format("Assets/KenneyParticles/smoke_{:02}.png", i));
    }

    ParticleSystem smoke(resources, L"Smoke", smokeDefinition, float3(0.2f, 0.f, 0.2f), 1024);
    {
        Stopwatch sw;
        printf("Seeding particle system...\n");
        smoke.SeedState(70.f);
        printf("Done in %f seconds.\n", sw.ElapsedSeconds());
    }

    //-----------------------------------------------------------------------------------------------------------------
    // Misc initialization
    //-----------------------------------------------------------------------------------------------------------------
    DebugSettings debugSettings = DebugSettings();
    PresentMode presentMode = PresentMode::Vsync;
    DearImGui dearImGui(graphics, window);
    FrameStatistics stats(graphics);

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
    bool showControlsHint = true;
    CameraInput cameraInput(window);
    CameraController camera;
    float cameraFovDegrees = 45.f;

    // Start the camera in a sensible location for Sponza
    camera.WarpTo(float3(4.35f, 1.f, 0.f), 0.f, Math::HalfPi);

    // Per-frame constant buffer
    // This is a pretty heavyweight abstraction for a constant buffer, ideally we should have a little bump allocator for this sort of thing
    FrequentlyUpdatedResource perFrameCbResource(graphics, DescribeBufferResource(sizeof(ShaderInterop::PerFrameCb)), L"Per-frame constant buffer");

    // Custom WndProc
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
                case WM_DPICHANGED:
                {
                    WORD dpi = HIWORD(wParam);
                    Assert(dpi == LOWORD(wParam) && "The X and Y DPIs should match!");
                    float dpiScale = (float)dpi / (float)USER_DEFAULT_SCREEN_DPI;
                    dearImGui.ScaleUi(dpiScale);

                    RECT* suggestedRectangle = (RECT*)lParam;
                    AssertWinError(SetWindowPos
                    (
                        hwnd,
                        nullptr,
                        suggestedRectangle->left,
                        suggestedRectangle->top,
                        suggestedRectangle->right - suggestedRectangle->left,
                        suggestedRectangle->bottom - suggestedRectangle->top,
                        SWP_NOZORDER | SWP_NOACTIVATE
                    ));

                    return 0;
                }
                default:
                    return { };
            }
        });

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

        stats.StartFrame();

        cameraInput.StartFrame(deltaTime);
        camera.ApplyMovement(cameraInput.MoveVector(), cameraInput.LookVector());

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

            int shift = 1;
            for (DepthStencilBuffer& depthBuffer : downsampledDepthBuffers)
            {
                depthBuffer.Resize(LightLinkedList::ScreenSizeToLllBufferSize(screenSize, shift));
                shift++;
            }

            lightLinkedList.Resize(screenSize);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Frame setup
        //-------------------------------------------------------------------------------------------------------------
        float4x4 perspectiveTransform;
        ShaderInterop::PerFrameCb perFrame;
        D3D12_GPU_VIRTUAL_ADDRESS perFrameCbAddress;

        GpuSyncPoint lightUpdateSyncPoint;

        GraphicsContext context(graphics.GraphicsQueue(), resources.PbrRootSignature, resources.PbrBlendOffSingleSided);
        {
            PIXScopedEvent(&context, 0, "Frame #%lld setup", frameNumber);
            context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_RENDER_TARGET);
            context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            context.Clear(swapChain, 0.01f, 0.01f, 0.01f, 1.f);
            context.Clear(depthBuffer);

            perspectiveTransform = float4x4::MakePerspectiveTransformReverseZ(Math::Deg2Rad(cameraFovDegrees) , screenSizeF.x / screenSizeF.y, 0.0001f);
            perFrame =
            {
                .ViewProjectionTransform = camera.ViewTransform() * perspectiveTransform,
                .EyePosition = camera.Position(),
                .LightLinkedListBufferWidth = LightLinkedList::ScreenSizeToLllBufferSize(screenSize, lightLinkedListShift).x,
                .LightLinkedListBufferShift = lightLinkedListShift,
                .DeltaTime = deltaTime,
                .FrameNumber = (uint32_t)frameNumber,
                .LightCount = std::min((uint32_t)lights.size(), LightHeap::MAX_LIGHTS),
                .ViewTransformInverse = camera.ViewTransform().Inverted(),
            };
            perFrame.ViewProjectionTransformInverse = perFrame.ViewProjectionTransform.Inverted();

            GpuSyncPoint perFrameCbSyncPoint = perFrameCbResource.Update(perFrame);
            graphics.GraphicsQueue().AwaitSyncPoint(perFrameCbSyncPoint); // (This is part of why FrequentlyUpdatedResource isn't the ideal abstraction here)
            graphics.ComputeQueue().AwaitSyncPoint(perFrameCbSyncPoint); // Make sure perFrame is available to async compute
            perFrameCbAddress = perFrameCbResource.Current()->GetGPUVirtualAddress();

            lightUpdateSyncPoint = lightHeap.Update(lights);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Update particle system
        //-------------------------------------------------------------------------------------------------------------
#if true
        smoke.Update(context.Compute(), deltaTime, perFrameCbAddress);
#else
        // Alternatively we could update particles on the async compute queue instead of the graphics queue
        // In practice this didn't really add anything other than complexity, so I've disabled it
        {
            ComputeContext asyncCompute(graphics.ComputeQueue());
            smoke.Update(asyncCompute, deltaTime, perFrameCbAddress);            
            asyncCompute.Finish();
        }
#endif

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
                lightLinkedListShift,
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
        {
            PIXScopedEvent(&context, 3, "Transparents pass");
            //TODO: Sort and render transparent nodes from glTF (there aren't any in Sponza so I never implemented this)

            context.SetRenderTarget(swapChain, depthBuffer.ReadOnlyView());
            context.SetFullViewportScissor(screenSize);
            smoke.Render(context, perFrameCbAddress, lightHeap, lightLinkedList);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Debug overlays
        //-------------------------------------------------------------------------------------------------------------
        uint32_t maxLightsPerPixelForOverlay = 0;
        if (debugSettings.OverlayMode != LightLinkedListDebugMode::None)
        {
            PIXScopedEvent(&context, 4, "Debug overlay");
            context.SetRenderTarget(swapChain);
            context.SetFullViewportScissor(screenSize);
            ShaderInterop::LightLinkedListDebugParams params =
            {
                .Mode = debugSettings.OverlayMode,
                .MaxLightsPerPixel = maxLightsPerPixelForOverlay = (std::max(1u, stats.MaximumLightCountForAnyPixel()) + 9) / 10 * 10,
                .DebugOverlayAlpha = debugSettings.OverlayAlpha,
            };
            lightLinkedList.DrawDebugOverlay(context, lightHeap, perFrameCbAddress, params);
        }

        // Light sprites
        if (showLightSprites)
        {
            PIXScopedEvent(&context, 4, "Light sprites");
            context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
            context.SetRenderTarget(swapChain, depthBuffer.View());
            context.SetFullViewportScissor(screenSize);
            context->SetGraphicsRootSignature(resources.LightSpritesRootSignature);
            context->SetPipelineState(resources.LightSprites);
            context->SetGraphicsRootConstantBufferView(ShaderInterop::LightSprites::RpPerFrameCb, perFrameCbAddress);
            context->SetGraphicsRootShaderResourceView(ShaderInterop::LightSprites::RpLightHeap, lightHeap.BufferGpuAddress());
            context->SetGraphicsRootDescriptorTable(ShaderInterop::LightSprites::RpTexture, lightSprite.SrvHandle().ResidentHandle());
            context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            context.DrawInstanced(4, perFrame.LightCount, 1, 1);
        }

        // Show gizmo for moving lights
        //TODO: Add UI for selecting different lights instead of hard-coding things
        if (lights.size() > 2)
        {
            float4x4 lightWorld = float4x4::MakeTranslation(lights[2].Position);
            if (ImGuizmo::Manipulate(camera.ViewTransform(), perspectiveTransform, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, lightWorld))
            {
                lights[2].Position = float3(lightWorld.m30, lightWorld.m31, lightWorld.m32);
            }
        }

        //-------------------------------------------------------------------------------------------------------------
        // UI
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 4, "UI");
            ImGuiStyle& style = ImGui::GetStyle();

            // Submit main dockspace and menu bar
            ImGuiDockNode* centralNode;
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
                centralNode = ImGui::DockBuilderGetCentralNode(0xC0FFEEEE);

                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::BeginMenu("Debug"))
                    {
                        if (ImGui::Button("Reset all"))
                            debugSettings = DebugSettings();

                        ImGui::Checkbox("Light boundaries", &debugSettings.ShowLightBoundaries);
                        ImGui::Combo("Overlay", (int*)&debugSettings.OverlayMode, ShaderInterop::LightLinkedListDebugModeNames);
                        ImGui::SliderFloat("Overlay Alpha", &debugSettings.OverlayAlpha, 0.05f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

                        ImGui::Separator();

                        ImGui::Checkbox("Animate lights", &debugSettings.AnimateLights);

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("View"))
                    {
                        ImGui::Checkbox("Show light sprites", &showLightSprites);
                        ImGui::Checkbox("Show controls hint", &showControlsHint);
                        ImGui::Checkbox("Particle Editor", &showParticleSystemEditor);
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Settings"))
                    {
                        ImGui::SliderFloat("Mouse sensitivity", &cameraInput.m_MouseSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Controller sensitivity", &cameraInput.m_ControllerLookSensitivity, 0.1f, 10.f);
                        ImGui::SliderFloat("Camera Fov", &cameraFovDegrees, 10.f, 140.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                        const char* syncModes[] = { "Vsync", "No sync", "Tearing" };
                        if (ImGui::BeginCombo("Sync Mode", syncModes[(int)presentMode]))
                        {
                            if (ImGui::Selectable("Vsync", presentMode == PresentMode::Vsync)) { presentMode = PresentMode::Vsync; }
                            if (ImGui::Selectable("No sync", presentMode == PresentMode::NoSync)) { presentMode = PresentMode::NoSync; }
                            ImGui::BeginDisabled(!swapChain.SupportsTearing());
                            if (ImGui::Selectable("Tearing", presentMode == PresentMode::Tearing)) { presentMode = PresentMode::Tearing; }
                            if (!swapChain.SupportsTearing()) { ImGui::SetItemTooltip("Your computer doesn't support modern tearing."); }
                            ImGui::EndDisabled();
                            ImGui::EndCombo();
                        }
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

            // Light linked list settings
            ImGui::SetNextWindowSize(ImVec2(275.f * dearImGui.DpiScale(), 0.f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Light linked list settings"))
            {
                ImGui::PushItemWidth(-FLT_MIN);
                char comboTemp[128];
                uint2 size = LightLinkedList::ScreenSizeToLllBufferSize(screenSize, lightLinkedListShift);
                uint2 currentLightBufferSize = size;
                snprintf(comboTemp, sizeof(comboTemp), "1/%d (%dx%d)", (int)std::pow(2, lightLinkedListShift), size.x, size.y);
                ImGui::TextUnformatted("Buffer size");
                if (ImGui::BeginCombo("##lightLinkedListShiftCombo", comboTemp))
                {
                    for (uint32_t div = 1, i = 0; i <= downsampledDepthBuffers.size(); div *= 2, i++)
                    {
                        size = LightLinkedList::ScreenSizeToLllBufferSize(screenSize, i);
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
                    ImGui::Text("Light links heap");
                    double speed = 16.0 + std::pow((double)lightLinkLimit / (double)LightLinkedList::MAX_LIGHT_LINKS, 2.0) * 5120000.0;
                    ImGui::DragInt("##lightLinksLimit", &lightLinkLimit, (float)std::max(1.0, speed), 0, LightLinkedList::MAX_LIGHT_LINKS, "%u", ImGuiSliderFlags_AlwaysClamp);
                }

                ImGui::SeparatorText("Buffer Sizes");
                {
                    const char* sizeUnits;
                    size_t sizeBytes;
                    double size;
                    size_t totalSize = 0;

                    totalSize += sizeBytes = currentLightBufferSize.x * currentLightBufferSize.y * sizeof(uint32_t);
                    size = GetHumanFriendlySize(sizeBytes, sizeUnits);
                    ImGui::Text("First link buffer: %.2f %s", size, sizeUnits);

                    totalSize += sizeBytes = lightLinkLimit * ShaderInterop::SizeOfLightLink;
                    double heapUsed = ((double)stats.NumberOfLightLinksUsed() / (double)lightLinkLimit) * 100.0;
                    size = GetHumanFriendlySize(sizeBytes, sizeUnits);
                    ImGui::Text(" Light links heap: %.2f %s (%.2f%%)", size, sizeUnits, heapUsed);

                    size = GetHumanFriendlySize(sizeBytes, sizeUnits);
                    ImGui::Text("            Total: %.2f %s", size, sizeUnits);
                }

                ImGui::SeparatorText("Frame Statistics");
                {
                    ImGui::Text("Light links used: %d", stats.NumberOfLightLinksUsed());
                    ImGui::Text("Max lights per pixel: %d", stats.MaximumLightCountForAnyPixel());
                    ImGui::Text("Average lights per pixel: %.1f", (double)stats.NumberOfLightLinksUsed() / (double)(currentLightBufferSize.x * currentLightBufferSize.y));
                }

                ImGui::PopItemWidth();
            }
            ImGui::End();

            // Particle editor
            if (showParticleSystemEditor)
            {
                ImGui::SetNextWindowSize(ImVec2(275.f * dearImGui.DpiScale(), 0.f), ImGuiCond_FirstUseEver);
                if (ImGui::Begin("Particle Editor", &showParticleSystemEditor))
                {
                    ImGui::PushItemWidth(-90.f);
                    ImGui::DragFloat("Spawn rate", &smokeDefinition.SpawnRate, 0.1f, 0.f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Max size", &smokeDefinition.MaxSize, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    
                    ImGui::SeparatorText("Lifetime");
                    ImGui::SliderFloat("Fade out time", &smokeDefinition.FadeOutTime, 0.f, smokeDefinition.LifeMin, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloatRange2("Lifetime", &smokeDefinition.LifeMin, &smokeDefinition.LifeMax, 0.1f, 0.f, FLT_MAX, "%.1f s", nullptr, ImGuiSliderFlags_AlwaysClamp);

                    ImGui::SeparatorText("Movement");
                    ImGui::Text("Velocity direction variance");
                    ImGui::DragFloat("X##VelocityDirectionVariance", &smokeDefinition.VelocityDirectionVariance.x, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Y##VelocityDirectionVariance", &smokeDefinition.VelocityDirectionVariance.y, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Z##VelocityDirectionVariance", &smokeDefinition.VelocityDirectionVariance.z, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::Text("Velocity direction bias");
                    ImGui::DragFloat("X##VelocityDirectionBias", &smokeDefinition.VelocityDirectionBias.x, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Y##VelocityDirectionBias", &smokeDefinition.VelocityDirectionBias.y, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Z##VelocityDirectionBias", &smokeDefinition.VelocityDirectionBias.z, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::Text("Velocity magnitude");
                    ImGui::DragFloatRange2("Linear", &smokeDefinition.VelocityMagnitudeMin, &smokeDefinition.VelocityMagnitudeMax, 0.01f);
                    ImGui::DragFloatRange2("Angular", &smokeDefinition.AngularVelocityMin, &smokeDefinition.AngularVelocityMax, 0.01f, 0.f, 0.f, "%.3f rad/s");

                    ImGui::SeparatorText("Spawn point");
                    ImGui::Checkbox("Show spawn point gizmo", &showParticleSystemGizmo);
                    ImGui::DragFloat("X##SpawnPointVariance", &smokeDefinition.SpawnPointVariance.x, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Y##SpawnPointVariance", &smokeDefinition.SpawnPointVariance.y, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("Z##SpawnPointVariance", &smokeDefinition.SpawnPointVariance.z, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

                    ImGui::SeparatorText("Visuals");
                    ImGui::ColorEdit3("Base color", &smokeDefinition.BaseColor.x);
                    ImGui::DragFloatRange2("Shade", &smokeDefinition.MinShade, &smokeDefinition.MaxShade, 0.001f, 0.f, 1.f, "%.3f", nullptr, ImGuiSliderFlags_AlwaysClamp);

                    ImGui::SeparatorText("Commands");
                    if (ImGui::Button("Reset", ImVec2(-1.f, 0.f))) { smoke.Reset(context.Compute()); }

                    static float seedSeconds = 10.f;
                    ImGui::PopItemWidth();
                    ImGui::PushItemWidth(-FLT_MIN);
                    if (ImGui::Button("Advance system by")) { smoke.SeedState(seedSeconds); }
                    ImGui::SameLine();
                    ImGui::SliderFloat("##seedSeconds", &seedSeconds, 1.f / 30.f, smokeDefinition.LifeMax, "%.1f seconds");
                    ImGui::PopItemWidth();
                }
                ImGui::End();

                if (showParticleSystemGizmo)
                {
                    float3 spawnPoint = smoke.SpawnPoint();
                    if (ImGuizmo::Manipulate(camera.ViewTransform(), perspectiveTransform, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, spawnPoint))
                    { smoke.SpawnPoint(spawnPoint); }
                }
            }

            // Show viewport overlays
            {
                float padding = 5.f;
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                ImU32 white = IM_COL32(255, 255, 255, 255);
                ImU32 black = IM_COL32(0, 0, 0, 255);

                // Show overlay legend
                float overlayLegendWidth = 0.f;
                if (debugSettings.OverlayMode == LightLinkedListDebugMode::LightCount)
                {
                    float w = std::min(screenSizeF.x * 0.25f, 270.f * dearImGui.DpiScale());
                    overlayLegendWidth = w + padding + 2.f;
                    float h = 20.f * dearImGui.DpiScale();

                    float x0 = centralNode->Pos.x + centralNode->Size.x - w - padding - 1.f;
                    float x1 = x0 + (w / 3.f);
                    float x2 = x0 + (w / 3.f * 2.f);
                    float x3 = x0 + w;

                    float y0 = centralNode->Pos.y + centralNode->Size.y - h - padding - 1.f;
                    float y1 = y0 + h;

                    drawList->AddRectFilled(ImVec2(x0 - 1.f, y0 - 1.f), ImVec2(x3 + 1.f, y1 + 1.f), white);
                    uint32_t color0 = IM_COL32(0, 0, 255, 255);
                    uint32_t color1 = IM_COL32(0, 255, 255, 255);
                    uint32_t color2 = IM_COL32(0, 255, 0, 255);
                    uint32_t color3 = IM_COL32(255, 255, 0, 255);
                    drawList->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), color0, color1, color1, color0);
                    drawList->AddRectFilledMultiColor(ImVec2(x1, y0), ImVec2(x2, y1), color1, color2, color2, color1);
                    drawList->AddRectFilledMultiColor(ImVec2(x2, y0), ImVec2(x3, y1), color2, color3, color3, color2);

                    std::string labelMax = std::format("{}", maxLightsPerPixelForOverlay);
                    ImVec2 labelSize = ImGui::CalcTextSize(labelMax);
                    float labelY = y0 + h * 0.5f - labelSize.y * 0.5f;
                    ImGui::HudText(drawList, ImVec2(x0 + padding, labelY), "0");
                    ImGui::HudText(drawList, ImVec2(x3 - labelSize.x - padding, labelY), labelMax);
                }

                // Show controls hint
                if (showControlsHint)
                {
                    std::string controlsHint = std::format("Move with {}, click+drag look around or use Xbox controller.", cameraInput.WasdName());
                    float wrapWidth = centralNode->Size.x - padding * 2.f - overlayLegendWidth;
                    ImVec2 hintSize = ImGui::CalcTextSize(controlsHint, false, wrapWidth);
                    ImVec2 position = ImVec2(centralNode->Pos.x + padding, centralNode->Pos.y + centralNode->Size.y - hintSize.y - padding);
                    ImGui::HudText(drawList, position, controlsHint, wrapWidth);
                }
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
            context.Flush();

            swapChain.Present(presentMode);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Collect frame statistics
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 99, "Collect frame statistics");
            stats.StartCollectStatistics(context);
            lightLinkedList.CollectStatistics(context.Compute(), screenSize, lightLinkedListShift, stats.LightLinkedListStatisticsLocation());
            stats.FinishCollectStatistics(context);
        }

        //-------------------------------------------------------------------------------------------------------------
        // Housekeeping
        //-------------------------------------------------------------------------------------------------------------
        PIXScopedEvent(99, "Housekeeping");
        context.Finish();

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
