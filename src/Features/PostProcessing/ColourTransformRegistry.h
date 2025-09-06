#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Feature.h"
#include "Util.h"

namespace nlohmann
{
	template <>
	struct adl_serializer<float4>
	{
		static void to_json(json& j, const float4& v)
		{
			j = json::array({ v.x, v.y, v.z, v.w });
		}

		static void from_json(const json& j, float4& v)
		{
			if (j.is_array() && j.size() >= 4) {
				v.x = j[0];
				v.y = j[1];
				v.z = j[2];
				v.w = j[3];
			}
		}
	};
}

struct TransformConfigJSON
{
	std::string name;
	std::string func_name;
	std::string description;
	std::string ui_type;
	nlohmann::json ui_params;
	std::array<float4, 8> default_settings;
};

struct TransformFileJSON
{
	std::string version;
	std::string description;
	std::vector<TransformConfigJSON> transforms;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	TransformConfigJSON,
	name,
	func_name,
	description,
	ui_type,
	ui_params,
	default_settings)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	TransformFileJSON,
	version,
	description,
	transforms)

class UIFunctionRegistry
{
public:
	using CTP = std::array<float4, 8>;
	using UIFunction = std::function<void(CTP&)>;

	static UIFunctionRegistry& GetInstance()
	{
		static UIFunctionRegistry instance;
		return instance;
	}

	void RegisterUIFunction(const std::string& type, UIFunction func)
	{
		registry_[type] = func;
	}

	UIFunction GetUIFunction(const std::string& type, const nlohmann::json& params) const
	{
		auto it = registry_.find(type);
		if (it != registry_.end()) {
			return CreateParameterizedFunction(type, params);
		}
		// Return empty function for unknown types or "_" (separator) types
		return [](CTP&) {};
	}

	void InitializeDefaultFunctions();

private:
	std::unordered_map<std::string, UIFunction> registry_;

	UIFunction CreateParameterizedFunction(const std::string& type, const nlohmann::json& params) const;
};