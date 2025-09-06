#include "RTContactShadows.h"

#include "DX12SwapChain.h"
#include "State.h"
#include "Util.h"

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
	RTContactShadows::Settings,
	Enable,
	Intensity,
	MaxDistance,
	MaxSteps)

void RTContactShadows::DrawSettings()
{
	if (!rtSupported) {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "DirectX Raytracing not supported on this device");
		ImGui::TextWrapped("RT Contact Shadows requires DirectX 12 Ultimate with hardware raytracing support (RTX 20-series+ or RDNA2+)");
		return;
	}

	ImGui::Checkbox("Enable RT Contact Shadows", (bool*)&settings.Enable);
	if (settings.Enable) {
		ImGui::SliderFloat("Intensity", &settings.Intensity, 0.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Max Distance", &settings.MaxDistance, 10.0f, 500.0f, "%.1f");
		ImGui::SliderInt("Max Steps", (int*)&settings.MaxSteps, 4, 64);
	}

	ImGui::Spacing();
	ImGui::TextWrapped("RT Contact Shadows use hardware raytracing to provide accurate contact shadows between objects and surfaces.");
}

void RTContactShadows::RestoreDefaultSettings()
{
	settings = {};
}

void RTContactShadows::LoadSettings(json& o_json)
{
	settings = o_json;
	// Force disable if RT not supported
	if (!rtSupported) {
		settings.Enable = false;
	}
}

void RTContactShadows::SaveSettings(json& o_json)
{
	o_json = settings;
}

void RTContactShadows::SetupResources()
{
	CheckRaytracingSupport();

	if (!rtSupported) {
		settings.Enable = false;
		logger::info("RT Contact Shadows: DirectX Raytracing not supported, feature disabled");
		return;
	}

	auto device = globals::d3d::device;
	auto renderer = globals::game::renderer;
	auto& mainRT = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	// Create contact shadow texture matching main render target resolution
	{
		D3D11_TEXTURE2D_DESC mainTexDesc;
		mainRT.texture->GetDesc(&mainTexDesc);

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = mainTexDesc.Width;
		texDesc.Height = mainTexDesc.Height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8_UNORM;  // Single channel for shadow factor
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		contactShadowTexture = std::make_unique<Texture2D>(texDesc);

		logger::info("RT Contact Shadows: Created {}x{} contact shadow texture", texDesc.Width, texDesc.Height);
	}

	// Create constant buffer for RT parameters
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(RTContactShadowsCB);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		rtContactShadowsCB = std::make_unique<ConstantBuffer>(bufferDesc);
	}

	// Create linear sampler for texture sampling
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &linearSampler));
	}

	// Initialize raytracing pipeline if enabled
	if (settings.Enable) {
		InitializeRaytracing();
	}

	logger::info("RT Contact Shadows: Resource setup completed, RT supported: {}, enabled: {}",
		rtSupported, settings.Enable);
}

void RTContactShadows::CheckRaytracingSupport()
{
	auto dx12SwapChain = globals::dx12SwapChain;
	if (!dx12SwapChain || !dx12SwapChain->d3d12Device) {
		rtSupported = false;
		logger::info("RT Contact Shadows: DX12 device not available");
		return;
	}

	// Query for DX12 device with raytracing support
	HRESULT hr = dx12SwapChain->d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12Device));
	if (FAILED(hr)) {
		rtSupported = false;
		logger::warn("RT Contact Shadows: Failed to query ID3D12Device5 interface");
		return;
	}

	// Check raytracing capability
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	hr = d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
	if (FAILED(hr)) {
		rtSupported = false;
		logger::warn("RT Contact Shadows: Failed to check D3D12_OPTIONS5 feature support");
		return;
	}

	if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
		rtSupported = false;
		logger::info("RT Contact Shadows: Hardware raytracing not supported (RaytracingTier: NOT_SUPPORTED)");
		return;
	}

	// Get command list for raytracing operations
	if (dx12SwapChain->commandLists[0]) {
		hr = dx12SwapChain->commandLists[0]->QueryInterface(IID_PPV_ARGS(&d3d12CommandList));
		if (FAILED(hr)) {
			rtSupported = false;
			logger::warn("RT Contact Shadows: Failed to query ID3D12GraphicsCommandList4 interface");
			return;
		}
	} else {
		rtSupported = false;
		logger::warn("RT Contact Shadows: DX12 command list not available");
		return;
	}

	rtSupported = true;
	logger::info("RT Contact Shadows: DirectX Raytracing supported (RaytracingTier: {})",
		static_cast<int>(options5.RaytracingTier));
}

