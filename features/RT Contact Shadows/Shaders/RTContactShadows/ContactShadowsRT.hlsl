// RT Contact Shadows - Hardware Raytracing Implementation
// Uses DirectX 12 Ultimate for accurate contact shadows between objects

#include "Common/FrameBuffer.hlsli"
#include "Common/SharedData.hlsli"

// Global constant buffer for RT Contact Shadows
cbuffer RTContactShadowsCB : register(b1)
{
	float Intensity;
	float MaxDistance;
	uint MaxSteps;
	uint FrameIndex;
	float ScreenSizeX;
	float ScreenSizeY;
	float PaddingX;
	float PaddingY;
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
	bool hit;
};

[shader("raygeneration")] void ContactShadowRayGen() {
	uint2 screenPos = DispatchRaysIndex().xy;
	uint2 screenSize = DispatchRaysDimensions().xy;

	// Early exit if outside screen bounds
	if (any(screenPos >= screenSize))
		return;

	// Convert screen position to UV coordinates
	float2 uv = (screenPos + 0.5) / float2(screenSize);

	// Sample depth and normal from G-buffer
	float depth = DepthTexture.SampleLevel(PointSampler, uv, 0);
	float4 normalSample = NormalTexture.SampleLevel(PointSampler, uv, 0);

	// Skip background/sky pixels
	if (depth >= 0.999999) {
		ContactShadowsTexture[screenPos] = 1.0;
		return;
	}

	// Reconstruct world position from depth
	float3 worldPos = ScreenToWorld(uv, depth, 0);
	float3 worldNormal = normalize(normalSample.xyz * 2.0 - 1.0);

	// Get primary light direction (sun/directional light)
	float3 lightDir = normalize(-SharedData::LightDirection.xyz);

	// Calculate light attenuation based on distance
	float lightDistance = length(SharedData::LightDirection.xyz);
	float attenuation = 1.0 / (1.0 + lightDistance * lightDistance * 0.001);

	// Skip pixels facing away from light
	float NoL = dot(worldNormal, lightDir);
	if (NoL <= 0.01) {
		ContactShadowsTexture[screenPos] = 0.0;  // Fully shadowed for back-facing surfaces
		return;
	}

	// Setup contact shadow ray
	RayDesc ray;
	ray.Origin = worldPos + worldNormal * 0.01;  // Small bias to avoid self-intersection
	ray.Direction = lightDir;
	ray.TMin = 0.01;
	ray.TMax = min(MaxDistance, lightDistance * 0.5);  // Don't trace beyond reasonable contact distance

	// Initialize payload
	ContactShadowPayload payload;
	payload.shadowFactor = 1.0;  // Default to no shadow (lit)
	payload.hit = false;

	// Trace shadow ray with early termination
	TraceRay(Scene,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
			RAY_FLAG_SKIP_CLOSEST_HIT_SHADER |
			RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
		0xFF,  // Instance mask - hit all objects
		0,     // Ray contribution to hit group index
		0,     // Multiplier for geometry contribution
		0,     // Miss shader index
		ray,
		payload);

	// Apply intensity scaling and NoL factor
	float shadowValue = lerp(1.0, payload.shadowFactor, Intensity * NoL * attenuation);

	// Write result to contact shadow texture
	ContactShadowsTexture[screenPos] = shadowValue;
}

	[shader("miss")] void ContactShadowMiss(inout ContactShadowPayload payload)
{
	// Ray missed all geometry - no contact shadow, fully lit
	payload.shadowFactor = 1.0;
	payload.hit = false;
}

[shader("anyhit")] void ContactShadowAnyHit(inout ContactShadowPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
	// Hit geometry - create contact shadow
	payload.shadowFactor = 0.0;
	payload.hit = true;

	// Accept hit and terminate ray
	AcceptHitAndEndSearch();
}

// Helper function to convert screen coordinates to world position
float3 ScreenToWorld(float2 uv, float depth, uint eyeIndex)
{
	// Convert UV to normalized device coordinates
	float2 ndc = uv * 2.0 - 1.0;
	ndc.y = -ndc.y;  // Flip Y for DirectX

	// Reconstruct clip space position
	float4 clipPos = float4(ndc, depth, 1.0);

	// Transform to view space
	float4 viewPos = mul(FrameBuffer::CameraProjInverse[eyeIndex], clipPos);
	viewPos /= viewPos.w;

	// Transform to world space
	float4 worldPos = mul(FrameBuffer::CameraViewInverse[eyeIndex], viewPos);
	return worldPos.xyz;
}

// Sample point sampler for G-buffer textures
SamplerState PointSampler : register(s0);