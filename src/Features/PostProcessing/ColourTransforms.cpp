#include "ColourTransforms.h"

#include "State.h"
#include "Util.h"

#include "ColourSpace.h"
#include "ColourTransformRegistry.h"

#include "IconsFontAwesome5.h"

#include <filesystem>
#include <fstream>

template <size_t N, typename T>
constexpr auto make_array(T value) -> std::array<T, N>
{
	std::array<T, N> a{};
	for (auto& x : a)
		x = value;
	return a;
}

struct SavedSettings
{
	std::string TransformType = "nothingburger";
	std::array<float4, 8> Params;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	SavedSettings,
	TransformType,
	Params)

std::vector<TransformFileJSON> LoadTransformConfigFiles()
{
	std::vector<TransformFileJSON> allConfigs;

	// Define the directory where transform config files are located
	std::filesystem::path configDir = "Data\\Shaders\\PostProcessing\\ColourTransforms\\Configs";

	try {
		if (!std::filesystem::exists(configDir)) {
			logger::warn("Transform config directory does not exist: {}", configDir.string());
			return allConfigs;
		}

		// Load all JSON files in the config directory
		for (const auto& entry : std::filesystem::directory_iterator(configDir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				try {
					std::ifstream file(entry.path());
					if (!file.is_open()) {
						logger::warn("Failed to open transform config file: {}", entry.path().string());
						continue;
					}

					nlohmann::json j;
					file >> j;

					TransformFileJSON config = j;
					allConfigs.push_back(config);

					logger::info("Loaded transform config file: {} with {} transforms",
						entry.path().filename().string(), config.transforms.size());
				} catch (const std::exception& e) {
					logger::error("Error loading transform config file {}: {}", entry.path().string(), e.what());
				}
			}
		}
	} catch (const std::exception& e) {
		logger::error("Error reading transform config directory: {}", e.what());
	}

	return allConfigs;
}

template <int num = 3>
bool shiftSlider(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	static_assert(num > 1 && num < 5);

	if (ImGui::GetIO().KeyShift) {
		auto changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
		if (changed)
			for (int i = 1; i < num; i++)
				v[i] = v[0];
		return changed;
	} else {
		if constexpr (num == 2)
			return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
		else if constexpr (num == 3)
			return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
		else if constexpr (num == 4)
			return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
	}
}

template <int num = 1>
bool exposureSlider(float* val)
{
	float tempVal[num];
	for (int i = 0; i < num; i++)
		tempVal[i] = log2(val[i]);

	bool retval;
	if constexpr (num == 1)
		retval = ImGui::SliderFloat("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 2)
		retval = shiftSlider<2>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 3)
		retval = shiftSlider<3>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 4)
		retval = shiftSlider<4>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");

	for (int i = 0; i < num; i++)
		val[i] = exp2(tempVal[i]);

	return retval;
}

template <int num = 3>
bool shiftSlider(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	static_assert(num > 1 && num < 5);

	if (ImGui::GetIO().KeyShift) {
		auto changed = ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
		if (changed)
			for (int i = 1; i < num; i++)
				v[i] = v[0];
		return changed;
	} else {
		if constexpr (num == 2)
			return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
		else if constexpr (num == 3)
			return ImGui::SliderFloat3(label, v, v_min, v_max, format, flags);
		else if constexpr (num == 4)
			return ImGui::SliderFloat4(label, v, v_min, v_max, format, flags);
	}
}

template <int num = 1>
bool exposureSlider(float* val)
{
	float tempVal[num];
	for (int i = 0; i < num; i++)
		tempVal[i] = log2(val[i]);

	bool retval;
	if constexpr (num == 1)
		retval = ImGui::SliderFloat("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 2)
		retval = shiftSlider<2>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 3)
		retval = shiftSlider<3>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");
	else if constexpr (num == 4)
		retval = shiftSlider<4>("Exposure", tempVal, -4.f, 4.f, "%+.2f EV");

	for (int i = 0; i < num; i++)
		val[i] = exp2(tempVal[i]);

	return retval;
}

struct TransformInfo
{
	std::string_view name;
	std::string_view func_name;
	std::string_view desc;

	using CTP = std::array<float4, 8>;
	std::function<void(CTP&)> draw_settings_func;
	CTP default_settings;

	CTP cached_settings;