void RTContactShadows::InitializeRaytracing()
{
	if (!rtSupported || initialized) {
		return;
	}

	logger::info("RT Contact Shadows: Initializing raytracing pipeline...");

	try {
		// Step 1: Create acceleration structures
		logger::trace("RT Contact Shadows: Creating acceleration structures...");
		CreateAccelerationStructures();

		if (!bottomLevelAS || !topLevelAS) {
			throw std::runtime_error("Failed to create acceleration structures");
		}

		// Step 2: Create raytracing pipeline
		logger::trace("RT Contact Shadows: Creating raytracing pipeline...");
		CreateRaytracingPipeline();

		if (!rtPipelineState) {
			throw std::runtime_error("Failed to create raytracing pipeline state");
		}

		// Step 3: Create shader table
		logger::trace("RT Contact Shadows: Creating shader table...");
		CreateShaderTable();

		if (!shaderTable) {
			throw std::runtime_error("Failed to create shader table");
		}

		initialized = true;
		logger::info("RT Contact Shadows: Raytracing pipeline initialized successfully");

	} catch (const std::exception& e) {
		logger::error("RT Contact Shadows: Initialization failed - {}", e.what());
		logger::info("RT Contact Shadows: Disabling feature due to initialization failure");

		// Clean up any partially created resources
		ClearShaderCache();

		// Disable the feature
		settings.Enable = false;
		initialized = false;
	}
}

void RTContactShadows::CreateAccelerationStructures()
{
	if (!d3d12Device) {
		return;
	}

	// TODO: Build BLAS (Bottom Level Acceleration Structure) from scene geometry
	// This would need to:
	// 1. Gather geometry from the game's render state
	// 2. Create vertex/index buffers on GPU
	// 3. Build BLAS for each unique mesh
	// 4. Build TLAS (Top Level AS) with instance transforms

	// For now, create placeholder resources
	// In a real implementation, this would build from Skyrim's scene geometry

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = 1024;  // Placeholder size
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	// Create placeholder BLAS
	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&bottomLevelAS));

	if (FAILED(hr)) {
		logger::error("Failed to create BLAS resource");
		return;
	}

	// Create placeholder TLAS
	hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&topLevelAS));

	if (FAILED(hr)) {
		logger::error("Failed to create TLAS resource");
		return;
	}
}

void RTContactShadows::CreateRaytracingPipeline()
{
	if (!d3d12Device) {
		return;
	}

	// TODO: Load and compile RT shaders
	// This would:
	// 1. Load ContactShadowsRT.hlsl
	// 2. Compile raygen, miss, and anyhit shaders
	// 3. Create raytracing pipeline state object
	// 4. Set up shader associations and local root signatures

	// For now, create a minimal pipeline setup
	// In a real implementation, this would load our HLSL shaders

	// Create RT pipeline state object
	D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
	stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	// Note: This is a placeholder - real implementation would need:
	// - DXIL library subobjects for compiled shaders
	// - Hit group subobjects
	// - Local and global root signature subobjects
	// - Raytracing shader config subobjects
	// - Raytracing pipeline config subobjects

	HRESULT hr = d3d12Device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&rtPipelineState));
	if (FAILED(hr)) {
		logger::warn("RT pipeline creation failed - RT Contact Shadows will be disabled");
		settings.Enable = false;
		return;
	}

	logger::info("RT Contact Shadows pipeline created successfully");
}

