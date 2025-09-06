#if defined(PSHADER)
// Debug mode:
// 0 = Normal output
// 1 = Motion vectors visualization
// 2 = History weight visualization
// 3 = Neighborhood bounds range visualization
// 4 = Luminance confidence visualization
// 5 = HDR intensity visualization

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

static uint localFrameCount = 0;

cbuffer ViewportConstants : register(b12)
{
	float4 viewportParams[45];
}

float GetHDRLuminance(float3 color)
{
	// Use Rec. 2020 luminance coefficients for HDR
	return dot(color, float3(0.2627, 0.6780, 0.0593));
}

float3 GetHDRNeighborhoodBounds(float2 uv, out float3 minColor, out float3 maxColor, out float3 debugInfo)
{
	// Sample center and initialize
	float2 centerCoord = viewportParams[43].xy * uv;
	centerCoord = clamp(centerCoord, float2(0, 0), float2(viewportParams[44].z, viewportParams[43].y));
	float3 centerColor = currentFrameTex.Sample(currentFrameSampler, centerCoord).xyz;

	float centerLum = GetHDRLuminance(centerColor);

	minColor = centerColor;
	maxColor = centerColor;

	// Sample 3x3 neighborhood with HDR-aware accumulation
	const float2 offset = taaParams[3].xy;
	float3 colorSum = centerColor;
	float3 colorSqSum = centerColor * centerColor;
	float lumSum = centerLum;
	float lumSqSum = centerLum * centerLum;

	[unroll] for (int i = -1; i <= 1; i++)
	{
		for (int j = -1; j <= 1; j++) {
			if (i == 0 && j == 0)
				continue;

			float2 sampleCoord = viewportParams[43].xy * (uv + float2(i, j) * offset);
			sampleCoord = clamp(sampleCoord, float2(0, 0), float2(viewportParams[44].z, viewportParams[43].y));
			float3 sampleColor = currentFrameTex.Sample(currentFrameSampler, sampleCoord).xyz;
			float sampleLum = GetHDRLuminance(sampleColor);

			colorSum += sampleColor;
			colorSqSum += sampleColor * sampleColor;
			lumSum += sampleLum;
			lumSqSum += sampleLum * sampleLum;

			minColor = min(minColor, sampleColor);
			maxColor = max(maxColor, sampleColor);
		}
	}

	// Calculate HDR-aware variance
	float3 mean = colorSum / 9.0;
	float3 variance = max(colorSqSum / 9.0 - mean * mean, 0.0);
	float3 stdDev = sqrt(variance);

	float lumMean = lumSum / 9.0;
	float lumVariance = max(lumSqSum / 9.0 - lumMean * lumMean, 0.0);
	float lumStdDev = sqrt(lumVariance);

	// Dynamic expansion based on HDR intensity
	float hdrScale = 1.0 + saturate(lumMean - 1.0) * 2.0;
	float3 boundsExpansion = stdDev * lerp(1.5, 3.0, saturate(lumMean - 1.0));
	boundsExpansion = max(boundsExpansion, mean * lerp(0.2, 0.4, saturate(lumMean - 1.0)));

	minColor = minColor - boundsExpansion * hdrScale;
	maxColor = maxColor + boundsExpansion * hdrScale;

	// Store debug information
	debugInfo = float3(lumMean, lumStdDev, hdrScale);

	return centerColor;
}

void main(
	float4 position : SV_POSITION0,
	float2 texCoord : TEXCOORD0,
	out float4 outColor : SV_Target0,
	out float4 outHistory : SV_Target1)
{
	float3 debugInfo;
	float3 minColor, maxColor;
	float3 centerColor = GetHDRNeighborhoodBounds(texCoord, minColor, maxColor, debugInfo);

	// Get motion and history
	float2 velocity = velocityTex.Sample(velocitySampler, texCoord).xy;
	float2 reprojectedUV = texCoord + velocity;

	float2 historyUV = viewportParams[43].zw * reprojectedUV;
	historyUV = clamp(historyUV, float2(0, 0), float2(viewportParams[44].w, viewportParams[43].w));
	float3 historyColor = historyTex.Sample(historySampler, historyUV).xyz;

	// HDR-aware motion adaptation
	float motionLength = length(velocity);
	float velocityScale = saturate(motionLength * lerp(128.0, 64.0, saturate(GetHDRLuminance(centerColor) - 1.0)));

	// HDR-aware confidence calculation
	float centerLuminance = GetHDRLuminance(centerColor);
	float historyLuminance = GetHDRLuminance(historyColor);
	float lumRatio = max(centerLuminance, historyLuminance) / (min(centerLuminance, historyLuminance) + 0.0001);
	float confidence = 1.0 - saturate((lumRatio - 1.0) * lerp(0.5, 0.25, saturate(max(centerLuminance, historyLuminance) - 1.0)));

	// Clamp history with HDR awareness
	historyColor = clamp(historyColor, minColor, maxColor);

	// Adaptive blending for HDR
	float baseHistoryWeight = lerp(0.95, 0.85, velocityScale);
	float historyWeight = baseHistoryWeight * confidence;

	// HDR-specific weight adjustment
	if (centerLuminance > 1.0) {
		float hdrFactor = saturate(centerLuminance - 1.0);
		historyWeight *= lerp(1.0, 0.5, hdrFactor);
	}

	float3 finalColor = lerp(centerColor, historyColor, historyWeight);

	float3 debugColor = finalColor;  // Default to normal output

	// Add frame counter to corner
	bool isFrameCounter = position.x < 60 && position.y < 20;

	switch ((localFrameCount / 10000) % 7) {
	case 1:
		debugColor = isFrameCounter ? float3(1, 0, 0) : float3(velocity * 10.0, 0);  // Red + motion vectors
		break;
	case 2:
		debugColor = isFrameCounter ? float3(0, 1, 0) : float3(historyWeight, historyWeight, historyWeight);  // Green + weights
		break;
	case 3:
		debugColor = isFrameCounter ? float3(0, 0, 1) : (maxColor - minColor) * 0.5;  // Blue + bounds
		break;
	case 4:
		debugColor = isFrameCounter ? float3(1, 1, 0) : float3(confidence, confidence, confidence);  // Yellow + confidence
		break;
	case 5:
		debugColor = isFrameCounter ? float3(1, 0, 1) : debugInfo;  // Purple + HDR info
		break;
	case 6:
		// Custom debug visualization
		debugColor = isFrameCounter                                                ? float3(0, 1, 1) :
		             (position.x > (viewportParams[44].z - 20) && position.y < 20) ? float3(1, 1, 1) :
		                                                                             finalColor;
		break;
	}

	finalColor = debugColor;

	outColor = float4(finalColor, 1.0);
	outHistory = float4(finalColor, 1.0);

	// Increment at the end of the shader
	localFrameCount++;
}

#else
#	include "Common/DummyVSTexCoord.hlsl"
#endif