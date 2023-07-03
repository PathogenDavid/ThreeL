#include "pch.h"
#include "BackBuffer.h"
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

static Texture LoadHdr(GraphicsCore& graphics, std::string filePath)
{
    int width;
    int height;
    int channels;
    float* data = stbi_loadf(filePath.c_str(), &width, &height, &channels, 0);
    Texture texture(graphics, std::format(L"{}", filePath), std::span(data, width * height * channels), uint2((uint32_t)width, (uint32_t)height), channels);
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

    Window window(L"ThreeL", 1280, 720);

    // Don't show the PIX HUD on Dear ImGui viewports
    PIXSetTargetWindow(window.GetHwnd());
    PIXSetHUDOptions(PIX_HUD_SHOW_ON_TARGET_WINDOW_ONLY);

    SwapChain swapChain(graphics, window);

    ResourceManager resources(graphics);

    //-----------------------------------------------------------------------------------------------------------------
    // Load environment HDR
    //-----------------------------------------------------------------------------------------------------------------
    Texture environmentHdr = LoadHdr(graphics, "Assets/ninomaru_teien_1k.hdr");
    Assert(environmentHdr.BindlessIndex() == 0); // We currently hard-code the environment HDR to be bindless slot 0 in the shader

    //-----------------------------------------------------------------------------------------------------------------
    // Load glTF model
    //-----------------------------------------------------------------------------------------------------------------
    Scene scene = LoadGltfScene(resources,
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
            context.Clear(swapChain, 0.f, 0.f, 1.f, 1.f);
            context.Clear(depthBuffer);
            context.SetRenderTarget(swapChain, depthBuffer.View());

            D3D12_VIEWPORT viewport = { 0.f, 0.f, screenSizeF.x, screenSizeF.y, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
            context->RSSetViewports(1, &viewport);
            D3D12_RECT scissor = { 0, 0, (LONG)screenSize.x, (LONG)screenSize.y };
            context->RSSetScissorRects(1, &scissor);

            dearImGui.NewFrame();
        }

        ShaderInterop::PerFrameCb perFrame =
        {
            .ViewProjectionTransform = float4x4::MakeCameraLookAtViewTransform(float3(0.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3::UnitZ)
                * float4x4::MakePerspectiveTransformReverseZ(0.8f, screenSizeF.x / screenSizeF.y, 0.0001f),
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
                    //TODO: Normal transform
                    .NormalTransform = float4x4::Identity,
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
        //TODO

        //-------------------------------------------------------------------------------------------------------------
        // UI
        //-------------------------------------------------------------------------------------------------------------
        {
            PIXScopedEvent(&context, 4, "UI");
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
    }

    return 0;
}

int main()
{
    int result = MainImpl();
    DebugLayer::ReportLiveObjects();
    return result;
}
