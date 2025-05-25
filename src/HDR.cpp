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
	ImGui::SliderFloat("Dechroma", &settings.dechroma, 0.f, 2.f);
	ImGui::SliderFloat("Hue Correction Strength", &settings.hueCorrectionStrength, 0.f, 2.f);
}

void HDR::SaveSettings(json& o_json)
{
	std::lock_guard<std::mutex> lock(settingsMutex);
	auto settingsCopy = settings;
	settingsCopy.enableHDR = enabledSaveLater;
	o_json = settingsCopy;
}

void HDR::LoadSettings(json& o_json)
{
	std::lock_guard<std::mutex> lock(settingsMutex);
	settings = o_json;
	enabledSaveLater = settings.enableHDR;
}

void HDR::RestoreDefaultSettings()
{
	settings = {};
}