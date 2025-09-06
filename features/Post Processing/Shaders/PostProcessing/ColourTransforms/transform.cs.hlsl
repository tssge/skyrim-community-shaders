/// By ProfJack/五脚猫, 2024-2-17 UTC

#include "PostProcessing/common.hlsli"

#define PI 3.1415926535

RWTexture2D<float4> RWTexOut : register(u0);

Texture2D<float4> TexColor : register(t0);

cbuffer TonemapCB : register(b1)
{
	float4 Params[8];
};

/////////////////////////////////////////////////////////////////////////////////

// https://www.shadertoy.com/view/ss23DD
float3 LiftGammaGain(float3 rgb, float4 lift, float4 gamma, float4 gain)
{
	float4 liftt = 1.0 - pow(1.0 - lift, log2(gain + 1.0));

	float4 gammat = gamma.rgba - float4(0.0, 0.0, 0.0, Color::RGBToLuminance(gamma.rgb));
	float4 gammatTemp = 1.0 + 4.0 * abs(gammat);
	gammat = lerp(gammatTemp, 1.0 / gammatTemp, step(0.0, gammat));

	float3 col = rgb;
	float luma = Color::RGBToLuminance(col);

	col = pow(col, gammat.rgb);
	col *= pow(gain.rgb, gammat.rgb);
	col = max(lerp(2.0 * liftt.rgb, 1.0, col), 0.0);

	luma = pow(luma, gammat.a);
	luma *= pow(gain.a, gammat.a);
	luma = max(lerp(2.0 * liftt.a, 1.0, luma), 0.0);

	col += luma - Color::RGBToLuminance(col);

	return col;
}

/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////

float3 Clamp(float3 val)
{
	return clamp(val, Params[0].xyz, Params[1].xyz);
}

float3 Gamma(float3 val)
{
	return Gamma(val, Params[0].rgb, Params[1].rgb, Params[2].rgb);
}

float3 PerceptualQuantizerEncode(float3 val)
{
	val *= (Params[0].x / 10000.f);
	float3 y_m1 = pow(val, 0.1593017578125f);
	return pow((0.8359375f + 18.8515625f * y_m1) / (1.f + 18.6875f * y_m1), 78.84375f);
}

float3 PerceptualQuantizerDecode(float3 val)
{
	float3 e_m12 = pow(val, 1.f / 78.84375f);
	float3 out_color = pow(max(0, e_m12 - 0.8359375f) / (18.8515625f - 18.6875f * e_m12), 1.f / 0.1593017578125f);
	return out_color * (10000.f / Params[0].x);
}

float3 ASC_CDL(float3 val)
{
	return ASC_CDL(val, Params[0].rgb, Params[1].rgb, Params[2].rgb);
}

float3 LiftGammaGain(float3 val)
{
	return LiftGammaGain(val, Params[0].gbar, Params[1].gbar, Params[2].gbar);
}

float3 SaturationHue(float3 val)
{
	val = Saturation(val, Params[0].r);
	val = HueShift(val, Params[0].g);
	return val;
}

float3 OklchSaturation(float3 val)
{
	float3 oklab = RgbToOklab(val);

	float c = length(oklab.yz);
	float h = atan2(oklab.z, oklab.y);

	c = min(0.37, c * Params[0].r);
	c = (1 - pow(1 - c / 0.37, Params[0].g)) * 0.37;
	h += Params[0].b * PI;

	sincos(h, oklab.z, oklab.y);
	oklab.yz *= c;

	return max(0, OklabToRgb(oklab));
}

