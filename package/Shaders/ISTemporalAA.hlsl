#if defined(PSHADER)
// Input textures for temporal AA
Texture2D<float4> motionVectorTex : register(t5);
Texture2D<float4> previousFrameTex : register(t4);
Texture2D<float4> depthTex : register(t3);
Texture2D<float4> velocityTex : register(t2);
Texture2D<float4> historyTex : register(t1);
Texture2D<float4> currentFrameTex : register(t0);

// Samplers
SamplerState motionVectorSampler : register(s5);
SamplerState previousFrameSampler : register(s4);
SamplerState depthSampler : register(s3);
SamplerState velocitySampler : register(s2);
SamplerState historySampler : register(s1);
SamplerState currentFrameSampler : register(s0);

// Constant buffers
cbuffer TAAConstants : register(b2)
{
    float4 taaParams[6];
}

cbuffer ViewportConstants : register(b12)
{
    float4 viewportParams[45];
}

#define NEG -

float3 GetNeighborhoodBounds(float2 uv, out float3 minColor, out float3 maxColor)
{
    // Sample 3x3 neighborhood
    const float2 offset = taaParams[3].xy;
    float2 samplesUV[9];
    samplesUV[0] = uv + float2(-offset.x, -offset.y);
    samplesUV[1] = uv + float2(0, -offset.y);
    samplesUV[2] = uv + float2(offset.x, -offset.y);
    samplesUV[3] = uv + float2(-offset.x, 0);
    samplesUV[4] = uv;
    samplesUV[5] = uv + float2(offset.x, 0);
    samplesUV[6] = uv + float2(-offset.x, offset.y);
    samplesUV[7] = uv + float2(0, offset.y);
    samplesUV[8] = uv + float2(offset.x, offset.y);

    // Initialize bounds
    minColor = float3(1.0f, 1.0f, 1.0f);
    maxColor = float3(0.0f, 0.0f, 0.0f);
    float3 centerColor = float3(0.0f, 0.0f, 0.0f);

    // Find min/max color in neighborhood
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        float2 sampleCoord = viewportParams[43].xy * samplesUV[i];
        sampleCoord = clamp(sampleCoord, float2(0,0), float2(viewportParams[44].z, viewportParams[43].y));
        float3 sampleColor = currentFrameTex.Sample(currentFrameSampler, sampleCoord).yxz;

        minColor = min(minColor, sampleColor);
        maxColor = max(maxColor, sampleColor);

        if (i == 4) // Center sample
            centerColor = sampleColor;
    }

    return centerColor;
}

float GetLuminance(float3 color)
{
    return dot(color.xzy, float3(0.5, 0.25, 0.25));
}

void main(
    float4 position : SV_POSITION0,
    float2 texCoord : TEXCOORD0,
    out float4 outColor : SV_Target0,
    out float4 outHistory : SV_Target1)
{
    // Get color bounds from neighborhood
    float3 minColor, maxColor;
    float3 centerColor = GetNeighborhoodBounds(texCoord, minColor, maxColor);

    // Sample motion vectors and calculate reprojection
    float2 velocity = velocityTex.Sample(velocitySampler, texCoord).xy;
    float2 reprojectedUV = texCoord + velocity;

    // Sample history buffer
    float2 historyUV = viewportParams[43].zw * reprojectedUV;
    historyUV = clamp(historyUV, float2(0,0), float2(viewportParams[44].w, viewportParams[43].w));
    float3 historyColor = historyTex.Sample(historySampler, historyUV).xyz;

    // Calculate blend factor based on motion
    float motionLength = length(velocity);
    float velocityScale = saturate(motionLength * 128.0); // Scale factor for velocity-based blending

    // Clamp history color to neighborhood bounds
    historyColor = clamp(historyColor, minColor, maxColor);

    // Calculate adaptive blend weight
    float historyWeight = lerp(0.95, 0.85, velocityScale); // Less weight when fast motion

    // Perform temporal blend
    float3 finalColor = lerp(historyColor, centerColor, 1 - historyWeight);

    // Output results
    outColor = float4(finalColor, 1.0);
    outHistory = float4(finalColor, 1.0);
}

#else
#include "Common/DummyVSTexCoord.hlsl"
#endif