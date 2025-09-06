// RT Contact Shadows - Raygen Shader
// Uses DirectX 12 Hardware Raytracing for accurate contact shadows

#include "Common/FrameBuffer.hlsli"
#include "Common/SharedData.hlsli"

// Global constant buffer for RT Contact Shadows
cbuffer RTContactShadowsCB : register(b1)
{
    float Intensity;
    float MaxDistance;
    uint MaxSteps;
    uint FrameIndex;
    float2 ScreenSize;
    float2 Padding;
};

// Input textures
Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> NormalTexture : register(t1);

// Output texture
RWTexture2D<float> ContactShadowsTexture : register(u0);

// Raytracing acceleration structure
RaytracingAccelerationStructure Scene : register(t2);

// Ray payload for contact shadow rays
struct ContactShadowPayload
{
    float shadowFactor;
};

[shader("raygeneration")]
void ContactShadowRayGen()
{
    uint2 screenPos = DispatchRaysIndex().xy;
    float2 screenSize = DispatchRaysDimensions().xy;
    
    // Early exit if outside screen bounds
    if (any(screenPos >= screenSize))
        return;
    
    // Convert screen position to UV coordinates
    float2 uv = (screenPos + 0.5) / screenSize;
    
    // Sample depth and normal
    float depth = DepthTexture.SampleLevel(0, uv, 0);
    float4 normalSample = NormalTexture.SampleLevel(0, uv, 0);
    
    // Skip background pixels
    if (depth >= 1.0)
    {
        ContactShadowsTexture[screenPos] = 1.0;
        return;
    }
    
    // Reconstruct world position
    float3 worldPos = ScreenToWorld(uv, depth, 0);
    float3 worldNormal = normalize(normalSample.xyz * 2.0 - 1.0);
    
    // Get primary light direction (sun)
    float3 lightDir = SharedData::LightDirection.xyz;
    
    // Setup shadow ray
    RayDesc ray;
    ray.Origin = worldPos + worldNormal * 0.01; // Bias to avoid self-intersection
    ray.Direction = -lightDir;
    ray.TMin = 0.01;
    ray.TMax = MaxDistance;
    
    // Trace shadow ray
    ContactShadowPayload payload = { 1.0 }; // Default to no shadow
    
    TraceRay(Scene, 
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
             0xFF, // Instance mask
             0,    // Ray contribution to hit group index
             0,    // Multiplier for geometry contribution to hit group index
             0,    // Miss shader index
             ray,
             payload);
    
    // Apply intensity and write result
    float shadowValue = lerp(1.0, payload.shadowFactor, Intensity);
    ContactShadowsTexture[screenPos] = shadowValue;
}

[shader("miss")]
void ContactShadowMiss(inout ContactShadowPayload payload)
{
    // Ray missed all geometry - no shadow
    payload.shadowFactor = 1.0;
}

[shader("anyhit")]
void ContactShadowAnyHit(inout ContactShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Hit geometry - in shadow
    payload.shadowFactor = 0.0;
}

// Helper function to convert screen coordinates to world position
float3 ScreenToWorld(float2 uv, float depth, uint eyeIndex)
{
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 viewPos = mul(FrameBuffer::CameraProjInverse[eyeIndex], clipPos);
    viewPos /= viewPos.w;
    
    float4 worldPos = mul(FrameBuffer::CameraViewInverse[eyeIndex], viewPos);
    return worldPos.xyz;
}