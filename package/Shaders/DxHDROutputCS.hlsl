// Based on the Pixel Shaders from the DirectX Toolkit
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929

Texture2D<float4> Framebuffer : register(t0);
RWTexture2D<float4> HDROutput : register(u0);

cbuffer PerFrame : register(b0)
{
	float linearExposure : packoffset(c0.x);
	float paperWhiteNits : packoffset(c0.y);
	float tonemapSelector : packoffset(c0.z);
	float4x3 colorRotation : packoffset(c1);
}

// sRGB
// https://en.wikipedia.org/wiki/SRGB

// Apply the (approximate) sRGB curve to linear values
static float3 LinearToSRGBEst(float3 color)
{
	return pow(abs(color), 1 / 2.2f);
}

// (Approximate) sRGB to linear
static float3 SRGBToLinearEst(float3 srgb)
{
	return pow(abs(srgb), 2.2f);
}

// HDR10 Media Profile
// https://en.wikipedia.org/wiki/High-dynamic-range_video#HDR10

// Apply the ST.2084 curve to normalized linear values and outputs normalized non-linear values
static float3 LinearToST2084(float3 normalizedLinearValue)
{
	return pow((0.8359375f + 18.8515625f * pow(abs(normalizedLinearValue), 0.1593017578f)) / (1.0f + 18.6875f * pow(abs(normalizedLinearValue), 0.1593017578f)), 78.84375f);
}

// ST.2084 to linear, resulting in a linear normalized value
static float3 ST2084ToLinear(float3 ST2084)
{
	return pow(max(pow(abs(ST2084), 1.0f / 78.84375f) - 0.8359375f, 0.0f) / (18.8515625f - 18.6875f * pow(abs(ST2084), 1.0f / 78.84375f)), 1.0f / 0.1593017578f);
}

// Reinhard tonemap operator
// Reinhard et al. "Photographic tone reproduction for digital images." ACM Transactions on Graphics. 21. 2002.
// http://www.cs.utah.edu/~reinhard/cdrom/tonemap.pdf
float3 ToneMapReinhard(float3 color)
{
	return color / (1.0f + color);
}

