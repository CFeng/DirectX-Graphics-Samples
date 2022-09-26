
#include "UnityStandardBRDF.hlsli"
#include "PixelPacking_Velocity.hlsli"

#define PI 3.141592

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);
SamplerState AnisoWrapSampler : register(s2);

Texture2D<float4> InDiffuseOcclusion : register(t0);
Texture2D<float4> InSpecularSmoothness : register(t1);
Texture2D<float3> InNormal : register(t2);
Texture2D<float> InDepth : register(t3);
Texture2D<packed_velocity_t> _CameraMotionVectorsTexture : register(t4);

//cbuffer CB0 : register(b0)
//{
//	float4x4 WorldToCameraMatrix;
//}

float sqr(float x)
{
	return x * x;
}

float fract(float x)
{
	return x - floor(x);
}

float4 GetAlbedo(float2 uv)
{
	return InDiffuseOcclusion.SampleLevel(PointSampler, uv, 0);
}

float4 GetSpecular(float2 uv)
{
	return InSpecularSmoothness.SampleLevel(PointSampler, uv, 0);
}

float GetRoughness(float smoothness)
{
	return max(1 - smoothness, 0.05f);
}

float3 GetNormal(float2 uv)
{
	return InNormal.SampleLevel(PointSampler, uv, 0);
}

float3 GetVelocity(uint2 st)
{
	return UnpackVelocity(_CameraMotionVectorsTexture[st]);
}

float3 GetViewNormal(float3 normal)
{
	float3 viewNormal = mul((float3x3)_WorldToCameraMatrix, normal);
	return normalize(viewNormal);
}

float GetDepth(float2 uv)
{
	float z = InDepth.SampleLevel(PointSampler, uv, 0);
//#if defined(UNITY_REVERSED_Z)
	z = 1.0f - z;
//#endif
	return z;
}

float3 GetScreenPos(float2 uv, float depth)
{
	//return float3(uv.xy * 2 - 1, depth);
	return float3(uv.x * 2 - 1, (1 - uv.y) * 2 - 1, depth);	// HACK!!
}

float3 GetWorlPos(float3 screenPos)
{
	float4 worldPos = mul(_InverseViewProjectionMatrix, float4(screenPos, 1));
	return worldPos.xyz / worldPos.w;
}

float3 GetViewPos(float3 screenPos)
{
	float4 viewPos = mul(_InverseProjectionMatrix, float4(screenPos, 1));
	return viewPos.xyz / viewPos.w;
}

float3 GetViewDir(float3 worldPos)
{
	return normalize(worldPos - _WorldSpaceCameraPos);
}
