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

#include "SSSRRS.hlsli"

RWTexture2D<float4> OutColor : register(u0);

Texture2D<float4> _MainTex : register(t5);

cbuffer CB1 : register(b1)
{
    float3 _WorldSpaceCameraPos;

    float4x4 _InverseProjectionMatrix;
    float4x4 _InverseViewProjectionMatrix;
    float4x4 _WorldToCameraMatrix;

    uint2 _ScreenSize;	// width, height
}

#include "SSRLib.hlsli"

[RootSignature(SSSR_RootSig)]
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint2 pos = DTid.xy;
    float2 uv = (pos + 0.5) / _ScreenSize;  // STtoUV, TODO: use rcp to save the "/"?

    uint2 ST = DTid.xy;
    float3 Velocity = GetVelocity(ST);
    uint2 prevST = ST - uint2(Velocity.xy);
    OutColor[ST] = _MainTex[prevST];
}