	static auto& GetTransforms()
	{
		static std::vector<TransformInfo> transforms;
		static std::once_flag flag;

		std::call_once(flag, []() {
			// Initialize the UI function registry
			UIFunctionRegistry::GetInstance().InitializeDefaultFunctions();

			// Load transforms from JSON configuration files
			auto configFiles = LoadTransformConfigFiles();

			for (const auto& configFile : configFiles) {
				for (const auto& transformConfig : configFile.transforms) {
					TransformInfo info;

					// Convert JSON strings to string_view (we need to store the strings permanently)
					static std::vector<std::string> permanent_names;
					static std::vector<std::string> permanent_func_names;
					static std::vector<std::string> permanent_descs;

					permanent_names.push_back(transformConfig.name);
					permanent_func_names.push_back(transformConfig.func_name);
					permanent_descs.push_back(transformConfig.description);

					info.name = permanent_names.back();
					info.func_name = permanent_func_names.back();
					info.desc = permanent_descs.back();
					info.default_settings = transformConfig.default_settings;
					info.cached_settings = transformConfig.default_settings;

					// Get the UI function from the registry
					info.draw_settings_func = UIFunctionRegistry::GetInstance().GetUIFunction(
						transformConfig.ui_type, transformConfig.ui_params);

					transforms.push_back(info);
				}
			}

			// If no transforms were loaded from files, fall back to a minimal set
			if (transforms.empty()) {
				logger::warn("No transforms loaded from JSON files, using fallback transforms");

				// Add a basic separator and one simple transform as fallback
				static std::string fallback_name1 = "_";
				static std::string fallback_func1 = "Basic Transforms";
				static std::string fallback_desc1 = "Primary operators with basic functions.";

				static std::string fallback_name2 = "ASC CDL";
				static std::string fallback_func2 = "ASC_CDL";
				static std::string fallback_desc2 = "ASC Color Decision List.";

				transforms.push_back({ fallback_name1,
					fallback_func1,
					fallback_desc1,
					[](CTP&) {},
					{} });

				transforms.push_back({ fallback_name2,
					fallback_func2,
					fallback_desc2,
					[](CTP& params) {
						using f4 = float4;
						constexpr auto shiftHint = []() { ImGui::TextWrapped("Press Shift to control all channels at the same time."); };
						shiftSlider("Slope", &params[0].x, 0.f, 2.f, "%.2f");
						shiftSlider("Power", &params[1].x, 0.f, 2.f, "%.2f");
						shiftSlider("Offset", &params[2].x, -1.f, 1.f, "%.2f");
						shiftHint();
					},
					{ float4{ 1.f, 1.f, 1.f, 0.f }, float4{ 1.f, 1.f, 1.f, 0.f }, float4{ 0.f, 0.f, 0.f, 0.f } } });

				// Set cached settings for fallback transforms
				for (auto& t : transforms) {
					t.cached_settings = t.default_settings;
				}
			}
		});

		return transforms;
	}

