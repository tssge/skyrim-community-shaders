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

	void DestroyResources() const;
	void ClearShaderCache();

	// Format constants to be used elsewhere
	static constexpr auto BSGraphics_HDR_Format = RE::BSGraphics::Format::kR16G16B16A16_FLOAT;
	static constexpr auto BSGraphics_HDR_R10_Format = RE::BSGraphics::Format::kR10G10B10A2_UNORM;
};
