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

Texture2D<float4> _RayCast : register(t5);
Texture2D<float4> _PreviousBuffer : register(t6);
Texture2D<float4> _MainTex : register(t7);

RWTexture2D<float4> OutColor : register(u0);

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

	float depth = GetDepth(uv);
	float3 screenPos = GetScreenPos(uv, depth);
	float4 hitPacked = _RayCast.SampleLevel(PointSampler, uv, 0);

	float2 velocity;
//	if (_ReflectionVelocity == 1)
//	{
//#if defined(UNITY_REVERSED_Z)
//		hitPacked.z = 1.0f - hitPacked.z;
//#endif
//		velocity = CalculateMotion(hitPacked.z, uv); // Reflection Depth derived motion. Removes smudghing cause by screen motion vectors.
//	}
//	else
		velocity = GetVelocity(pos) / _ScreenSize; // 5.4 motion vector

	float2 prevUV = uv - velocity;

	float4 current = _MainTex.SampleLevel(PointSampler, uv, 0);
	float4 previous = _PreviousBuffer.SampleLevel(PointSampler, prevUV, 0);

	float2 du = float2(1.0 / _ScreenSize.x, 0.0);
	float2 dv = float2(0.0, 1.0 / _ScreenSize.y);

	float4 currentTopLeft = _MainTex.SampleLevel(PointSampler, uv.xy - dv - du, 0);
	float4 currentTopCenter = _MainTex.SampleLevel(PointSampler, uv.xy - dv, 0);
	float4 currentTopRight = _MainTex.SampleLevel(PointSampler, uv.xy - dv + du, 0);
	float4 currentMiddleLeft = _MainTex.SampleLevel(PointSampler, uv.xy - du, 0);
	float4 currentMiddleCenter = _MainTex.SampleLevel(PointSampler, uv.xy, 0);
	float4 currentMiddleRight = _MainTex.SampleLevel(PointSampler, uv.xy + du, 0);
	float4 currentBottomLeft = _MainTex.SampleLevel(PointSampler, uv.xy + dv - du, 0);
	float4 currentBottomCenter = _MainTex.SampleLevel(PointSampler, uv.xy + dv, 0);
	float4 currentBottomRight = _MainTex.SampleLevel(PointSampler, uv.xy + dv + du, 0);

	float4 currentMin = min(currentTopLeft, min(currentTopCenter, min(currentTopRight, min(currentMiddleLeft, min(currentMiddleCenter, min(currentMiddleRight, min(currentBottomLeft, min(currentBottomCenter, currentBottomRight))))))));
	float4 currentMax = max(currentTopLeft, max(currentTopCenter, max(currentTopRight, max(currentMiddleLeft, max(currentMiddleCenter, max(currentMiddleRight, max(currentBottomLeft, max(currentBottomCenter, currentBottomRight))))))));

	float scale = 2.0;	// _TScale;

	float4 center = (currentMin + currentMax) * 0.5f;
	currentMin = (currentMin - center) * scale + center;
	currentMax = (currentMax - center) * scale + center;

	previous = clamp(previous, currentMin, currentMax);

	OutColor[pos] = lerp(current, previous, saturate(0.85 * (1 - length(velocity) * 8)));	// _TResponse = 0.85
}
