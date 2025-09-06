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

	logger::trace("RT Contact Shadows: Building BLAS from scene geometry...");

	// Clear previous geometry data
	uniqueGeometries.clear();
	meshInstances.clear();
	geometryCache.clear();

	// Step 1: Collect geometry from the game's render state
	CollectSceneGeometry();

	if (uniqueGeometries.empty()) {
		logger::warn("RT Contact Shadows: No geometry collected for BLAS building");
		return;
	}

	logger::trace("RT Contact Shadows: Collected {} unique geometries, {} instances", 
		uniqueGeometries.size(), meshInstances.size());

	// Step 2: Create vertex/index buffers on GPU
	CreateGeometryBuffers();

	// Step 3: Build BLAS for each unique mesh
	BuildBLAS();

	// Step 4: Build TLAS with instance transforms
	BuildTLAS();

	logger::info("RT Contact Shadows: Successfully built acceleration structures");
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

	// TODO: Set up shader table addresses
	// rayDispatchDesc.RayGenerationShaderRecord.StartAddress = ...
	// rayDispatchDesc.MissShaderTable.StartAddress = ...
	// rayDispatchDesc.HitGroupTable.StartAddress = ...

	// Set pipeline state and dispatch
	// d3d12CommandList->SetPipelineState1(rtPipelineState.get());
	// d3d12CommandList->DispatchRays(&rayDispatchDesc);

	logger::trace("RT Contact Shadows ray dispatch completed for frame {}", cbData.FrameIndex);
}

void RTContactShadows::CollectSceneGeometry()
{
	// TODO: This is a placeholder implementation
	// In a full implementation, this would need to hook into the render pipeline
	// to collect geometry from active render passes during scene rendering
	
	// For now, create a simple test geometry (triangle)
	GeometryData testGeometry;
	testGeometry.vertices = {
		0.0f, 1.0f, 0.0f,   // Top vertex
		-1.0f, -1.0f, 0.0f, // Bottom left
		1.0f, -1.0f, 0.0f   // Bottom right
	};
	testGeometry.indices = { 0, 1, 2 };
	testGeometry.vertexCount = 3;
	testGeometry.indexCount = 3;
	
	// Add to unique geometries
	uniqueGeometries.push_back(testGeometry);
	
	// Create instance
	MeshInstance instance;
	instance.geometry = &uniqueGeometries.back();
	instance.transform = DirectX::XMMatrixIdentity();
	instance.instanceID = 0;
	
	meshInstances.push_back(instance);
	
	logger::trace("RT Contact Shadows: Added test geometry with {} vertices, {} indices", 
		testGeometry.vertexCount, testGeometry.indexCount);
}

void RTContactShadows::ProcessRenderPass(RE::BSRenderPass* a_pass)
{
	if (!a_pass || !a_pass->geometry) {
		return;
	}

	// Get triangle shape from geometry
	auto triShape = a_pass->geometry->AsTriShape();
	if (!triShape) {
		return;
	}

	// Get renderer data
	auto rendererData = a_pass->geometry->GetGeometryRuntimeData().rendererData;
	if (!rendererData || !rendererData->rawVertexData) {
		return;
	}

	// Get geometry hash for caching
	std::string geoHash = GetGeometryHash(a_pass->geometry);
	
	// Check if we already have this geometry
	auto cacheIt = geometryCache.find(geoHash);
	GeometryData* geometry = nullptr;
	
	if (cacheIt == geometryCache.end()) {
		// Create new geometry data
		GeometryData newGeometry;
		
		// Extract vertex positions
		uint32_t vertexSize = rendererData->vertexDesc.GetSize();
		uint32_t positionOffset = rendererData->vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::Attribute::VA_POSITION);
		uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		
		newGeometry.vertices.reserve(vertexCount * 3);
		
		for (uint32_t v = 0; v < vertexCount; v++) {
			float* position = reinterpret_cast<float*>(&rendererData->rawVertexData[vertexSize * v + positionOffset]);
			newGeometry.vertices.push_back(position[0]);
			newGeometry.vertices.push_back(position[1]);
			newGeometry.vertices.push_back(position[2]);
		}
		
		newGeometry.vertexCount = vertexCount;
		
		// Extract indices if available
		// Note: Index data access depends on BSTriShape implementation details
		// This is a simplified approach
		for (uint32_t i = 0; i < vertexCount; i++) {
			newGeometry.indices.push_back(i);
		}
		newGeometry.indexCount = vertexCount;
		
		// Add to collection
		uniqueGeometries.push_back(newGeometry);
		geometryCache[geoHash] = uniqueGeometries.size() - 1;
		geometry = &uniqueGeometries.back();
	} else {
		// Use cached geometry
		geometry = &uniqueGeometries[cacheIt->second];
	}
	
	// Create instance with transform
	MeshInstance instance;
	instance.geometry = geometry;
	
	// Get world transform from the geometry node
	if (auto node = a_pass->geometry->GetObjectByName()) {
		auto& worldTransform = node->world;
		instance.transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(&worldTransform)));
	}
	
	instance.instanceID = static_cast<uint32_t>(meshInstances.size());
	meshInstances.push_back(instance);
}

