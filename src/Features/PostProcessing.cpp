#include "PostProcessing.h"

#include "IconsFontAwesome5.h"
#include "imgui_stdlib.h"

#include "State.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	PostProcessing::Settings,
	DisableVanillaTonemapping)

void PostProcessing::DrawSettings()
{
	// 0 for list of feats
	// 1 for feat settings
	static int pageNum = 0;
	static int featIdx = 0;
	static int presetIdx = -1;
	const float _iconButtonSize = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.x;
	const ImVec2 iconButtonSize{ _iconButtonSize, _iconButtonSize };

	ImGui::BeginGroup();
	std::string currentPreset = (presetIdx >= 0 && presetIdx < presets.size()) ? presets[presetIdx] : "Select a preset";

	if (ImGui::BeginCombo("##PresetCombo", currentPreset.c_str())) {
		presets = LoadPresets();

		for (int i = 0; i < presets.size(); ++i) {
			bool isSelected = presetIdx == i;
			if (ImGui::Selectable(presets[i].c_str(), isSelected))
				presetIdx = i;
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		if (presetIdx >= 0 && presetIdx < presets.size()) {
			LoadPresetFrom(presets[presetIdx]);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		ImGui::OpenPopup("Save Preset");
	}

	if (ImGui::BeginPopup("Save Preset")) {
		static std::string presetName;
		ImGui::InputText("Preset Name", &presetName);
		if (ImGui::Button("Save") && !presetName.empty()) {
			SavePresetTo(presetName);
			presetName.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::EndGroup();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Spacing();

	if (pageNum == 0) {
		// Effect List

		if (ImGui::Button(ICON_FA_PLUS, iconButtonSize))
			ImGui::OpenPopup("New Feature");
		if (auto _tt = Util::HoverTooltipWrapper())
			ImGui::Text("Add a new effect.");

		if (ImGui::BeginPopup("New Feature", ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar)) {
			bool doClose = false;
			if (ImGui::BeginListBox("##Feature List")) {
				const auto& featConstructors = PostProcessFeatureConstructor::GetFeatureConstructors();

				for (auto& [id, featCon] : featConstructors) {
					if (ImGui::Selectable(featCon.name.c_str())) {
						feats.push_back(std::unique_ptr<PostProcessFeature>{ featCon.fn() });
						feats.back()->name = feats.back()->GetType();

						auto bogey = json::object();
						feats.back()->LoadSettings(bogey);
						feats.back()->SetupResources();

						featIdx = (int)feats.size() - 1;

						doClose = true;
					}
					if (auto _tt = Util::HoverTooltipWrapper())
						ImGui::Text(featCon.desc.c_str());
				}

				ImGui::EndListBox();
			}
			if (doClose)
				ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Dummy({ ImGui::GetTextLineHeightWithSpacing() * 2, 1 });
		ImGui::SameLine();

		ImGui::Checkbox("Bypass", &bypass);
		ImGui::SameLine();
		ImGui::Checkbox("Disable Vanilla Tonemapping", (bool*)&settings.DisableVanillaTonemapping);

		ImGui::Spacing();

		int markedFeat = -1;
		int actionType = -1;  // 0 - remove, 1 - move up, 2 - move down
		if (ImGui::BeginListBox("##Features", { -FLT_MIN, -FLT_MIN })) {
			for (int i = 0; i < feats.size(); ++i) {
				ImGui::PushID(i);

				bool isSelected = featIdx == i;
				if (ImGui::Selectable(feats[i]->name.c_str(), isSelected)) {
					featIdx = i;
				}

				if (ImGui::IsItemHovered()) {
					if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
						markedFeat = i;
						actionType = 0;
					}
					if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && i > 0) {
						markedFeat = i;
						actionType = 1;
					}
					if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && i < feats.size() - 1) {
						markedFeat = i;
						actionType = 2;
					}
				}

				ImGui::PopID();
			}
			ImGui::EndListBox();
		}

		if (markedFeat != -1) {
			if (actionType == 0) {
				feats.erase(feats.begin() + markedFeat);
				if (featIdx >= markedFeat)
					featIdx = std::max(0, featIdx - 1);
			} else if (actionType == 1) {
				std::swap(feats[markedFeat], feats[markedFeat - 1]);
				if (featIdx == markedFeat)
					featIdx--;
			} else if (actionType == 2) {
				std::swap(feats[markedFeat], feats[markedFeat + 1]);
				if (featIdx == markedFeat)
					featIdx++;
			}
		}

	} else {
		// Feature Settings
		if (featIdx >= 0 && featIdx < feats.size()) {
			feats[featIdx]->DrawSettings();
		}
	}
}

void PostProcessing::LoadSettings(json& o_json)
{
	if (o_json.contains("PostProcessing")) {
		settings = o_json["PostProcessing"];
	}
}

void PostProcessing::SaveSettings(json& o_json)
{
	o_json["PostProcessing"] = settings;
}

void PostProcessing::RestoreDefaultSettings()
{
	settings = {};
}

void PostProcessing::SetupResources()
{
	auto renderer = globals::game::renderer;

	// Create copy texture
	auto gameTexMainCopy = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN_COPY];

	D3D11_TEXTURE2D_DESC texDesc;
	gameTexMainCopy.texture->GetDesc(&texDesc);

	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {
		.Format = texDesc.Format,
		.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D = { .MostDetailedMip = 0, .MipLevels = 1 }
	};

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
		.Format = texDesc.Format,
		.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D,
		.Texture2D = { .MipSlice = 0 }
	};

	texDesc.MipLevels = srvDesc.Texture2D.MipLevels = 1;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	texDesc.MiscFlags = 0;

	texCopy = std::make_unique<Texture2D>(texDesc);
	texCopy->CreateSRV(srvDesc);
	texCopy->CreateUAV(uavDesc);

	// Compile copy shader
	auto path = std::filesystem::path("Data\\Shaders\\PostProcessing") / "copy.cs.hlsl";
	if (auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(path.c_str(), {}, "cs_5_0", "main")))
		copyCS.attach(rawPtr);

	// Setup all features
	for (auto& feat : feats) {
		feat->SetupResources();
	}
}

