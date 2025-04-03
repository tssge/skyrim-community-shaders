#pragma once
#include "FidelityFX/host/ffx_types.h"
#include "ffx_api_types.h"
#include "reshade/reshade.hpp"

#include <PostProcess.h>

class HDR
{
public:
	static HDR* GetSingleton()
	{
		static HDR singleton;
		return &singleton;
	}

	static std::string GetShortName() { return "High Dynamic Range"; }

	struct Settings
	{
		bool enableHDR = false;

		uint tonemapOperator = 0;

		float exposure = 1.0f;
		float highlights = 1.0f;
		float shadows = 1.0f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float dechroma = 0.0f;
		float hueCorrectionStrength = 0.0f;

		uint paperWhite = 1000;
		uint peakNits = 10000;
	};

	bool enabledSaveLater = false;

	Settings settings;
	std::mutex settingsMutex;

	void DrawSettings();
	void SaveSettings(json& o_json);
	void LoadSettings(json& o_json);
	void RestoreDefaultSettings();

	void SetupResources();
	void UpdateHDRData() const;

	void ApplyHDR();

	void DestroyResources() const;
	void ClearShaderCache();

	XM_ALIGNED_STRUCT(16)
	HDRDataCB
	{
		// parameters0.x = tonemapOperator
		// parameters0.y = paperWhite
		// parameters0.z = peakNits
		// parameters0.w = exposure
		DirectX::XMVECTOR parameters0;
		// parameters1.x = highlights
		// parameters1.y = shadows
		// parameters1.z = contrast
		// parameters1.w = saturation
		DirectX::XMVECTOR parameters1;
		// parameters2.x = dechroma
		// parameters2.y = hueCorrectionStrength
		// parameters2.z = 0.f // Currently unused
		// parameters2.w = 0.f // Currently unused
		DirectX::XMVECTOR parameters2;
	};

	static_assert((sizeof(HDRDataCB) % 16) == 0, "CB size not padded correctly");

	ConstantBuffer* hdrDataCB = nullptr;

	Texture2D* hdrTexture = nullptr;
	Texture2D* outputTexture = nullptr;

	ID3D11ComputeShader* hdrOutputCS = nullptr;
	ID3D11ComputeShader* GetHDROutputCS();

	// Format constants to be used elsewhere
	static constexpr auto BSGraphics_HDR_Format = RE::BSGraphics::Format::kR16G16B16A16_FLOAT;
};