// mimicking lightroom colour mixer
float3 OklchColourMixer(float3 val)
{
	static const float redHue = 0.08120523664;  //0xff0000

	float3 oklab = RgbToOklab(val);

	float l = oklab.x;
	float c = length(oklab.yz);
	float h = atan2(oklab.z, oklab.y);

	float lerpFactor = (h / (2 * PI) - redHue) * 7;
	int leftHue = floor(lerpFactor);
	lerpFactor = lerpFactor - leftHue;
	leftHue += (leftHue < 0) * 7;
	int rightHue = (leftHue + 1) % 7;
	float effect = saturate(c / 0.37);

	// hue shift
	h = h + lerp(Params[leftHue].x, Params[rightHue].x, lerpFactor) * PI / 4;
	// vibrance
	float c1 = (1 - pow(1 - c / 0.37, Params[leftHue].y)) * 0.37;
	float c2 = (1 - pow(1 - c / 0.37, Params[rightHue].y)) * 0.37;
	c = lerp(c1, c2, lerpFactor);
	// brightness
	l = l + lerp(Params[leftHue].z, Params[rightHue].z, lerpFactor) * effect;

	oklab.x = l;
	sincos(h, oklab.z, oklab.y);
	oklab.yz *= c;

	return max(0, OklabToRgb(oklab));
}

/////////////////////////////////////////////////////////////////////////////////

float3 MatMul(float3 val)
{
	return mul(float3x3(Params[0].rgb, Params[1].rgb, Params[2].rgb), val);
}

/////////////////////////////////////////////////////////////////////////////////

float3 ExposureContrast(float3 val)
{
	val *= Params[0].xyz;
	val = LinearContrast(val, Params[1].xyz, Params[2].xyz);
	return val;
}

/////////////////////////////////////////////////////////////////////////////////

/*
    tizian/tonemapper
        url:    https://github.com/tizian/tonemapper
        license:
            The MIT License (MIT)

            Copyright (c) 2022 Tizian Zeltner

            Permission is hereby granted, free of charge, to any person obtaining a copy
            of this software and associated documentation files (the "Software"), to deal
            in the Software without restriction, including without limitation the rights
            to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
            copies of the Software, and to permit persons to whom the Software is
            furnished to do so, subject to the following conditions:

            The above copyright notice and this permission notice shall be included in all
            copies or substantial portions of the Software.

            THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
            IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
            FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
            AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
            LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
            OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
            SOFTWARE.
*/

float3 Reinhard(float3 val)
{
	val *= Params[0].x;
	float luma = Color::RGBToLuminance(val);
	float lumaOut = luma / (1 + luma);
	val = val / (luma + 1e-10) * lumaOut;
	val = saturate(val);
	return val;
}

float3 ReinhardExt(float3 val)
{
	val *= Params[0].x;
	float luma = Color::RGBToLuminance(val);
	float lumaOut = luma * (1 + luma / (Params[0].y * Params[0].y)) / (1 + luma);
	val = val / (luma + 1e-10) * lumaOut;
	val = saturate(val);
	return val;
}

float3 HejlBurgessDawsonFilmic(float3 val)
{
	val *= Params[0].x;
	val = max(0, val - 0.004);
	val = (val * (6.2 * val + .5)) / (val * (6.2 * val + 1.7) + 0.06);
	val = pow(saturate(val), 2.2);
	return val;
}

float3 AldridgeFilmic(float3 val)
{
	val *= Params[0].x;
	float tmp = 2.0 * Params[0].y;
	val = val + (tmp - val) * clamp(tmp - val, 0.0, 1.0) * (0.25 / Params[0].y) - Params[0].y;
	val = (val * (6.2 * val + 0.5)) / (val * (6.2 * val + 1.7) + 0.06);
	val = pow(saturate(val), 2.2);
	return val;
}

float3 ACEScct(float3 linearColor, bool inverse)
{
	const float a = 10.5402377416545;
	const float b = 0.0729055341958355;
	const float cutoff = 0.0078125;
	const float cutoff2 = 0.155251141552511;

	float3 cct = linearColor;

	if (!inverse) {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] > cutoff) {
				cct[i] = (log2(linearColor[i]) + 9.72) / 17.52;
			} else {
				cct[i] = linearColor[i] * a + b;
			}
		}
	} else {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] >= cutoff2) {
				cct[i] = pow(2, linearColor[i] * 17.52 - 9.72);
			} else {
				cct[i] = (linearColor[i] - b) / a;
			}
		}
	}

	return cct;
}

