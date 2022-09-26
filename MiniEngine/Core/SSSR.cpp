//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pch.h"
#include "SSSR.h"
#include "BufferManager.h"
//#include "GraphicsCore.h"
//#include "GraphicsCommon.h"
#include "CommandContext.h"
#include "SystemTime.h"
//#include "PostEffects.h"
#include "Camera.h"
#include "TextureManager.h"
#include "TemporalEffects.h"

#include "CompiledShaders/SSSRRecursiveCS.h"
#include "CompiledShaders/SSSRRayCastCS.h"
#include "CompiledShaders/SSSRResolveCS.h"
#include "CompiledShaders/SSSRTemporalCS.h"
#include "CompiledShaders/SSSRCombineCS.h"
#include "CompiledShaders/SSSRDebugOverlayCS.h"

using namespace Graphics;
using namespace Math;
using namespace SSSR;

namespace SSSR
{
    BoolVar EnableSSSR("Graphics/SSSR/Enable", true);
    //NumVar Sharpness("Graphics/AA/TAA/Sharpness", 0.5f, 0.0f, 1.0f, 0.25f);
    //NumVar TemporalMaxLerp("Graphics/AA/TAA/Blend Factor", 1.0f, 0.0f, 1.0f, 0.01f);
    //ExpVar TemporalSpeedLimit("Graphics/AA/TAA/Speed Limit", 64.0f, 1.0f, 1024.0f, 1.0f);
    //BoolVar TriggerReset("Graphics/AA/TAA/Reset", false);
    //BoolVar EnableCBR("Graphics/CBR/Enable", false);

    RootSignature s_RootSignature;

    ComputePSO s_RecursiveCS(L"SSSR: Recursive CS");
    ComputePSO s_RayCastCS(L"SSSR: Ray Cast CS");
    ComputePSO s_ResolveCS(L"SSSR: Resolve CS");
    ComputePSO s_TemporalCS(L"SSSR: Temporal CS");
    ComputePSO s_CombineCS(L"SSSR: Combine CS");
    ComputePSO s_DebugOverlayCS(L"SSSR: Debug Overlay CS");

    TextureRef s_NoiseTexture;
    int s_SampleIndex = 0;
    const int k_SampleCount = 64;

    uint32_t s_FrameIndex = 0;
    uint32_t s_FrameIndexMod2 = 0;
    float s_JitterX = 0.5f;
    float s_JitterY = 0.5f;
    float s_JitterDeltaX = 0.0f;
    float s_JitterDeltaY = 0.0f;

    void Recursive(ComputeContext& Context, const Camera& camera);
    void RayCast(ComputeContext& Context, const Camera& camera, float jitterSampleX, float jitterSampleY);
    void Resolve(ComputeContext& Context, const Camera& camera, float jitterSampleX, float jitterSampleY);
    void Temporal(ComputeContext& Context, const Camera& camera);
    void Combine(ComputeContext& Context, const Camera& camera);

    void DebugOverlay(CommandContext& BaseContext);
}

float GetHaltonValue(int index, int radix)
{
    float result = 0.f;
    float fraction = 1.f / (float)radix;

    while (index > 0)
    {
        result += (float)(index % radix) * fraction;

        index /= radix;
        fraction /= (float)radix;
    }

    return result;
}

void GenerateRandomOffset(float& offsetX, float& offsetY)
{
    offsetX = GetHaltonValue(s_SampleIndex & 1023, 2);
    offsetY = GetHaltonValue(s_SampleIndex & 1023, 3);

    if (++s_SampleIndex >= k_SampleCount)
        s_SampleIndex = 0;
}

