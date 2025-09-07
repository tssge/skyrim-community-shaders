#pragma once

#include "Feature.h"
#include "FidelityFX/host/ffx_types.h"
#include "ffx_api_types.h"
#include "reshade/reshade.hpp"

#include <PostProcess.h>

struct HDR : Feature
{
	static HDR* GetSingleton()
	{
		static HDR singleton;
		return &singleton;
	}

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
	} settings;

	bool enabledSaveLater = false;

	// Feature system implementation
	virtual inline std::string GetName() override { return "High Dynamic Range"; }
	virtual inline std::string GetShortName() override { return "HDR"; }
	virtual inline std::string_view GetShaderDefineName() override { return "HDR"; }
	virtual inline bool HasShaderDefine(RE::BSShader::Type t) override
	{
		return t == RE::BSShader::Type::ImageSpace;
	}
	virtual bool SupportsVR() override { return true; }

	virtual void DrawSettings() override;
	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void RestoreDefaultSettings() override;
	virtual void SetupResources() override;
	virtual void ClearShaderCache() override;

	void ApplyHDR();

	// Format constants to be used elsewhere
	static constexpr auto BSGraphics_HDR_Format = RE::BSGraphics::Format::kR16G16B16A16_FLOAT;
	static constexpr auto BSGraphics_HDR_R10_Format = RE::BSGraphics::Format::kR10G10B10A2_UNORM;

private:
	Texture2D* hdrTexture = nullptr;
	Texture2D* outputTexture = nullptr;

	ID3D11ComputeShader* hdrOutputCS = nullptr;
	ID3D11ComputeShader* GetHDROutputCS();
};
