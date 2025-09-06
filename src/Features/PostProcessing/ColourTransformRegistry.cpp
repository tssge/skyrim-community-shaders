#include "ColourTransformRegistry.h"

#include "IconsFontAwesome5.h"

// Forward declare template functions that are defined in ColourTransforms.cpp
template <int num>
bool shiftSlider(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags);

template <int num>
bool exposureSlider(float* val);

template <size_t N, typename T>
constexpr auto make_array(T value) -> std::array<T, N>
{
	std::array<T, N> a{};
	for (auto& x : a)
		x = value;
	return a;
}

UIFunctionRegistry::UIFunction UIFunctionRegistry::CreateParameterizedFunction(const std::string& type, const nlohmann::json& params) const
{
	if (type == "none" || type == "_") {
		return [](CTP&) {};
	}

	if (type == "shift_slider_pairs") {
		struct SliderConfig
		{
			std::string label;
			std::array<int, 2> param_path;
			float min, max;
			std::string format;
		};

		std::vector<SliderConfig> sliders;
		bool show_shift_hint = false;

		if (params.contains("sliders")) {
			for (const auto& slider_json : params["sliders"]) {
				SliderConfig config;
				config.label = slider_json.value("label", "");
				auto path = slider_json.value("param_path", std::vector<int>{ 0, 0 });
				config.param_path = { path[0], path[1] };
				config.min = slider_json.value("min", 0.0f);
				config.max = slider_json.value("max", 1.0f);
				config.format = slider_json.value("format", "%.3f");
				sliders.push_back(config);
			}
		}

		if (params.contains("show_shift_hint")) {
			show_shift_hint = params["show_shift_hint"];
		}

		return [sliders, show_shift_hint](CTP& params_ref) {
			for (const auto& slider : sliders) {
				float* value_ptr = &params_ref[slider.param_path[0]][slider.param_path[1]];
				shiftSlider<3>(slider.label.c_str(), value_ptr, slider.min, slider.max, slider.format.c_str(), 0);
			}
			if (show_shift_hint) {
				ImGui::TextWrapped("Press Shift to control all channels at the same time.");
			}
		};
	}

	if (type == "simple_sliders") {
		struct SliderConfig
		{
			std::string label;
			std::array<int, 2> param_path;
			float min, max;
			std::string format;
		};

		std::vector<SliderConfig> sliders;

		if (params.contains("sliders")) {
			for (const auto& slider_json : params["sliders"]) {
				SliderConfig config;
				config.label = slider_json.value("label", "");
				auto path = slider_json.value("param_path", std::vector<int>{ 0, 0 });
				config.param_path = { path[0], path[1] };
				config.min = slider_json.value("min", 0.0f);
				config.max = slider_json.value("max", 1.0f);
				config.format = slider_json.value("format", "%.3f");
				sliders.push_back(config);
			}
		}

		return [sliders](CTP& params_ref) {
			for (const auto& slider : sliders) {
				float* value_ptr = &params_ref[slider.param_path[0]][slider.param_path[1]];
				ImGui::SliderFloat(slider.label.c_str(), value_ptr, slider.min, slider.max, slider.format.c_str());
			}
		};
	}

	if (type == "exposure_contrast") {
		int exposure_channels = 3;
		std::array<int, 2> contrast_param = { 1, 0 };
		std::array<int, 2> pivot_param = { 2, 0 };
		bool show_shift_hint = false;

		if (params.contains("exposure_channels")) {
			exposure_channels = params["exposure_channels"];
		}
		if (params.contains("contrast_param")) {
			auto path = params["contrast_param"];
			contrast_param = { path[0], path[1] };
		}
		if (params.contains("pivot_param")) {
			auto path = params["pivot_param"];
			pivot_param = { path[0], path[1] };
		}
		if (params.contains("show_shift_hint")) {
			show_shift_hint = params["show_shift_hint"];
		}

		return [exposure_channels, contrast_param, pivot_param, show_shift_hint](CTP& params_ref) {
			if (exposure_channels == 3) {
				exposureSlider<3>(&params_ref[0].x);
			} else if (exposure_channels == 1) {
				exposureSlider<1>(&params_ref[0].x);
			}
			shiftSlider<3>("Contrast", &params_ref[contrast_param[0]][contrast_param[1]], 0.f, 3.f, "%.2f", 0);
			shiftSlider<3>("Pivot", &params_ref[pivot_param[0]][pivot_param[1]], 0.f, 4.f, "%.2f", 0);
			if (show_shift_hint) {
				ImGui::TextWrapped("Press Shift to control all channels at the same time.");
			}
		};
	}

	if (type == "lift_gamma_gain") {
		return [](CTP& params_ref) {
			ImGui::DragFloat4("Lift", &params_ref[0].x, 1e-3f, -1.f, 1.f, "%.3f");
			ImGui::DragFloat4("Gamma", &params_ref[1].x, 1e-3f, -1.5f, 1.5f, "%.3f");
			ImGui::DragFloat4("Gain", &params_ref[2].x, 1e-3f, 0.f, 2.f, "%.3f");
		};
	}

	// For unknown types, return empty function
	return [](CTP&) {};
}

void UIFunctionRegistry::InitializeDefaultFunctions()
{
	// Register basic UI function types
	RegisterUIFunction("none", [](CTP&) {});
	RegisterUIFunction("shift_slider_pairs", [](CTP&) {});  // Will be parameterized
	RegisterUIFunction("simple_sliders", [](CTP&) {});      // Will be parameterized
	RegisterUIFunction("exposure_contrast", [](CTP&) {});   // Will be parameterized
	RegisterUIFunction("lift_gamma_gain", [](CTP&) {});     // Will be parameterized
}