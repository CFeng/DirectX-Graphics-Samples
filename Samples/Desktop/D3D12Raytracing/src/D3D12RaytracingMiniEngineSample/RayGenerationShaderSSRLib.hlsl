//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#define HLSL
#include "ModelViewerRaytracing.h"
#include "../../../../../MiniEngine/Core/Shaders/PixelPacking_Velocity.hlsli"

Texture2D<float>    depth    : register(t12);
Texture2D<float4>   normals  : register(t13);
Texture2D<packed_velocity_t> VelocityBuffer : register(t14);
Texture2D<float4>   prevColor : register(t15);
Texture2D<float>    prevDepth : register(t16);

[shader("raygeneration")]
void RayGen()
{
    uint2 DTid = DispatchRaysIndex().xy;
    float2 xy = DTid.xy + 0.5;

    // Screen position for the ray
    float2 screenPos = xy / g_dynamic.resolution * 2.0 - 1.0;

    // Invert Y for DirectX-style coordinates
    screenPos.y =  -screenPos.y;

    float2 readGBufferAt = xy;

    // Read depth and normal
    float sceneDepth = depth.Load(int3(readGBufferAt, 0));
    float4 normalData = normals.Load(int3(readGBufferAt, 0));
    if (normalData.w == 0.0)
        return;

#ifdef VALIDATE_NORMAL
    // Check if normal is real and non-zero
    float lenSq = dot(normalData.xyz, normalData.xyz);
    if (!isfinite(lenSq) || lenSq < 1e-6)
        return;
    float3 normal = normalData.xyz * rsqrt(lenSq);
#else
    float3 normal = normalData.xyz;
#endif

    // Unproject into the world position using depth
    float4 unprojected = mul(g_dynamic.cameraToWorld, float4(screenPos, sceneDepth, 1));
    float3 world = unprojected.xyz / unprojected.w;

    float3 primaryRayDirection = normalize(g_dynamic.worldCameraPosition - world);

    // R
    float3 direction = normalize(-primaryRayDirection - 2 * dot(-primaryRayDirection, normal) * normal);
    float3 origin = world - direction * 0.1f;     // Lift off the surface a bit

    RayDesc rayDesc = { origin,
        0.0f,
        direction,
        FLT_MAX };

    RayPayload payload;
    payload.SkipShading = false;
    payload.RayHitT = FLT_MAX;
    TraceRay(g_accel, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0,0,1,0, rayDesc, payload);

    if (payload.RayHitT < FLT_MAX)
    {
        float4 camPos = mul(g_dynamic.worldToCamera, float4(payload.WorldPosition, 1));
        camPos /= camPos.w;

        float2 ST = (camPos.xy + 1.0) * 0.5 * g_dynamic.resolution - 0.5;
        ST.y = g_dynamic.resolution.y - ST.y;

        float f = step(0, ST.x) * step(0, ST.y) * step(ST.x, g_dynamic.resolution.x - 1) * step(ST.y, g_dynamic.resolution.y - 1);

        //g_screenOutput[DTid] = float4((camPos.xy + 1.0) * 0.5, 0, 1);
        //if (ST.x >= 0 && ST.x < g_dynamic.resolution)
        //{
        float3 Velocity = UnpackVelocity(VelocityBuffer[ST]);

        float temporalDepth = prevDepth[ST + Velocity.xy] + Velocity.z;
        //float4 temporalColor = prevColor[DTid + Velocity.xy];

        f *= step(temporalDepth - 0.001, camPos.z);

        float reflectivity = normals[DTid].w;
        float3 outputColor = g_screenOutput[DTid].rgb + reflectivity * prevColor[ST + Velocity.xy].rgb;

        g_screenOutput[DTid] = float4(f * outputColor + (1 - f) * payload.OutputColor, 1.0);
        //}
        //g_screenOutput[DTid] = float4(payload.OutputColor, 1.0);
    }
}