void PostProcessing::ClearShaderCache()
{
	if (copyCS) {
		copyCS->Release();
		copyCS = nullptr;
	}

	for (auto& feat : feats) {
		feat->ClearShaderCache();
	}
}

void PostProcessing::Reset()
{
	feats.clear();
	bypass = false;
	isrefraction = false;
}

void PostProcessing::PostPostLoad()
{
	// Load presets and setup features
	presets = LoadPresets();
}

void PostProcessing::PreProcess()
{
	if (bypass || !loaded)
		return;

	auto context = globals::d3d::context;
	auto renderer = globals::game::renderer;

	// Copy main render target
	auto& main = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];
	context->CopyResource(texCopy->resource.get(), main.texture);

	TextureInfo texInfo = { texCopy->resource.get(), texCopy->srv.get() };

	// Apply all post-processing effects
	for (auto& feat : feats) {
		if (feat->enabled) {
			feat->Draw(texInfo);
		}
	}

	// Copy back to main render target
	context->CopyResource(main.texture, texInfo.resource);
}

std::vector<std::string> PostProcessing::LoadPresets()
{
	std::vector<std::string> presetList;
	
	// TODO: Implement preset loading from filesystem
	// This would scan the ppPresetPath directory for .json files
	
	return presetList;
}

void PostProcessing::SavePresetTo(std::string a_name)
{
	// TODO: Implement preset saving
	// This would save the current feature configuration to a .json file
}

void PostProcessing::LoadPresetFrom(std::string a_name)
{
	// TODO: Implement preset loading
	// This would load a feature configuration from a .json file
}
