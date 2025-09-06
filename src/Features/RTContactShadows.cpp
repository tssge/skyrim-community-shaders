#include "RTContactShadows.h"

#include "State.h"
#include "Util.h"
#include "DX12SwapChain.h"

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
		return;
	}

	auto device = globals::d3d::device;
	auto& data = State::GetSingleton()->shadowState;

	// Create contact shadow texture
	{
		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = data.uiShadowMapResolution;
		texDesc.Height = data.uiShadowMapResolution;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

		contactShadowTexture = std::make_unique<Texture2D>(texDesc);
	}

	// Create constant buffer
	{
		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(RTContactShadowsCB);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		rtContactShadowsCB = std::make_unique<ConstantBuffer>(bufferDesc);
	}

	// Create linear sampler
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		DX::ThrowIfFailed(device->CreateSamplerState(&samplerDesc, &linearSampler));
	}

	if (settings.Enable) {
		InitializeRaytracing();
	}
}

void RTContactShadows::CheckRaytracingSupport()
{
	auto dx12SwapChain = globals::dx12SwapChain;
	if (!dx12SwapChain || !dx12SwapChain->d3d12Device) {
		rtSupported = false;
		return;
	}

	// Query for DX12 device with raytracing support
	HRESULT hr = dx12SwapChain->d3d12Device->QueryInterface(IID_PPV_ARGS(&d3d12Device));
	if (FAILED(hr)) {
		rtSupported = false;
		return;
	}

	// Check raytracing capability
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	hr = d3d12Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
	if (FAILED(hr) || options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
		rtSupported = false;
		return;
	}

	rtSupported = true;
}

void RTContactShadows::InitializeRaytracing()
{
	if (!rtSupported || initialized) {
		return;
	}

	try {
		CreateAccelerationStructures();
		CreateRaytracingPipeline();
		CreateShaderTable();
		initialized = true;
	} catch (const std::exception& e) {
		// If raytracing initialization fails, disable the feature
		settings.Enable = false;
		initialized = false;
	}
}

void RTContactShadows::CreateAccelerationStructures()
{
	// TODO: Implement acceleration structure creation
	// This would need to build BLAS from scene geometry and TLAS for instances
	// For now, we'll create placeholder structures
}

void RTContactShadows::CreateRaytracingPipeline()
{
	// TODO: Implement raytracing pipeline state object creation
	// This would load and compile the RT shaders (raygen, closesthit, miss)
}

void RTContactShadows::CreateShaderTable()
{
	// TODO: Implement shader table creation
	// Maps shader identifiers to shader records
}

void RTContactShadows::ClearShaderCache()
{
	initialized = false;
	rtPipelineState = nullptr;
	shaderTable = nullptr;
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
	if (!d3d12CommandList || !rtPipelineState) {
		return;
	}

	// TODO: Implement ray dispatch
	// This would:
	// 1. Update constant buffer with current frame data
	// 2. Set raytracing pipeline state
	// 3. Set shader table and resources
	// 4. Dispatch rays for contact shadow computation
}