float3 ARRIlogC4(float3 linearColor, bool inverse)
{
	const float a = 2231.8263;
	const float b = 0.9071359;
	const float c = 0.0928641;
	const float s = 0.6816768;
	const float t = -0.0180570;

	float3 logColor = linearColor;

	if (!inverse) {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] > t) {
				logColor[i] = (log2(linearColor[i] * a + 64) - 6) / 14 * b + c;
			} else {
				logColor[i] = (linearColor[i] - t) / s;
			}
		}
	} else {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] > 0) {
				logColor[i] = (pow(2, 14 * (linearColor[i] - c) / b + 6) - 64) / a;
			} else {
				logColor[i] = linearColor[i] * s + t;
			}
		}
	}

	return logColor;
}

float3 SonySLog3(float3 linearColor, bool inverse)
{
	float3 logColor = linearColor;

	if (!inverse) {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] > 0.0112500) {
				logColor[i] = (420.0 + log10((linearColor[i] + 0.01) / 0.19) * 261.5) / 1023.0;
			} else {
				logColor[i] = (linearColor[i] * (171.2102946929 - 95.0) / 0.0112500 + 95.0) / 1023.0;
			}
		}
	} else {
		for (int i = 0; i < 3; i++) {
			if (linearColor[i] > 0.1712102946929 / 1023.0) {
				logColor[i] = pow(10, (linearColor[i] * 1023.0 - 420.0) / 261.5) * 0.19 - 0.01;
			} else {
				logColor[i] = (linearColor[i] * 1023.0 - 95.0) * 0.0112500 / (171.2102946929 - 95.0);
			}
		}
	}

	return logColor;
}

float3 LinearToLog(float3 val)
{
	float3 logColor = val;
	if (Params[0].y == 0)
		logColor = ACEScct(val, (bool)Params[0].x);
	else if (Params[0].y == 1)
		logColor = ARRIlogC4(val, (bool)Params[0].x);
	else if (Params[0].y == 2)
		logColor = SonySLog3(val, (bool)Params[0].x);
	return logColor;
}

float3 AcesHill(float3 val)
{
	static const float3x3 g_sRGBToACEScg = float3x3(
		0.613117812906440, 0.341181995855625, 0.045787344282337,
		0.069934082307513, 0.918103037508582, 0.011932775530201,
		0.020462992637737, 0.106768663382511, 0.872715910619442);
	static const float3x3 g_ACEScgToSRGB = float3x3(
		1.704887331049502, -0.624157274479025, -0.080886773895704,
		-0.129520935348888, 1.138399326040076, -0.008779241755018,
		-0.024127059936902, -0.124620612286390, 1.148822109913262);

	val *= Params[0].x;

	val = mul(g_sRGBToACEScg, val);
	float3 a = val * (val + 0.0245786f) - 0.000090537f;
	float3 b = val * (0.983729f * val + 0.4329510f) + 0.238081f;
	val = a / b;
	val = mul(g_ACEScgToSRGB, val);

	val = saturate(val);

	return val;
}

float3 AcesNarkowicz(float3 val)
{
	val *= Params[0].x;

	static const float A = 2.51;
	static const float B = 0.03;
	static const float C = 2.43;
	static const float D = 0.59;
	static const float E = 0.14;
	val *= 0.6;
	val = (val * (A * val + B)) / (val * (C * val + D) + E);
	val = saturate(val);
	return val;
}

float3 AcesGuy(float3 val)
{
	val *= Params[0].x;
	val = val / (val + 0.155f) * 1.019;

	val = pow(saturate(val), 2.2);
	return val;
}

float3 LottesFilmic(float3 val)
{
	val *= Params[0].x;
	float a = Params[0].y,
		  d = Params[0].z,
		  b = (-pow(Params[1].x, a) + pow(Params[0].w, a) * Params[1].y) /
	          ((pow(Params[0].w, a * d) - pow(Params[1].x, a * d)) * Params[1].y),
		  c = (pow(Params[0].w, a * d) * pow(Params[1].x, a) - pow(Params[0].w, a) * pow(Params[1].x, a * d) * Params[1].y) /
	          ((pow(Params[0].w, a * d) - pow(Params[1].x, a * d)) * Params[1].y);

	val = pow(val, a) / (pow(val, a * d) * b + c);
	val = saturate(val);
	return val;
}

