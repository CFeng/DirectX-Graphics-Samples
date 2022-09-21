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

Texture2D<float4> _MainTex : register(t5);
Texture2D<float4> _ReflectionBuffer : register(t6);

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
	float3 worldPos = GetWorlPos(screenPos);

	float3 cubemap = 0;	// GetCubeMap(uv);
	float3 worldNormal = GetNormal(uv);

	float4 diffuse = GetAlbedo(uv);
	float occlusion = diffuse.a;
	float4 specular = GetSpecular(uv);
	float roughness = GetRoughness(specular.a);

	float4 sceneColor = _MainTex.SampleLevel(PointSampler, uv, 0);
	sceneColor.rgb = max(1e-5, sceneColor.rgb - cubemap.rgb);

	float4 reflection = _ReflectionBuffer.SampleLevel(PointSampler, uv, 0);

	float3 viewDir = GetViewDir(worldPos);
	float NdotV = saturate(dot(worldNormal, -viewDir));

	float3 reflDir = normalize(reflect(-viewDir, worldNormal));
	float fade = saturate(dot(-viewDir, reflDir) * 2.0);
	float mask = sqr(reflection.a) /* fade*/;

	float oneMinusReflectivity;
	diffuse.rgb = EnergyConservationBetweenDiffuseAndSpecular(diffuse, specular.rgb, oneMinusReflectivity);

	UnityLight light;
	light.color = 0;
	light.dir = 0;
	light.ndotl = 0;

	UnityIndirect ind;
	ind.diffuse = 0;
	ind.specular = reflection;

	//if (_UseFresnel == 1)
		reflection.rgb = BRDF2_Unity_PBS(0, specular.rgb, oneMinusReflectivity, 1 - roughness, worldNormal, -viewDir, light, ind).rgb;

	reflection.rgb *= occlusion;

	sceneColor.rgb += lerp(cubemap.rgb, reflection.rgb, mask); // Combine reflection and cubemap and add it to the scene color 

	OutColor[pos] = sceneColor;
}
