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

	if (ImGui::Button("Reload HDR shaders", { -1, 0 })) {
		ClearShaderCache();
		GetHDROutputCS();
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

	UpdateHDRData();
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

	texDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
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

	hdrDataCB = new ConstantBuffer(ConstantBufferDesc<HDRDataCB>());

	UpdateHDRData();
}

void HDR::ApplyHDR()
{
	std::lock_guard<std::mutex> lock(settingsMutex);

	if (!settings.enableHDR)
		return;

	auto context = globals::d3d::context;
	auto state = globals::state;
	auto renderer = globals::game::renderer;

	state->BeginPerfEvent("HDR");

	ID3D11Resource* inputTextureResource;
	auto& inputRT = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];
	inputRT.SRV->GetResource(&inputTextureResource);
	context->CopyResource(hdrTexture->resource.get(), inputTextureResource);

	{
		auto dispatchCount = Util::GetScreenDispatchCount(false);

		ID3D11ShaderResourceView* views[1] = { hdrTexture->srv.get() };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		ID3D11UnorderedAccessView* uavs[1] = { outputTexture->uav.get() };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		ID3D11Buffer* cbs[1] = { hdrDataCB->CB() };

		context->CSSetConstantBuffers(0, ARRAYSIZE(cbs), cbs);

		context->CSSetShader(GetHDROutputCS(), nullptr, 0);

		context->Dispatch(dispatchCount.x, dispatchCount.y, 1);

		// Cleanup
		views[0] = { nullptr };
		context->CSSetShaderResources(0, ARRAYSIZE(views), views);

		uavs[0] = { nullptr };
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);

		cbs[0] = { nullptr };
		context->CSSetConstantBuffers(0, ARRAYSIZE(cbs), cbs);

		ID3D11ComputeShader* shader = nullptr;
		context->CSSetShader(shader, nullptr, 0);
	}

	ID3D11Resource* outputTextureResource;
	auto& outputRT = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER];
	outputRT.SRV->GetResource(&outputTextureResource);
	context->CopyResource(outputTextureResource, outputTexture->resource.get());

	state->EndPerfEvent();
}

void HDR::DestroyResources() const
{
	hdrTexture->srv = nullptr;
	hdrTexture->uav = nullptr;
	hdrTexture->resource = nullptr;
	delete hdrTexture;

	outputTexture->srv = nullptr;
	outputTexture->uav = nullptr;
	outputTexture->resource = nullptr;
	delete outputTexture;
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

void HDR::UpdateHDRData() const
{
	HDRDataCB data;

	data.parameters0 = DirectX::XMVectorSet(static_cast<float>(settings.tonemapOperator), static_cast<float>(settings.paperWhite), static_cast<float>(settings.peakNits), settings.exposure);
	data.parameters1 = DirectX::XMVectorSet(settings.highlights, settings.shadows, settings.contrast, settings.saturation);
	data.parameters2 = DirectX::XMVectorSet(settings.dechroma, settings.hueCorrectionStrength, 0.f, 0.f);

	hdrDataCB->Update(data);
}