float DayCurve(float x, float k)
{
	const float b = Params[0].y;
	const float w = Params[0].z;
	const float c = Params[0].w;
	const float s = Params[1].x;
	const float t = Params[1].y;

	if (x < c) {
		return k * (1.0 - t) * (x - b) / (c - (1.0 - t) * b - t * x);
	} else {
		return (1.0 - k) * (x - c) / (s * x + (1.0 - s) * w - c) + k;
	}
}

float3 DayFilmic(float3 val)
{
	const float b = Params[0].y;
	const float w = Params[0].z;
	const float c = Params[0].w;
	const float s = Params[1].x;
	const float t = Params[1].y;

	val *= Params[0].x;
	float k = (1.0 - t) * (c - b) / ((1.0 - s) * (w - c) + (1.0 - t) * (c - b));
	val = float3(DayCurve(val.r, k), DayCurve(val.g, k), DayCurve(val.b, k));

	val = saturate(val);
	return val;
}

float3 UchimuraFilmic(float3 val)
{
	const float P = Params[0].y;
	const float a = Params[0].z;
	const float m = Params[0].w;
	const float l = Params[1].x;
	const float c = Params[1].y;
	const float b = Params[1].z;

	val *= Params[0].x;

	float l0 = ((P - m) * l) / a,
		  S0 = m + l0,
		  S1 = m + a * l0,
		  C2 = (a * P) / (P - S1),
		  CP = -C2 / P;

	float3 w0 = 1.0 - smoothstep(0.0, m, val),
		   w2 = step(m + l0, val),
		   w1 = 1.0 - w0 - w2;

	float3 T = m * pow(val / m, c) + b,           // toe
		L = m + a * (val - m),                    // linear
		S = P - (P - S1) * exp(CP * (val - S0));  // shoulder

	val = T * w0 + L * w1 + S * w2;

	val = saturate(val);
	return val;
}

/*  AgX Reference:
 *  AgX by longbool https://www.shadertoy.com/view/dtSGD1
 *  AgX Minimal by bwrensch https://www.shadertoy.com/view/cd3XWr
 *  Fork AgX Minima troy_s 342 by troy_s https://www.shadertoy.com/view/mdcSDH
 */

// Mean error^2: 3.6705141e-06
float3 AgxDefaultContrastApprox5(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;

	return +15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x +
	       0.4298 * x2 + 0.1191 * x - 0.00232;
}

// Mean error^2: 1.85907662e-06
float3 AgxDefaultContrastApprox6(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;

	return -17.86 * x4 * x2 * x + 78.01 * x4 * x2 - 126.7 * x4 * x + 92.06 * x4 -
	       28.72 * x2 * x + 4.361 * x2 - 0.1718 * x + 0.002857;
}

float3 Agx(float3 val)
{
	const float3x3 agx_mat = transpose(
		float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
			0.0784335999999992, 0.878468636469772, 0.0784336,
			0.0792237451477643, 0.0791661274605434, 0.879142973793104));

	const float min_ev = -12.47393f;
	const float max_ev = 4.026069f;

	// Input transform
	val = mul(agx_mat, val);

	// Log2 space encoding
	val = clamp(log2(val), min_ev, max_ev);
	val = (val - min_ev) / (max_ev - min_ev);

	// Apply sigmoid function approximation
	val = AgxDefaultContrastApprox6(val);

	return val;
}

float3 AgxEotf(float3 val)
{
	const float3x3 agx_mat_inv = transpose(
		float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
			-0.0980208811401368, 1.15190312990417, -0.0980434501171241,
			-0.0990297440797205, -0.0989611768448433, 1.15107367264116));

	// Undo input transform
	val = mul(agx_mat_inv, val);

	// sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
	// NOTE: We're linearizing the output here. Comment/adjust when
	// *not* using a sRGB render target
	val = pow(saturate(val), 2.2);

	return val;
}