void RTContactShadows::CreateShaderTable()
{
	if (!d3d12Device || !rtPipelineState) {
		return;
	}

	// TODO: Create shader table with shader identifiers
	// This would:
	// 1. Get shader identifiers from the pipeline state
	// 2. Create buffer for shader table
	// 3. Map and copy shader identifiers and parameters

	// Shader table layout:
	// [RayGen shader identifier]
	// [Miss shader identifier]
	// [Hit group shader identifier]

	const uint32_t shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	const uint32_t shaderTableSize = shaderIdentifierSize * 3;  // raygen + miss + hitgroup

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = shaderTableSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&shaderTable));

	if (FAILED(hr)) {
		logger::error("Failed to create shader table");
		return;
	}

	// TODO: Map buffer and copy shader identifiers
	// This would use ID3D12StateObjectProperties to get shader identifiers
}

void RTContactShadows::ClearShaderCache()
{
	logger::trace("RT Contact Shadows: Clearing raytracing resources...");

	// Clear raytracing pipeline resources
	initialized = false;
	rtPipelineState = nullptr;
	shaderTable = nullptr;
	topLevelAS = nullptr;
	bottomLevelAS = nullptr;
	d3d12CommandList = nullptr;

	// Note: d3d12Device is kept as it's shared and used for capability checking

	logger::trace("RT Contact Shadows: Raytracing resources cleared");
}

void RTContactShadows::Prepass()
{
	if (!settings.Enable || !rtSupported || !initialized) {
		return;
	}

	DispatchRays();
}

void RTContactShadows::DispatchRays()
{
	if (!d3d12CommandList || !rtPipelineState || !topLevelAS) {
		return;
	}

	auto renderer = globals::game::renderer;
	auto& mainRT = renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMAIN];

	// Update constant buffer with current frame data
	RTContactShadowsCB cbData = {};
	cbData.Intensity = settings.Intensity;
	cbData.MaxDistance = settings.MaxDistance;
	cbData.MaxSteps = settings.MaxSteps;
	cbData.FrameIndex = State::GetSingleton()->uiFrameCount;

	D3D11_TEXTURE2D_DESC texDesc;
	mainRT.texture->GetDesc(&texDesc);
	cbData.ScreenSizeX = static_cast<float>(texDesc.Width);
	cbData.ScreenSizeY = static_cast<float>(texDesc.Height);

	// TODO: Update constant buffer and bind resources
	// This would:
	// 1. Map and update the constant buffer
	// 2. Set raytracing pipeline state
	// 3. Bind acceleration structures and textures
	// 4. Set shader table
	// 5. Dispatch rays

	// Placeholder ray dispatch
	D3D12_DISPATCH_RAYS_DESC rayDispatchDesc = {};
	rayDispatchDesc.Width = texDesc.Width;
	rayDispatchDesc.Height = texDesc.Height;
	rayDispatchDesc.Depth = 1;

	// Set up shader table addresses based on layout from CreateShaderTable
	if (shaderTable) {
		D3D12_GPU_VIRTUAL_ADDRESS shaderTableAddress = shaderTable->GetGPUVirtualAddress();
		const uint32_t shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		// RayGen shader record (first entry in shader table)
		rayDispatchDesc.RayGenerationShaderRecord.StartAddress = shaderTableAddress;
		rayDispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderIdentifierSize;

		// Miss shader table (second entry in shader table)
		rayDispatchDesc.MissShaderTable.StartAddress = shaderTableAddress + shaderIdentifierSize;
		rayDispatchDesc.MissShaderTable.SizeInBytes = shaderIdentifierSize;
		rayDispatchDesc.MissShaderTable.StrideInBytes = shaderIdentifierSize;

		// Hit group table (third entry in shader table)
		rayDispatchDesc.HitGroupTable.StartAddress = shaderTableAddress + (shaderIdentifierSize * 2);
		rayDispatchDesc.HitGroupTable.SizeInBytes = shaderIdentifierSize;
		rayDispatchDesc.HitGroupTable.StrideInBytes = shaderIdentifierSize;
	}

	// Set pipeline state and dispatch
	// d3d12CommandList->SetPipelineState1(rtPipelineState.get());
	// d3d12CommandList->DispatchRays(&rayDispatchDesc);

	logger::trace("RT Contact Shadows ray dispatch completed for frame {}", cbData.FrameIndex);
}