	static void GetDefaultParams(int& transformType, CTP& params)
	{
		auto& transforms = GetTransforms();
		if (auto it = std::ranges::find_if(transforms, [&](TransformInfo& x) { return "ASC CDL"sv == x.name; });
			it != transforms.end()) {
			transformType = (int)(it - transforms.begin());
			params = it->default_settings;
		} else
			logger::error("Somehow, the default settings are invalid. Please contact the author.");
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourTransforms::DrawSettings()
{
	auto& transforms = TransformInfo::GetTransforms();

	if (ImGui::BeginCombo("Transforms", transforms[transformType].name.data(), ImGuiComboFlags_HeightLargest)) {
		for (int i = 0; i < transforms.size(); ++i) {
			if (transforms[i].name == "_"sv) {
				ImGui::SeparatorText(transforms[i].func_name.data());
			} else {
				if (ImGui::Selectable(transforms[i].name.data(), i == transformType)) {
					transforms[transformType].cached_settings = settings;
					settings = transforms[i].cached_settings;
					transformType = i;
					recompileFlag = true;
				}
			}

			if (auto _tt = Util::HoverTooltipWrapper())
				ImGui::Text(transforms[i].desc.data());
		}
		ImGui::EndCombo();
	}
	ImGui::Spacing();
	ImGui::TextWrapped(transforms[transformType].desc.data());
	ImGui::Spacing();
	if (ImGui::Button("Reset", { -1, 0 }))
		settings = transforms[transformType].default_settings;
	ImGui::Spacing();

	ImGui::PushID(transformType);
	transforms[transformType].draw_settings_func(settings);
	ImGui::PopID();
}

void ColourTransforms::RestoreDefaultSettings()
{
	TransformInfo::GetDefaultParams(transformType, settings);
	recompileFlag = true;
}

void ColourTransforms::LoadSettings(json& o_json)
{
	auto& transforms = TransformInfo::GetTransforms();

	SavedSettings tempSettings = o_json;

	if (auto it = std::ranges::find_if(transforms, [&](TransformInfo& x) { return tempSettings.TransformType == x.name; });
		it != transforms.end()) {
		transformType = (int)(it - transforms.begin());
		settings = tempSettings.Params;
	} else {
		TransformInfo::GetDefaultParams(transformType, settings);
	}

	recompileFlag = true;
}

void ColourTransforms::SaveSettings(json& o_json)
{
	auto& transforms = TransformInfo::GetTransforms();

	SavedSettings tempSettings = {
		.TransformType = transforms[transformType].name.data(),
		.Params = settings,
	};

	o_json = tempSettings;
}

void ColourTransforms::SetupResources()
{
	auto renderer = globals::game::renderer;

	logger::debug("Creating buffers...");
	{
		tonemapCB = std::make_unique<ConstantBuffer>(ConstantBufferDesc(uint(sizeof(float4) * settings.size()), false));
	}

	logger::debug("Creating 2D textures...");
	{
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

		texTonemap = std::make_unique<Texture2D>(texDesc);
		texTonemap->CreateSRV(srvDesc);
		texTonemap->CreateUAV(uavDesc);
	}

	CompileComputeShaders();
}

void ColourTransforms::ClearShaderCache()
{
	const auto shaderPtrs = std::array{
		&tonemapCS
	};

	for (auto shader : shaderPtrs)
		if ((*shader)) {
			(*shader)->Release();
			shader->detach();
		}

	CompileComputeShaders();
}

void ColourTransforms::CompileComputeShaders()
{
	const auto& transforms = TransformInfo::GetTransforms();

	struct ShaderCompileInfo
	{
		winrt::com_ptr<ID3D11ComputeShader>* programPtr;
		std::string_view filename;
		std::vector<std::pair<const char*, const char*>> defines;
		std::string entry = "main";
	};

	std::vector<ShaderCompileInfo>
		shaderInfos = {
			{ &tonemapCS, "transform.cs.hlsl", { { "TRANSFORM_FUNC", transforms[transformType].func_name.data() } } },
		};

	for (auto& info : shaderInfos) {
		auto path = std::filesystem::path("Data\\Shaders\\PostProcessing\\ColourTransforms") / info.filename;
		if (auto rawPtr = reinterpret_cast<ID3D11ComputeShader*>(Util::CompileShader(path.c_str(), info.defines, "cs_5_0", info.entry.c_str())))
			info.programPtr->attach(rawPtr);
	}

	recompileFlag = false;
}

void ColourTransforms::Draw(TextureInfo& inout_tex)
{
	auto context = globals::d3d::context;

	if (recompileFlag)
		ClearShaderCache();

	tonemapCB->Update(settings.data(), settings.size() * sizeof(float4));

	ID3D11ShaderResourceView* srv = inout_tex.srv;
	ID3D11UnorderedAccessView* uav = texTonemap->uav.get();
	ID3D11Buffer* cb = tonemapCB->CB();

	context->CSSetConstantBuffers(1, 1, &cb);
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
	context->CSSetShaderResources(0, 1, &srv);
	context->CSSetShader(tonemapCS.get(), nullptr, 0);

	context->Dispatch((texTonemap->desc.Width + 7) >> 3, (texTonemap->desc.Height + 7) >> 3, 1);

	// clean up
	srv = nullptr;
	uav = nullptr;
	cb = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
	context->CSSetShaderResources(0, 1, &srv);
	context->CSSetConstantBuffers(0, 1, &cb);
	context->CSSetShader(nullptr, nullptr, 0);

	inout_tex = { texTonemap->resource.get(), texTonemap->srv.get() };
}