float3 AgxMinimal(float3 val)
{
	val *= Params[0].x;

	val = Agx(val);
	val = ASC_CDL(val, Params[0].y, Params[0].z, Params[0].w);
	val = Saturation(val, Params[1].x);
	val = AgxEotf(val);

	return val;
}

// src: https://github.com/ltmx/Melon-Tonemapper
// GPL-3.0 license
float3 MelonHueShift(float3 In)
{
	float A = max(In.x, In.y);
	return float3(A, max(A, In.z), In.z);
}

float3 MelonTonemap(float3 color)
{
	color *= Params[0].r;

	// remaps the colors to [0-1] range
	// tested to be as close ti ACES contrast levels as possible
	color = pow(color, float3(1.56, 1.56, 1.56));
	color = color / (color + 0.84);

	// governs the transition to white for high color intensities
	float factor = max(color.r, max(color.g, color.b)) * 0.15;  // multiply by 0.15 to get a similar look to ACES
	factor = factor / (factor + 1);                             // remaps the factor to [0-1] range
	factor *= factor;                                           // smooths the transition to white

	// shift the hue for high intensities (for a more pleasing look).
	color = lerp(color, MelonHueShift(color), factor);   // can be removed for more neutral colors
	color = lerp(color, float3(1.0, 1.0, 1.0), factor);  // shift to white for high intensities

	// clamp to [0-1] range
	return clamp(color, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
}

/*
    EmbarkStudios/kajiya
        url:    https://github.com/EmbarkStudios/kajiya
        license:
			Copyright (c) 2019 Embark Studios

			Permission is hereby granted, free of charge, to any
			person obtaining a copy of this software and associated
			documentation files (the "Software"), to deal in the
			Software without restriction, including without
			limitation the rights to use, copy, modify, merge,
			publish, distribute, sublicense, and/or sell copies of
			the Software, and to permit persons to whom the Software
			is furnished to do so, subject to the following
			conditions:

			The above copyright notice and this permission notice
			shall be included in all copies or substantial portions
			of the Software.

			THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
			ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
			TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
			PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
			SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
			CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
			OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
			IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
			DEALINGS IN THE SOFTWARE.
*/

float KajiyaCurve(float v)
{
	return 1.0 - exp(-v);
}

float3 KajiyaCurve(float3 v)
{
	return 1.0 - exp(-v);
}

float3 KajiyaTonemap(float3 col)
{
	col *= Params[0].r;

	float3 ycbcr = RgbToYCbCr(col);

	float bt = KajiyaCurve(length(ycbcr.yz) * 2.4);
	float desat = max((bt - 0.7) * 0.8, 0);
	desat = desat * desat;

	float3 desat_col = lerp(col, ycbcr.x, desat);

	float tm_luma = KajiyaCurve(ycbcr.x);
	float3 tm0 = col * max(tm_luma / max(Color::RGBToLuminance(col), 1e-5), 0);
	float final_mult = 0.97;
	float3 tm1 = KajiyaCurve(desat_col);

	return lerp(tm0, tm1, bt * bt) * final_mult;
}

/////////////////////////////////////////////////////////////////////////////////
// Enhanced HDR Functions (inspired by SpecialK)
/////////////////////////////////////////////////////////////////////////////////

// PQ-based perceptual boost similar to SpecialK's implementation
float3 PerceptualBoost(float3 color)
{
	// Parameters from UI
	float perceptual_strength = Params[0].x;
	float color_boost = Params[0].y;
	float luminance_threshold = Params[0].z;
	float blend_factor = Params[0].w;

	if (perceptual_strength <= 0.0f)
		return color;

	// Convert to XYZ to get proper luminance
	float3 xyz_color = mul(float3x3(
							   0.4123908, 0.35758434, 0.18048079,
							   0.21263901, 0.71516868, 0.07219232,
							   0.01933082, 0.11919478, 0.95053215),
		color);

	float luminance = max(xyz_color.y, 0.0f);
	float3 normalized_color = luminance > 0.0f ? xyz_color / luminance : xyz_color;

	if (luminance > 0.0f) {
		// Apply PQ-based perceptual boost
		float pq_luma = Color::pq::Encode(float3(luminance, luminance, luminance), perceptual_strength).x;
		float3 pq_color = Color::pq::Encode(xyz_color, perceptual_strength);

		// Enhanced PQ curve processing
		pq_luma = pow(pq_luma * blend_factor, 1.0f / blend_factor);
		pq_color = pow(pq_color * blend_factor, 1.0f / blend_factor);

		// Decode back to linear
		float boosted_luma = Color::pq::Decode(float3(pq_luma, pq_luma, pq_luma), perceptual_strength).x;
		float3 boosted_color = Color::pq::Decode(pq_color, perceptual_strength);

		// Apply color boost for highly saturated content
		if (color_boost > 0.0f) {
			float3 luma_ratio = boosted_luma > 0.0f ? (boosted_color / boosted_luma) : float3(1, 1, 1);
			boosted_color = lerp(luma_ratio * boosted_luma, boosted_color, color_boost);
		}

		// Convert back to RGB
		xyz_color = boosted_color;
	}

	// Convert XYZ back to RGB
	color = mul(float3x3(
					3.24096994, -1.53738318, -0.49861076,
					-0.96924364, 1.8759675, 0.04155506,
					0.05563008, -0.20397696, 1.05697151),
		xyz_color);

	return max(color, 0.0f);
}

// Enhanced gamut expansion similar to SpecialK's implementation
float3 EnhancedGamutExpansion(float3 color)
{
	float expansion_factor = Params[0].x;
	float saturation_threshold = Params[0].y;
	float luminance_weight = Params[0].z;

	if (expansion_factor <= 0.0f)
		return color;

	// Convert to AP1 working space (similar to SpecialK's approach)
	static const float3x3 sRGB_to_AP1 = float3x3(
		0.61319, 0.33951, 0.04737,
		0.07021, 0.91634, 0.01345,
		0.02062, 0.10957, 0.86961);

	static const float3x3 AP1_to_sRGB = float3x3(
		1.70505, -0.62179, -0.08326,
		-0.13026, 1.14080, -0.01055,
		-0.02400, -0.12897, 1.15297);

	float3 color_ap1 = mul(sRGB_to_AP1, color);

	// Calculate luminance and chromaticity
	float luma_ap1 = Color::RGBToLuminance(mul(AP1_to_sRGB, color_ap1));
	float3 chroma_ap1 = luma_ap1 > 0.0f ? (color_ap1 / luma_ap1) : float3(1, 1, 1);

	// Calculate chroma distance (saturation)
	float chroma_dist_sqr = dot(chroma_ap1 - 1.0f, chroma_ap1 - 1.0f);
	chroma_dist_sqr = max(chroma_dist_sqr, 0.000001f);

	// Calculate expansion amount based on chroma and luminance
	float expansion_amount = (1.0f - exp2(-4.0f * chroma_dist_sqr)) *
	                         (1.0f - exp2(-4.0f * expansion_factor * luma_ap1 * luminance_weight));

	// Apply expansion only if above saturation threshold
	if (sqrt(chroma_dist_sqr) > saturation_threshold) {
		// Wide gamut matrix (expands towards P3/BT2020-like)
		static const float3x3 expand_matrix = float3x3(
			0.83451690546233900, 0.1602595895494930, 0.00522350498816804,
			0.02554519357785500, 0.9731015318660700, 0.00135327455607548,
			0.00192582885428273, 0.0303727970124423, 0.96770137413327500);

		float3 expanded_color = mul(expand_matrix, color_ap1);
		color_ap1 = lerp(color_ap1, expanded_color, expansion_amount);
	}

	// Convert back to sRGB
	return mul(AP1_to_sRGB, color_ap1);
}

// Content EOTF handling
float3 ContentEOTF(float3 color)
{
	int eotf_type = (int)Params[0].x;
	float custom_gamma = Params[0].y;
	float midgray_adjust = Params[0].z;

	// Apply EOTF based on type
	switch (eotf_type) {
	case 0:  // Linear - no change
		break;

	case 1:  // sRGB
		color = pow(abs(color), 2.2f) * sign(color);
		break;

	case 2:  // Gamma 2.2
		color = pow(abs(color), 2.2f) * sign(color);
		break;

	case 3:  // Gamma 2.4
		color = pow(abs(color), 2.4f) * sign(color);
		break;

	case 4:  // Custom
		color = pow(abs(color), custom_gamma) * sign(color);
		break;
	}

	// Apply mid-gray adjustment if specified
	if (abs(midgray_adjust) > 0.001f) {
		float luma = Color::RGBToLuminance(color);
		float adjusted_luma = luma * (1.0f + midgray_adjust);
		color = luma > 0.0f ? (color / luma) * adjusted_luma : color;
	}

	return color;
}

// HDR Visualization tools
float3 HDRVisualization(float3 color)
{
	int viz_type = (int)Params[0].x;
	float max_luminance = Params[0].y;
	float reference_white = Params[0].z;
	float gamut_scale = Params[0].w;

	if (viz_type == 0)  // None
		return color;

	float3 result = color;

	switch (viz_type) {
	case 1:  // Luminance Heatmap
		{
			float luminance = Color::RGBToLuminance(color) * 80.0f;  // Convert to nits
			float normalized_luma = saturate(luminance / max_luminance);

			// Create heatmap from blue (low) through green, yellow, red to white (high)
			if (normalized_luma < 0.25f)
				result = lerp(float3(0, 0, 1), float3(0, 1, 1), normalized_luma * 4.0f);
			else if (normalized_luma < 0.5f)
				result = lerp(float3(0, 1, 1), float3(0, 1, 0), (normalized_luma - 0.25f) * 4.0f);
			else if (normalized_luma < 0.75f)
				result = lerp(float3(0, 1, 0), float3(1, 1, 0), (normalized_luma - 0.5f) * 4.0f);
			else if (normalized_luma < 1.0f)
				result = lerp(float3(1, 1, 0), float3(1, 0, 0), (normalized_luma - 0.75f) * 4.0f);
			else
				result = float3(1, 1, 1);  // Overexposed
			break;
		}

	case 2:  // Exposure Stops
		{
			float luminance = Color::RGBToLuminance(color) * 80.0f;
			float stops = log2(luminance / reference_white);

			// Color code by exposure stops
			if (stops < -4.0f)
				result = float3(0.1, 0.0, 0.3);  // Very dark purple
			else if (stops < -2.0f)
				result = float3(0.0, 0.0, 1.0);  // Blue
			else if (stops < -1.0f)
				result = float3(0.0, 0.5, 1.0);  // Cyan
			else if (stops < 0.0f)
				result = float3(0.0, 1.0, 0.0);  // Green
			else if (stops < 1.0f)
				result = float3(1.0, 1.0, 0.0);  // Yellow
			else if (stops < 2.0f)
				result = float3(1.0, 0.5, 0.0);  // Orange
			else if (stops < 4.0f)
				result = float3(1.0, 0.0, 0.0);  // Red
			else
				result = float3(1.0, 1.0, 1.0);  // White (overexposed)
			break;
		}

	case 3:  // Gamut Coverage (Rec.709)
	case 4:  // Gamut Coverage (P3)
		{
			// Check if color is outside target gamut
			bool outside_gamut = false;

			if (viz_type == 3)  // Rec.709
			{
				// Simple check: if any component is significantly outside [0,1] after scaling
				float3 scaled = color * gamut_scale;
				outside_gamut = any(scaled < -0.01f) || any(scaled > 1.01f);
			} else  // P3
			{
				// Convert to P3 and check bounds
				static const float3x3 sRGB_to_P3 = float3x3(
					0.8224621, 0.1775380, 0.0000000,
					0.0331941, 0.9668058, 0.0000000,
					0.0170826, 0.0723974, 0.9105199);
				float3 p3_color = mul(sRGB_to_P3, color * gamut_scale);
				outside_gamut = any(p3_color < -0.01f) || any(p3_color > 1.01f);
			}

			if (outside_gamut) {
				// Show out-of-gamut areas in magenta
				result = lerp(color, float3(1, 0, 1), 0.7f);
			} else {
				// Desaturate in-gamut areas
				float luma = Color::RGBToLuminance(color);
				result = lerp(float3(luma, luma, luma), color, 0.3f);
			}
			break;
		}

	case 5:  // 8-bit Quantization
	case 6:  // 10-bit Quantization
		{
			uint scale = (viz_type == 5) ? 255 : 1023;
			float scale_f = (float)scale;

			// Quantize to target bit depth
			uint3 quantized = uint3(color * scale);
			result = float3(quantized) / scale_f;

			// Highlight quantization errors
			float3 error = abs(color - result);
			float max_error = max(error.r, max(error.g, error.b));
			if (max_error > 0.01f) {
				result = lerp(result, float3(1, 0, 0), min(max_error * 10.0f, 0.5f));
			}
			break;
		}

	case 7:  // Overbright Detection
		{
			float max_component = max(color.r, max(color.g, color.b));
			if (max_component > 1.0f) {
				// Flash bright areas in red
				float flash = sin(SharedData::FrameCount * 0.1f) * 0.5f + 0.5f;
				result = lerp(color, float3(1, 0, 0), flash * 0.8f);
			} else {
				// Dim areas within normal range
				result = color * 0.3f;
			}
			break;
		}
	}

	return result;
}

// Enhanced ACES tonemapping with better color preservation
float3 EnhancedACES(float3 color)
{
	float exposure = Params[0].x;
	float saturation = Params[0].y;
	float highlight_protection = Params[0].z;
	float shoulder_strength = Params[0].w;

	color *= exposure;

	// Convert to ACES working space
	static const float3x3 sRGB_to_ACES = float3x3(
		0.59719, 0.35458, 0.04823,
		0.07600, 0.90834, 0.01566,
		0.02840, 0.13383, 0.83777);

	static const float3x3 ACES_to_sRGB = float3x3(
		1.60475, -0.53108, -0.07367,
		-0.10208, 1.10813, -0.00605,
		-0.00327, -0.07276, 1.07602);

	float3 aces_color = mul(sRGB_to_ACES, color);

	// Apply saturation adjustment in ACES space
	if (abs(saturation - 1.0f) > 0.001f) {
		float aces_luma = dot(aces_color, float3(0.2722287, 0.6740818, 0.0536895));
		aces_color = lerp(float3(aces_luma, aces_luma, aces_luma), aces_color, saturation);
	}

	// Enhanced ACES curve with highlight protection
	float3 a = aces_color * (aces_color + 0.0245786f) - 0.000090537f;
	float3 b = aces_color * (0.983729f * aces_color + 0.4329510f) + 0.238081f;
	float3 tone_mapped = a / b;

	// Apply highlight protection to preserve color
	if (highlight_protection > 0.0f) {
		float input_luma = dot(aces_color, float3(0.2722287, 0.6740818, 0.0536895));
		float output_luma = dot(tone_mapped, float3(0.2722287, 0.6740818, 0.0536895));

		// Calculate color ratio preservation
		float3 color_ratio = input_luma > 0.0f ? (aces_color / input_luma) : float3(1, 1, 1);
		float3 protected_color = color_ratio * output_luma;

		// Blend between standard tonemap and protected version
		float protection_weight = saturate(input_luma * highlight_protection);
		tone_mapped = lerp(tone_mapped, protected_color, protection_weight);
	}

	// Apply shoulder strength adjustment
	if (abs(shoulder_strength - 1.0f) > 0.001f) {
		tone_mapped = pow(abs(tone_mapped), 1.0f / shoulder_strength) * sign(tone_mapped);
	}

	// Convert back to sRGB
	float3 result = mul(ACES_to_sRGB, tone_mapped);

	return saturate(result);
}

[numthreads(8, 8, 1)] void main(uint2 tid : SV_DispatchThreadID) {
	float3 color = TexColor[tid].rgb;

	color = TRANSFORM_FUNC(color);

	RWTexOut[tid] = float4(color, 1);
}