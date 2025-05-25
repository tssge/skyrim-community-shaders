#include "Common/Color.hlsli"

Texture2D<float4> Framebuffer : register(t0);
RWTexture2D<float4> HDROutput : register(u0);

[numthreads(8, 8, 1)] void main(uint3 dispatchID
: SV_DispatchThreadID) {
	float4 framebuffer = Framebuffer[dispatchID.xy];

	// Linearize the incoming HDR buffer
	float3 untonemapped = Color::GammaToLinearSafe(framebuffer.xyz);

	float3 bt2020Color = Color::BT709ToBT2020(untonemapped);
	float3 pqColor = Color::pq::Encode(bt2020Color, 450.0);

	HDROutput[dispatchID.xy] = float4(pqColor, framebuffer.w);
}
