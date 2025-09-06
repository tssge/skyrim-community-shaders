#pragma once

#include <DirectXMath.h>
#include <string>
#include <unordered_map>
#include <vector>

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

	// Geometry data structures for BLAS building
	struct GeometryData
	{
		std::vector<float> vertices;    // Vertex position data (x,y,z per vertex)
		std::vector<uint32_t> indices;  // Index data
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
		uint32_t vertexStride = 12;  // 3 floats * 4 bytes = 12 bytes per vertex
	};

	struct MeshInstance
	{
		GeometryData* geometry = nullptr;
		DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();
		uint32_t instanceID = 0;
	};

	// DX12 Raytracing resources
	winrt::com_ptr<ID3D12Device5> d3d12Device = nullptr;
	winrt::com_ptr<ID3D12GraphicsCommandList4> d3d12CommandList = nullptr;
	winrt::com_ptr<ID3D12StateObject> rtPipelineState = nullptr;
	winrt::com_ptr<ID3D12Resource> shaderTable = nullptr;
	winrt::com_ptr<ID3D12Resource> topLevelAS = nullptr;
	winrt::com_ptr<ID3D12Resource> bottomLevelAS = nullptr;

	// Geometry buffers for BLAS
	winrt::com_ptr<ID3D12Resource> vertexBuffer = nullptr;
	winrt::com_ptr<ID3D12Resource> indexBuffer = nullptr;
	winrt::com_ptr<ID3D12Resource> blasScratchBuffer = nullptr;
	winrt::com_ptr<ID3D12Resource> tlasInstanceBuffer = nullptr;
	winrt::com_ptr<ID3D12Resource> tlasScratchBuffer = nullptr;

	// Geometry collection
	std::vector<GeometryData> uniqueGeometries;
	std::vector<MeshInstance> meshInstances;
	std::unordered_map<std::string, size_t> geometryCache;

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

	// Geometry collection methods
	void CollectSceneGeometry();
	void ProcessRenderPass(RE::BSRenderPass* a_pass);
	std::string GetGeometryHash(RE::BSGeometry* geometry);
	void CreateGeometryBuffers();
	void BuildBLAS();
	void BuildTLAS();

	virtual void LoadSettings(json& o_json) override;
	virtual void SaveSettings(json& o_json) override;
	virtual void RestoreDefaultSettings() override;
};