#pragma once

#include "Buffer.h"

struct TextureInfo
{
	ID3D11Resource* resource;
	ID3D11ShaderResourceView* srv;
};

struct PostProcessFeature
{
	std::string name;
	bool enabled = true;

	virtual std::string GetType() const = 0;
	virtual std::string GetDesc() const = 0;
	virtual bool SupportsVR() { return true; }

	virtual void SetupResources() {}
	virtual void ClearShaderCache() {}
	virtual void RestoreDefaultSettings() {}
	virtual void LoadSettings(json&) {}
	virtual void SaveSettings(json&) {}
	virtual void DrawSettings() {}
	virtual void Draw(TextureInfo&) = 0;
};

struct PostProcessFeatureConstructor
{
	std::string name;
	std::string desc;
	std::function<std::unique_ptr<PostProcessFeature>()> fn;

	static const std::unordered_map<std::string, PostProcessFeatureConstructor>& GetFeatureConstructors();
};