void SSSR::Initialize( void )
{
    s_NoiseTexture = TextureManager::LoadDDSFromFile("Textures/tex_BlueNoise_1024x1024_UNI.dds");

    s_RootSignature.Reset(4, 3);
    s_RootSignature[0].InitAsConstants(0, 4);
    s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
    s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
    s_RootSignature[3].InitAsConstantBuffer(1);
    s_RootSignature.InitStaticSampler(0, SamplerLinearClampDesc);
    s_RootSignature.InitStaticSampler(1, SamplerPointBorderDesc);
    s_RootSignature.InitStaticSampler(2, SamplerAnisoWrapDesc);
    s_RootSignature.Finalize(L"SSSR");

#define CreatePSO( ObjName, ShaderByteCode ) \
    ObjName.SetRootSignature(s_RootSignature); \
    ObjName.SetComputeShader(ShaderByteCode, sizeof(ShaderByteCode) ); \
    ObjName.Finalize();

    CreatePSO( s_RecursiveCS, g_pSSSRRecursiveCS);
    CreatePSO(s_RayCastCS, g_pSSSRRayCastCS);
    CreatePSO(s_ResolveCS, g_pSSSRResolveCS);
    CreatePSO(s_TemporalCS, g_pSSSRTemporalCS);
    CreatePSO(s_CombineCS, g_pSSSRCombineCS);
    CreatePSO(s_DebugOverlayCS, g_pSSSRDebugOverlayCS);

#undef CreatePSO
}

void SSSR::Shutdown( void )
{
}

//void SSSR::Update( uint64_t FrameIndex )
//{
//    // Not implemented on Desktop yet
//    EnableCBR = false;
//
//    s_FrameIndex = (uint32_t)FrameIndex;
//    s_FrameIndexMod2 = s_FrameIndex % 2;
//
//    if (EnableTAA)// && !DepthOfField::Enable)
//    {
//        static const float Halton23[8][2] =
//        {
//            { 0.0f / 8.0f, 0.0f / 9.0f }, { 4.0f / 8.0f, 3.0f / 9.0f },
//            { 2.0f / 8.0f, 6.0f / 9.0f }, { 6.0f / 8.0f, 1.0f / 9.0f },
//            { 1.0f / 8.0f, 4.0f / 9.0f }, { 5.0f / 8.0f, 7.0f / 9.0f },
//            { 3.0f / 8.0f, 2.0f / 9.0f }, { 7.0f / 8.0f, 5.0f / 9.0f }
//        };
//
//        const float* Offset = nullptr;
//
//        // With CBR, having an odd number of jitter positions is good because odd and even
//        // frames can both explore all sample positions.  (Also, the least useful sample is
//        // the first one, which is exactly centered between four pixels.)
//        if (EnableCBR)
//            Offset = Halton23[s_FrameIndex % 7 + 1];
//        else
//            Offset = Halton23[s_FrameIndex % 8];
//
//        s_JitterDeltaX = s_JitterX - Offset[0];
//        s_JitterDeltaY = s_JitterY - Offset[1];
//        s_JitterX = Offset[0];
//        s_JitterY = Offset[1];
//    }
//    else
//    {
//        s_JitterDeltaX = s_JitterX - 0.5f;
//        s_JitterDeltaY = s_JitterY - 0.5f;
//        s_JitterX = 0.5f;
//        s_JitterY = 0.5f;
//    }
//
//}

//uint32_t SSSR::GetFrameIndexMod2( void )
//{
//    return s_FrameIndexMod2;
//}

//void SSSR::ClearHistory( CommandContext& Context )
//{
//    GraphicsContext& gfxContext = Context.GetGraphicsContext();
//
//    if (EnableTAA)
//    {
//        gfxContext.TransitionResource(g_TemporalColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
//        gfxContext.TransitionResource(g_TemporalColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
//        gfxContext.ClearColor(g_TemporalColor[0]);
//        gfxContext.ClearColor(g_TemporalColor[1]);
//    }
//}

