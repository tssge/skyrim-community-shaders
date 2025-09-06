#pragma once

struct RTContactShadows : Feature
{
	static RTContactShadows* GetSingleton()
	{
		static RTContactShadows singleton;
		return &singleton;
	}

	virtual inline std::string GetName() override { return "RT Contact Shadows"; }
	virtual inline std::string GetShortName() override { return "RTContactShadows"; }
	virtual inline std::string_view GetShaderDefineName() override { return "RTCS"; }
	bool HasShaderDefine(RE::BSShader::Type) override { return true; };

	struct alignas(16) Settings
	{
		uint32_t Enable = 0;  // Disabled by default until RT capability is confirmed
		float Intensity = 1.0f;
		float MaxDistance = 100.0f;
		uint32_t MaxSteps = 16;
	} settings;

	struct alignas(16) RTContactShadowsCB
	{
		float Intensity;
		float MaxDistance;
		uint32_t MaxSteps;
		uint32_t FrameIndex;
		float ScreenSizeX;
		float ScreenSizeY;
		float PaddingX;
		float PaddingY;
	};

	// DX12 Raytracing resources
	winrt::com_ptr<ID3D12Device5> d3d12Device = nullptr;
	winrt::com_ptr<ID3D12GraphicsCommandList4> d3d12CommandList = nullptr;
	winrt::com_ptr<ID3D12StateObject> rtPipelineState = nullptr;
	winrt::com_ptr<ID3D12Resource> shaderTable = nullptr;
	winrt::com_ptr<ID3D12Resource> topLevelAS = nullptr;
	winrt::com_ptr<ID3D12Resource> bottomLevelAS = nullptr;

	// DX11 resources for integration
	eastl::unique_ptr<Texture2D> contactShadowTexture = nullptr;
	eastl::unique_ptr<ConstantBuffer> rtContactShadowsCB = nullptr;
	winrt::com_ptr<ID3D11SamplerState> linearSampler = nullptr;

	bool rtSupported = false;
	bool initialized = false;

	virtual void SetupResources() override;
	virtual void DrawSettings() override;
	virtual void ClearShaderCache() override;
	virtual void Prepass() override;

	void CheckRaytracingSupport();
	void InitializeRaytracing();
	void CreateAccelerationStructures();
	void CreateRaytracingPipeline();
	void CreateShaderTable();
	void DispatchRays();

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void RestoreDefaultSettings() override;
};