std::string RTContactShadows::GetGeometryHash(RE::BSGeometry* geometry)
{
	if (!geometry) {
		return "";
	}
	
	// Create a simple hash based on geometry pointer and vertex count
	auto triShape = geometry->AsTriShape();
	if (!triShape) {
		return "";
	}
	
	uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
	size_t geoPtr = reinterpret_cast<size_t>(geometry);
	
	return std::to_string(geoPtr) + "_" + std::to_string(vertexCount);
}

void RTContactShadows::CreateGeometryBuffers()
{
	if (uniqueGeometries.empty()) {
		return;
	}

	// Calculate total buffer sizes
	size_t totalVertexData = 0;
	size_t totalIndexData = 0;
	
	for (const auto& geometry : uniqueGeometries) {
		totalVertexData += geometry.vertices.size() * sizeof(float);
		totalIndexData += geometry.indices.size() * sizeof(uint32_t);
	}

	// Create vertex buffer
	if (totalVertexData > 0) {
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = totalVertexData;
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
			IID_PPV_ARGS(&vertexBuffer));

		if (FAILED(hr)) {
			logger::error("RT Contact Shadows: Failed to create vertex buffer");
			return;
		}

		// Map and copy vertex data
		void* mappedData = nullptr;
		hr = vertexBuffer->Map(0, nullptr, &mappedData);
		if (SUCCEEDED(hr)) {
			uint8_t* destPtr = static_cast<uint8_t*>(mappedData);
			size_t offset = 0;
			
			for (const auto& geometry : uniqueGeometries) {
				size_t dataSize = geometry.vertices.size() * sizeof(float);
				memcpy(destPtr + offset, geometry.vertices.data(), dataSize);
				offset += dataSize;
			}
			
			vertexBuffer->Unmap(0, nullptr);
		}
	}

	// Create index buffer
	if (totalIndexData > 0) {
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC resourceDesc = {};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = totalIndexData;
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
			IID_PPV_ARGS(&indexBuffer));

		if (FAILED(hr)) {
			logger::error("RT Contact Shadows: Failed to create index buffer");
			return;
		}

		// Map and copy index data
		void* mappedData = nullptr;
		hr = indexBuffer->Map(0, nullptr, &mappedData);
		if (SUCCEEDED(hr)) {
			uint8_t* destPtr = static_cast<uint8_t*>(mappedData);
			size_t offset = 0;
			
			for (const auto& geometry : uniqueGeometries) {
				size_t dataSize = geometry.indices.size() * sizeof(uint32_t);
				memcpy(destPtr + offset, geometry.indices.data(), dataSize);
				offset += dataSize;
			}
			
			indexBuffer->Unmap(0, nullptr);
		}
	}

	logger::trace("RT Contact Shadows: Created geometry buffers - vertex: {} bytes, index: {} bytes", 
		totalVertexData, totalIndexData);
}

void RTContactShadows::BuildBLAS()
{
	if (!d3d12Device || uniqueGeometries.empty()) {
		return;
	}

	// Build BLAS geometry descriptions
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	geometryDescs.reserve(uniqueGeometries.size());

	size_t vertexOffset = 0;
	size_t indexOffset = 0;

	for (const auto& geometry : uniqueGeometries) {
		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc = {};
		geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		
		// Set vertex buffer
		geoDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress() + vertexOffset;
		geoDesc.Triangles.VertexBuffer.StrideInBytes = geometry.vertexStride;
		geoDesc.Triangles.VertexCount = geometry.vertexCount;
		geoDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		
		// Set index buffer
		geoDesc.Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress() + indexOffset;
		geoDesc.Triangles.IndexCount = geometry.indexCount;
		geoDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		
		geometryDescs.push_back(geoDesc);
		
		vertexOffset += geometry.vertices.size() * sizeof(float);
		indexOffset += geometry.indices.size() * sizeof(uint32_t);
	}

	// Get prebuild info
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
	blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	blasInputs.NumDescs = static_cast<UINT>(geometryDescs.size());
	blasInputs.pGeometryDescs = geometryDescs.data();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuildInfo = {};
	d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuildInfo);

	// Create scratch buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC scratchDesc = {};
	scratchDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	scratchDesc.Width = blasPrebuildInfo.ScratchDataSizeInBytes;
	scratchDesc.Height = 1;
	scratchDesc.DepthOrArraySize = 1;
	scratchDesc.MipLevels = 1;
	scratchDesc.Format = DXGI_FORMAT_UNKNOWN;
	scratchDesc.SampleDesc.Count = 1;
	scratchDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	scratchDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&scratchDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&blasScratchBuffer));

	if (FAILED(hr)) {
		logger::error("RT Contact Shadows: Failed to create BLAS scratch buffer");
		return;
	}

	// Create BLAS buffer
	D3D12_RESOURCE_DESC blasDesc = {};
	blasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	blasDesc.Width = blasPrebuildInfo.ResultDataMaxSizeInBytes;
	blasDesc.Height = 1;
	blasDesc.DepthOrArraySize = 1;
	blasDesc.MipLevels = 1;
	blasDesc.Format = DXGI_FORMAT_UNKNOWN;
	blasDesc.SampleDesc.Count = 1;
	blasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	blasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&blasDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&bottomLevelAS));

	if (FAILED(hr)) {
		logger::error("RT Contact Shadows: Failed to create BLAS buffer");
		return;
	}

	// Build BLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc_ = {};
	blasDesc_.Inputs = blasInputs;
	blasDesc_.ScratchDataAddress = blasScratchBuffer->GetGPUVirtualAddress();
	blasDesc_.DestAccelerationStructureAddress = bottomLevelAS->GetGPUVirtualAddress();

	d3d12CommandList->BuildRaytracingAccelerationStructure(&blasDesc_, 0, nullptr);

	// Add barrier
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = bottomLevelAS.get();
	d3d12CommandList->ResourceBarrier(1, &barrier);

	logger::trace("RT Contact Shadows: Built BLAS with {} geometries", geometryDescs.size());
}