void SSSR::Render( CommandContext& BaseContext, const Camera& camera)
{
    ScopedTimer _prof(L"SSSR", BaseContext);

    ComputeContext& Context = BaseContext.GetComputeContext();

    static bool s_EnableSSSR = false;
    //static bool s_EnableCBR = false;

    if (EnableSSSR != s_EnableSSSR /*|| EnableCBR && !s_EnableCBR || TriggerReset*/)
    {
        //ClearHistory(Context);
        s_EnableSSSR = EnableSSSR;
        //s_EnableCBR = EnableCBR;
        //TriggerReset = false;
    }

    //uint32_t Src = s_FrameIndexMod2;
    //uint32_t Dst = Src ^ 1;

    {
        Recursive(Context, camera);

        float jitterSampleX, jitterSampleY;
        GenerateRandomOffset(jitterSampleX, jitterSampleY);

        RayCast(Context, camera, jitterSampleX, jitterSampleY);

        Context.CopyBuffer(g_SSSRMipMapBuffer2, g_SSSRMainBuffer0);
        g_SSSRMipMapBuffer2.GenerateMipMaps(Context);

        Resolve(Context, camera, jitterSampleX, jitterSampleY);

        Temporal(Context, camera);

        Combine(Context, camera);
    }

    //D3D12_RESOURCE_STATES oldSceneColorBufferState = g_SceneColorBuffer.GetUsageState();
    //D3D12_RESOURCE_STATES oldSSSRRayCastState = g_SSSRRayCast.GetUsageState();

    //RECT rc = { 0, 0, (LONG)g_SSSRRayCast->GetDesc().Width, (LONG)g_SSSRRayCast->GetDesc().Height };
    //Context.CopyTextureRegion(g_SceneColorBuffer, 0, 0, 0, g_SSSRRayCast, rc);

    //Context.TransitionResource(g_SceneColorBuffer, oldSceneColorBufferState);
    //Context.TransitionResource(g_SSSRRayCast, oldSSSRRayCastState);
}

void SSSR::Recursive(ComputeContext& Context, const Camera& camera)
{
    ScopedTimer _prof(L"Recursive", Context);

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_RecursiveCS);

    __declspec(align(16)) struct ConstantBuffer
    {
        Vector3 _WorldSpaceCameraPos;

        Matrix4 _InverseProjectionMatrix;
        Matrix4 _InverseViewProjectionMatrix;
        Matrix4 _WorldToCameraMatrix;

        uint32_t _ScreenSize[2];
    };
    ConstantBuffer cbv = {
        camera.GetPosition(),

        Invert(camera.GetProjMatrix()),
        Invert(camera.GetViewProjMatrix()),
        camera.GetViewMatrix(),

        g_SSSRMainBuffer1.GetWidth(), g_SSSRMainBuffer1.GetHeight()
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRMainBuffer1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRMainBuffer0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.SetDynamicDescriptor(1, 4, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 5, g_SSSRMainBuffer1.GetSRV());
    Context.SetDynamicDescriptor(2, 0, g_SSSRMainBuffer0.GetUAV());

    Context.Dispatch2D(g_SSSRMainBuffer1.GetWidth(), g_SSSRMainBuffer1.GetHeight(), 8, 8);
}

