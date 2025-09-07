#include "HDR.h"

#include "PCH.h"

#include "Buffer.h"
#include "State.h"
#include "Upscaling.h"
#include "Util.h"
#include <imgui.h>

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	HDR::Settings,
	enableHDR,
	tonemapOperator,
	exposure,
	highlights,
	shadows,
	contrast,
	saturation,
	dechroma,
	hueCorrectionStrength,
	paperWhite,
	peakNits);

void HDR::DrawSettings()
{
	const char* operators[] = {
		"None",
		"Saturate",
		"Frostbite",
		"Reinhard-Jodie",
		"ACES",
		"Uncharted 2",
		"DICE Plus",
		"RenoDRT"
	};

	ImGui::Text("Toggling this setting requires a restart to work correctly!");
	ImGui::Checkbox("HDR Enabled", &enabledSaveLater);

	if (settings.enableHDR != enabledSaveLater) {
		ImGui::TextColored({ 1, 0, 0, 1 }, "Warning: This setting will only apply after saving and restarting!");
	}

	if (ImGui::Button("Reset HDR Settings", { -1, 0 })) {
		settings.tonemapOperator = 0;

		settings.exposure = 1.0f;
		settings.highlights = 1.0f;
		settings.shadows = 1.0f;
		settings.contrast = 1.0f;
		settings.saturation = 1.0f;
		settings.dechroma = 0.0f;
		settings.hueCorrectionStrength = 0.0f;

		settings.paperWhite = 400;
		settings.peakNits = 10000;
	}

	ImGui::SliderInt("Tonemap Operator", reinterpret_cast<int*>(&settings.tonemapOperator), 0, 7, std::format("{}", operators[settings.tonemapOperator]).c_str());

	ImGui::SliderInt("Paper White (nits)", reinterpret_cast<int*>(&settings.paperWhite), 1, 25000);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("Paper White sets the game's reference white brightness.");
	}

	ImGui::SliderInt("Peak Brightness (nits)", reinterpret_cast<int*>(&settings.peakNits), 1, 25000);
	if (auto _tt = Util::HoverTooltipWrapper()) {
		ImGui::Text("Peak Brightness defines the maximum brightness level.");
	}

	ImGui::SliderFloat("Exposure", &settings.exposure, 0.f, 2.f);
	ImGui::SliderFloat("Highlights", &settings.highlights, 0.f, 2.f);
	ImGui::SliderFloat("Shadows", &settings.shadows, 0.f, 2.f);
	ImGui::SliderFloat("Contrast", &settings.contrast, 0.f, 2.f);
	ImGui::SliderFloat("Saturation", &settings.saturation, 0.f, 2.f);
	ImGui::SliderFloat("Dechroma", &settings.dechroma, 0.f, 1.f);
	ImGui::SliderFloat("Hue Correction Strength", &settings.hueCorrectionStrength, 0.f, 1.f);
}

void HDR::LoadSettings(json& o_json)
{
	if (o_json.contains("HDR")) {
		settings = o_json["HDR"];
	}
}

void HDR::SaveSettings(json& o_json)
{
	o_json["HDR"] = settings;
}

void HDR::RestoreDefaultSettings()
{
	settings = {};
}

void HDR::SetupResources()
{
	auto renderer = globals::game::renderer;
	auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	D3D11_TEXTURE2D_DESC texDesc{};
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	main.texture->GetDesc(&texDesc);
	main.SRV->GetDesc(&srvDesc);
	main.UAV->GetDesc(&uavDesc);

	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	srvDesc.Format = texDesc.Format;
	uavDesc.Format = texDesc.Format;

	hdrTexture = new Texture2D(texDesc);
	hdrTexture->CreateSRV(srvDesc);
	hdrTexture->CreateUAV(uavDesc);

	texDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
	srvDesc.Format = texDesc.Format;
	uavDesc.Format = texDesc.Format;

	outputTexture = new Texture2D(texDesc);
	outputTexture->CreateSRV(srvDesc);
	outputTexture->CreateUAV(uavDesc);
}

void HDR::ApplyHDR()
{
	if (!loaded || !settings.enableHDR)
		return;

	auto state = globals::state;
	auto context = globals::context;

	state->BeginPerfEvent("HDR");

	auto renderer = globals::game::renderer;
	auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	// Copy main render target to HDR texture
	context->CopyResource(hdrTexture->resource.get(), main.texture);

	// Apply HDR processing
	auto hdrCS = GetHDROutputCS();
	if (hdrCS) {
		context->CSSetShader(hdrCS, nullptr, 0);
		context->CSSetShaderResources(0, 1, &hdrTexture->srv);
		context->CSSetUnorderedAccessViews(0, 1, &outputTexture->uav, nullptr);

		// Set HDR constants
		struct HDRConstants {
			float exposure;
			float highlights;
			float shadows;
			float contrast;
			float saturation;
			float dechroma;
			float hueCorrectionStrength;
			uint tonemapOperator;
			uint paperWhite;
			uint peakNits;
			uint pad[2];
		} hdrConstants;

		hdrConstants.exposure = settings.exposure;
		hdrConstants.highlights = settings.highlights;
		hdrConstants.shadows = settings.shadows;
		hdrConstants.contrast = settings.contrast;
		hdrConstants.saturation = settings.saturation;
		hdrConstants.dechroma = settings.dechroma;
		hdrConstants.hueCorrectionStrength = settings.hueCorrectionStrength;
		hdrConstants.tonemapOperator = settings.tonemapOperator;
		hdrConstants.paperWhite = settings.paperWhite;
		hdrConstants.peakNits = settings.peakNits;

		// TODO: Set constant buffer

		// Dispatch compute shader
		auto& outputRT = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
		ID3D11Resource* outputTextureResource;
		outputRT.SRV->GetResource(&outputTextureResource);
		context->CopyResource(outputTextureResource, outputTexture->resource.get());

		state->EndPerfEvent();
	}
}

void HDR::ClearShaderCache()
{
	if (hdrOutputCS) {
		hdrOutputCS->Release();
		hdrOutputCS = nullptr;
	}
}

ID3D11ComputeShader* HDR::GetHDROutputCS()
{
	if (!hdrOutputCS) {
		logger::debug("Compiling HDROutputCS.hlsl");
		hdrOutputCS = static_cast<ID3D11ComputeShader*>(Util::CompileShader(L"Data\\Shaders\\HDROutputCS.hlsl", {}, "cs_5_0"));
	}
	return hdrOutputCS;
}