// ACES Filmic tonemap operator
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ToneMapACESFilmic(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

//--------------------------------------------------------------------------------------
// Pass-through
float4 CS_Copy(float4 bufferIn)
{
	return bufferIn;
}

// Saturate (clips above 1.0)
float4 CS_Saturate(float4 bufferIn)
{
	float3 sdr = saturate(bufferIn.xyz * linearExposure);
	return float4(sdr, bufferIn.a);
}

// Reinhard operator
float4 CS_Reinhard(float4 bufferIn)
{
	float3 sdr = ToneMapReinhard(bufferIn.xyz * linearExposure);
	return float4(sdr, bufferIn.a);
}

// ACES filmic operator
float4 CS_ACESFilmic(float4 bufferIn)
{
	float3 sdr = ToneMapACESFilmic(bufferIn.xyz * linearExposure);
	return float4(sdr, bufferIn.a);
}

//--------------------------------------------------------------------------------------
// SRGB, using Rec.709 color primaries and a gamma 2.2 curve

// sRGB
float4 CS_SRGB(float4 bufferIn)
{
	float3 srgb = LinearToSRGBEst(bufferIn.xyz);
	return float4(srgb, bufferIn.a);
}

// Saturate (clips above 1.0)
float4 CS_Saturate_SRGB(float4 bufferIn)
{
	float3 sdr = saturate(bufferIn.xyz * linearExposure);
	float3 srgb = LinearToSRGBEst(sdr);
	return float4(srgb, bufferIn.a);
}

// Reinhard operator
float4 CS_Reinhard_SRGB(float4 bufferIn)
{
	float3 sdr = ToneMapReinhard(bufferIn.xyz * linearExposure);
	float3 srgb = LinearToSRGBEst(sdr);
	return float4(srgb, bufferIn.a);
}

// ACES filmic operator
float4 CS_ACESFilmic_SRGB(float4 bufferIn)
{
	float3 sdr = ToneMapACESFilmic(bufferIn.xyz * linearExposure);
	float3 srgb = LinearToSRGBEst(sdr);
	return float4(srgb, bufferIn.a);
}

// Non-DXTK Tonemapping
float4 CS_Reinhard_Jodie(float4 bufferIn)
{
	float3 exposedColor = bufferIn.xyz * linearExposure;
	float l = dot(exposedColor, float3(0.2126f, 0.7152f, 0.0722f));
	float3 tv = exposedColor / (1.0f + exposedColor);
	return float4(lerp(exposedColor / (1.0f + l), tv, tv), bufferIn.a);
}

float4 CS_Reinhard_Jodie_SRGB(float4 bufferIn)
{
	float3 exposedColor = bufferIn.xyz * linearExposure;
	float l = dot(exposedColor, float3(0.2126f, 0.7152f, 0.0722f));
	float3 tv = exposedColor / (1.0f + exposedColor);
	float3 srgb = LinearToSRGBEst(lerp(exposedColor / (1.0f + l), tv, tv));
	return float4(srgb, bufferIn.a);
}


float3 Uncharted2_Tonemap_Partial(float3 bufferIn)
{
	const float A = 0.15f;
	const float B = 0.50f;
	const float C = 0.10f;
	const float D = 0.20f;
	const float E = 0.02f;
	const float F = 0.30f;
	return ((bufferIn * (A * bufferIn + C * B) + D * E) / (bufferIn * (A * bufferIn + B) + D * F)) - E / F;
}

float4 CS_Uncharted2Filmic(float4 bufferIn)
{
	float3 curr = Uncharted2_Tonemap_Partial(bufferIn.xyz * linearExposure);
	const float3 W = 11.2f;
	float3 white_scale = 1.0f / Uncharted2_Tonemap_Partial(W);
	return float4(curr * white_scale, bufferIn.a);
}

float4 CS_Uncharted2Filmic_SRGB(float4 bufferIn)
{
	float3 curr = Uncharted2_Tonemap_Partial(bufferIn.xyz * linearExposure);
	const float3 W = 11.2f;
	float3 white_scale = 1.0f / Uncharted2_Tonemap_Partial(W);
	float3 srgb = LinearToSRGBEst(curr * white_scale);
	return float4(srgb, bufferIn.a);
}

[numthreads(8, 8, 1)] void main(uint3 dispatchID
								: SV_DispatchThreadID) {
	float4 framebuffer = Framebuffer[dispatchID.xy];
	framebuffer = float4(mul(framebuffer.xyz, (float3x3)colorRotation), framebuffer.a);

	switch ((int)tonemapSelector) {
	default:
	case 0:
		framebuffer = CS_Copy(framebuffer);
		break;
	case 1:
		framebuffer = CS_Saturate(framebuffer);
		break;
	case 2:
		framebuffer = CS_Reinhard(framebuffer);
		break;
	case 3:
		framebuffer = CS_Reinhard_Jodie(framebuffer);
		break;
	case 4:
		framebuffer = CS_ACESFilmic(framebuffer);
		break;
	case 5:
		framebuffer = CS_Uncharted2Filmic(framebuffer);
		break;
	case 6:
		framebuffer = CS_SRGB(framebuffer);
		break;
	case 7:
		framebuffer = CS_Saturate_SRGB(framebuffer);
		break;
	case 8:
		framebuffer = CS_Reinhard_SRGB(framebuffer);
		break;
	case 9:
		framebuffer = CS_Reinhard_Jodie_SRGB(framebuffer);
		break;
	case 10:
		framebuffer = CS_ACESFilmic_SRGB(framebuffer);
		break;
	case 11:
		framebuffer = CS_Uncharted2Filmic_SRGB(framebuffer);
		break;
	}

	HDROutput[dispatchID.xy] = framebuffer;
}