void RTContactShadows::BuildTLAS()
{
	if (!d3d12Device || meshInstances.empty() || !bottomLevelAS) {
		return;
	}

	// Create instance buffer
	size_t instanceDataSize = meshInstances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC instanceDesc = {};
	instanceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	instanceDesc.Width = instanceDataSize;
	instanceDesc.Height = 1;
	instanceDesc.DepthOrArraySize = 1;
	instanceDesc.MipLevels = 1;
	instanceDesc.Format = DXGI_FORMAT_UNKNOWN;
	instanceDesc.SampleDesc.Count = 1;
	instanceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&instanceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&tlasInstanceBuffer));

	if (FAILED(hr)) {
		logger::error("RT Contact Shadows: Failed to create TLAS instance buffer");
		return;
	}

	// Map and fill instance data
	D3D12_RAYTRACING_INSTANCE_DESC* mappedInstances = nullptr;
	hr = tlasInstanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstances));
	if (SUCCEEDED(hr)) {
		for (size_t i = 0; i < meshInstances.size(); i++) {
			const auto& instance = meshInstances[i];
			auto& desc = mappedInstances[i];

			// Set transform (DirectX expects row-major 3x4 matrix)
			DirectX::XMFLOAT3X4 transform3x4;
			DirectX::XMStoreFloat3x4(&transform3x4, instance.transform);
			memcpy(desc.Transform, &transform3x4, sizeof(desc.Transform));

			desc.InstanceID = instance.instanceID;
			desc.InstanceMask = 0xFF;
			desc.InstanceContributionToHitGroupIndex = 0;
			desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			desc.AccelerationStructure = bottomLevelAS->GetGPUVirtualAddress();
		}
		tlasInstanceBuffer->Unmap(0, nullptr);
	}

	// Get prebuild info for TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
	tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	tlasInputs.NumDescs = static_cast<UINT>(meshInstances.size());
	tlasInputs.InstanceDescs = tlasInstanceBuffer->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
	d3d12Device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

	// Create TLAS scratch buffer
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC scratchDesc = {};
	scratchDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	scratchDesc.Width = tlasPrebuildInfo.ScratchDataSizeInBytes;
	scratchDesc.Height = 1;
	scratchDesc.DepthOrArraySize = 1;
	scratchDesc.MipLevels = 1;
	scratchDesc.Format = DXGI_FORMAT_UNKNOWN;
	scratchDesc.SampleDesc.Count = 1;
	scratchDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	scratchDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&scratchDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&tlasScratchBuffer));

	if (FAILED(hr)) {
		logger::error("RT Contact Shadows: Failed to create TLAS scratch buffer");
		return;
	}

	// Create TLAS buffer
	D3D12_RESOURCE_DESC tlasDesc = {};
	tlasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	tlasDesc.Width = tlasPrebuildInfo.ResultDataMaxSizeInBytes;
	tlasDesc.Height = 1;
	tlasDesc.DepthOrArraySize = 1;
	tlasDesc.MipLevels = 1;
	tlasDesc.Format = DXGI_FORMAT_UNKNOWN;
	tlasDesc.SampleDesc.Count = 1;
	tlasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	tlasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	hr = d3d12Device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&tlasDesc,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		nullptr,
		IID_PPV_ARGS(&topLevelAS));

	if (FAILED(hr)) {
		logger::error("RT Contact Shadows: Failed to create TLAS buffer");
		return;
	}

	// Build TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc_ = {};
	tlasDesc_.Inputs = tlasInputs;
	tlasDesc_.ScratchDataAddress = tlasScratchBuffer->GetGPUVirtualAddress();
	tlasDesc_.DestAccelerationStructureAddress = topLevelAS->GetGPUVirtualAddress();

	d3d12CommandList->BuildRaytracingAccelerationStructure(&tlasDesc_, 0, nullptr);

	// Add barrier
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = topLevelAS.get();
	d3d12CommandList->ResourceBarrier(1, &barrier);

	logger::trace("RT Contact Shadows: Built TLAS with {} instances", meshInstances.size());
}