void SSSR::RayCast(ComputeContext& Context, const Camera& camera, float jitterSampleX, float jitterSampleY)
{
    ScopedTimer _prof(L"RayCast", Context);

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_RayCastCS);

    __declspec(align(16)) struct ConstantBuffer
    {
        Vector3 _WorldSpaceCameraPos;

        Matrix4 _InverseProjectionMatrix;
        Matrix4 _InverseViewProjectionMatrix;
        Matrix4 _WorldToCameraMatrix;

        Vector4 _ProjectionParams;
        Vector4 _ZBufferParams;

        Vector4 _JitterSizeAndOffset;

        Matrix4 _ProjectionMatrix;

        uint32_t _RayCastSize[2];
        uint32_t _NoiseSize[2];
    };
    ConstantBuffer cbv = {
        camera.GetPosition(),

        Invert(camera.GetProjMatrix()),
        Invert(camera.GetViewProjMatrix()),
        camera.GetViewMatrix(),

        Vector4(1, camera.GetNearClip(), camera.GetFarClip(), 1 / camera.GetFarClip()),
        Vector4(-1 + camera.GetFarClip() / camera.GetNearClip(), 1, (-1 + camera.GetFarClip() / camera.GetNearClip()) / camera.GetFarClip(), 1 / camera.GetFarClip()),

        Vector4((float)g_SSSRRayCast.GetWidth() / (float)s_NoiseTexture->GetWidth(),
                    (float)g_SSSRRayCast.GetHeight() / (float)s_NoiseTexture->GetHeight(),
                    jitterSampleX,
                    jitterSampleY),

        camera.GetProjMatrix(),

        g_SSSRRayCast.GetWidth(), g_SSSRRayCast.GetHeight(),
        s_NoiseTexture->GetWidth(), s_NoiseTexture->GetHeight()
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    ColorBuffer& LinearDepth = g_LinearDepth[TemporalEffects::GetFrameIndexMod2()];

    Context.TransitionResource(g_SceneDiffuseOcclusionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneSpecularSmoothnessBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource((GpuResource&)*s_NoiseTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(LinearDepth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.TransitionResource(g_SSSRRayCast, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Context.TransitionResource(g_SSSRRayCastMask, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 0, g_SceneDiffuseOcclusionBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 1, g_SceneSpecularSmoothnessBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 2, g_SceneNormalBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 3, g_SceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(1, 4, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 5, s_NoiseTexture->GetSRV());
    Context.SetDynamicDescriptor(1, 6, LinearDepth.GetSRV());

    Context.SetDynamicDescriptor(2, 0, g_SSSRRayCast.GetUAV());
    Context.SetDynamicDescriptor(2, 1, g_SSSRRayCastMask.GetUAV());

    Context.Dispatch2D(g_SSSRRayCast.GetWidth(), g_SSSRRayCast.GetHeight());
}

void SSSR::Resolve(ComputeContext& Context, const Camera& camera, float jitterSampleX, float jitterSampleY)
{
    ScopedTimer _prof(L"Resolve", Context);

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_ResolveCS);

    double t = SystemTime::TicksToSeconds(SystemTime::GetCurrentTick());

    __declspec(align(16)) struct ConstantBuffer
    {
        Vector3 _WorldSpaceCameraPos;

        Matrix4 _InverseProjectionMatrix;
        Matrix4 _InverseViewProjectionMatrix;
        Matrix4 _WorldToCameraMatrix;

        Vector4 _SinTime; // sin(t/8), sin(t/4), sin(t/2), sin(t)

        Vector4 _JitterSizeAndOffset;

        uint32_t _ScreenSize[2];
        uint32_t _ResolveSize[2];
        uint32_t _NoiseSize[2];
    };
    ConstantBuffer cbv = {
        camera.GetPosition(),

        Invert(camera.GetProjMatrix()),
        Invert(camera.GetViewProjMatrix()),
        camera.GetViewMatrix(),

        Vector4(Sin(t/8), Sin(t / 4), Sin(t / 2), Sin(t)),

        Vector4((float)g_SSSRRayCast.GetWidth() / (float)s_NoiseTexture->GetWidth(),
                    (float)g_SSSRRayCast.GetHeight() / (float)s_NoiseTexture->GetHeight(),
                    jitterSampleX,
                    jitterSampleY),

        g_SceneDiffuseOcclusionBuffer.GetWidth(), g_SceneDiffuseOcclusionBuffer.GetHeight(),
        g_SSSRResolvePass.GetWidth(), g_SSSRResolvePass.GetHeight(),
        s_NoiseTexture->GetWidth(), s_NoiseTexture->GetHeight()
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    Context.TransitionResource(g_SceneDiffuseOcclusionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneSpecularSmoothnessBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource((GpuResource&)*s_NoiseTexture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRMipMapBuffer2, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRRayCast, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRRayCastMask, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.TransitionResource(g_SSSRResolvePass, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 0, g_SceneDiffuseOcclusionBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 1, g_SceneSpecularSmoothnessBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 2, g_SceneNormalBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 3, g_SceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(1, 4, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 5, s_NoiseTexture->GetSRV());
    Context.SetDynamicDescriptor(1, 6, g_SSSRMipMapBuffer2.GetSRV());
    Context.SetDynamicDescriptor(1, 7, g_SSSRRayCast.GetSRV());
    Context.SetDynamicDescriptor(1, 8, g_SSSRRayCastMask.GetSRV());

    Context.SetDynamicDescriptor(2, 0, g_SSSRResolvePass.GetUAV());

    Context.Dispatch2D(g_SSSRResolvePass.GetWidth(), g_SSSRResolvePass.GetHeight());
}

void SSSR::Temporal(ComputeContext& Context, const Camera& camera)
{
    ScopedTimer _prof(L"Temporal", Context);

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_TemporalCS);

    __declspec(align(16)) struct ConstantBuffer
    {
        Vector3 _WorldSpaceCameraPos;

        Matrix4 _InverseProjectionMatrix;
        Matrix4 _InverseViewProjectionMatrix;
        Matrix4 _WorldToCameraMatrix;

        uint32_t _ScreenSize[2];
    };
    ConstantBuffer cbv = {
        camera.GetPosition(),

        Invert(camera.GetProjMatrix()),
        Invert(camera.GetViewProjMatrix()),
        camera.GetViewMatrix(),

        g_SSSRTemporalBuffer.GetWidth(), g_SSSRTemporalBuffer.GetHeight(),
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRRayCast, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRTemporalBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRResolvePass, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.TransitionResource(g_SSSRTemporalBuffer0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 3, g_SceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(1, 4, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 5, g_SSSRRayCast.GetSRV());
    Context.SetDynamicDescriptor(1, 6, g_SSSRTemporalBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 7, g_SSSRResolvePass.GetSRV());

    Context.SetDynamicDescriptor(2, 0, g_SSSRTemporalBuffer0.GetUAV());

    Context.Dispatch2D(g_SSSRTemporalBuffer0.GetWidth(), g_SSSRTemporalBuffer0.GetHeight());

    Context.CopyBuffer(g_SSSRTemporalBuffer, g_SSSRTemporalBuffer0);
}

void SSSR::Combine(ComputeContext& Context, const Camera& camera)
{
    ScopedTimer _prof(L"Combine", Context);

    Context.SetRootSignature(s_RootSignature);
    Context.SetPipelineState(s_CombineCS);

    __declspec(align(16)) struct ConstantBuffer
    {
        Vector3 _WorldSpaceCameraPos;

        Matrix4 _InverseProjectionMatrix;
        Matrix4 _InverseViewProjectionMatrix;
        Matrix4 _WorldToCameraMatrix;

        uint32_t _ScreenSize[2];
    };
    ConstantBuffer cbv = {
        camera.GetPosition(),

        Invert(camera.GetProjMatrix()),
        Invert(camera.GetViewProjMatrix()),
        camera.GetViewMatrix(),

        g_SSSRTemporalBuffer.GetWidth(), g_SSSRTemporalBuffer.GetHeight(),
    };

    Context.SetDynamicConstantBufferView(3, sizeof(cbv), &cbv);

    Context.TransitionResource(g_SceneDiffuseOcclusionBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneSpecularSmoothnessBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneNormalBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Context.TransitionResource(g_SSSRTemporalBuffer0, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Context.TransitionResource(g_SSSRMainBuffer1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    Context.SetDynamicDescriptor(1, 0, g_SceneDiffuseOcclusionBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 1, g_SceneSpecularSmoothnessBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 2, g_SceneNormalBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 3, g_SceneDepthBuffer.GetDepthSRV());
    Context.SetDynamicDescriptor(1, 4, g_VelocityBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 5, g_SceneColorBuffer.GetSRV());
    Context.SetDynamicDescriptor(1, 6, g_SSSRTemporalBuffer0.GetSRV());

    Context.SetDynamicDescriptor(2, 0, g_SSSRMainBuffer1.GetUAV());

    Context.Dispatch2D(g_SSSRMainBuffer1.GetWidth(), g_SSSRMainBuffer1.GetHeight());

    Context.CopyBuffer(g_SceneColorBuffer, g_SSSRMainBuffer1);
}

void SSSR::DebugOverlay(CommandContext& BaseContext)
{
    //ScopedTimer _prof(L"DebugOverlay", BaseContext);

    //ComputeContext& Context = BaseContext.GetComputeContext();

    //Context.SetRootSignature(s_RootSignature);
    //Context.SetPipelineState(s_DebugOverlayCS);

    //ColorBuffer& copyBuffer = g_SSSRResolvePass;

    //Context.TransitionResource(copyBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    //Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    //Context.SetDynamicDescriptor(1, 0, copyBuffer.GetSRV());
    //Context.SetDynamicDescriptor(2, 0, g_SceneColorBuffer.GetUAV());

    //Context.Dispatch2D(copyBuffer.GetWidth() / 2, copyBuffer.GetHeight() / 2, 8, 8);
}
