# SpecialK Incompatibility - DirectX Hook Conflicts

## Issue Summary
Skyrim Community Shaders and SpecialK cannot be used together simultaneously. When both mods are loaded, they conflict with each other causing crashes, rendering issues, or preventing Skyrim from starting properly. Each mod works perfectly when used independently.

## Root Cause
Both Skyrim Community Shaders and SpecialK hook into DirectX functions and modify the graphics pipeline in various ways:

### Skyrim Community Shaders DirectX Hooks:
- **CreateDXGIFactory1** - Hooked for DXGI factory creation (src/Streamline.h)
- **D3D11CreateDeviceAndSwapChain** - Hooked for device creation (src/Streamline.h) 
- **Present** calls - Intercepted for frame presentation (src/DX12SwapChain.cpp, src/FidelityFX.cpp)
- **D3D12CreateDevice** - Device creation hooking (src/DX12SwapChain.cpp)
- Various shader compilation and resource management hooks

### SpecialK DirectX Hooks:
SpecialK is a comprehensive graphics enhancement tool that also hooks many of the same DirectX functions for:
- Frame rate limiting and presentation control
- HDR and color space management  
- DXGI swap chain management
- D3D11/D3D12 device creation and management
- Graphics debugging and profiling

## Conflict Scenarios
When both tools are loaded, they compete for the same DirectX function hooks, leading to:

1. **Hook Chain Conflicts**: Both tools attempt to hook the same functions, potentially breaking each other's hook chains
2. **State Inconsistency**: One tool may modify DirectX state that the other tool doesn't expect
3. **Resource Conflicts**: Both tools may try to manage the same DirectX resources differently
4. **Initialization Order Issues**: The order in which the tools initialize can affect which hooks take precedence

## Expected Behavior
- Users should be able to use either Skyrim Community Shaders OR SpecialK without issues
- Users should receive a clear warning if both are detected simultaneously
- Ideally, the tools should be able to coexist without conflicts

## Current Behavior  
- Game crashes on startup
- Rendering artifacts or black screens
- One or both tools failing to function properly
- Unpredictable behavior depending on load order

## Technical Details

### Affected Components
- **Streamline Integration** (`src/Streamline.h`, `src/Streamline.cpp`)
- **DX12 Swap Chain** (`src/DX12SwapChain.h`, `src/DX12SwapChain.cpp`)
- **FidelityFX Integration** (`src/FidelityFX.cpp`)
- **DirectX Utilities** (`src/Utils/D3D.h`, `src/Utils/D3D.cpp`)
- **Core Hooks** (`src/Hooks.h`, `src/Hooks.cpp`)

### Key Hook Points
```cpp
// From src/Hooks.cpp - IAT (Import Address Table) hooking
*(uintptr_t*)&ptrD3D11CreateDeviceAndSwapChain = SKSE::PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
*(uintptr_t*)&ptrCreateDXGIFactory = SKSE::PatchIAT(hk_CreateDXGIFactory, "dxgi.dll", !REL::Module::IsVR() ? "CreateDXGIFactory" : "CreateDXGIFactory1");

// Hook function implementations
HRESULT WINAPI hk_CreateDXGIFactory(REFIID, void** ppFactory)
{
    auto streamline = globals::streamline;
    if (!streamline->triedInitialization)
        globals::streamline->LoadInterposer();
    if (streamline->initialized)
        return streamline->slCreateDXGIFactory1(__uuidof(IDXGIFactory4), ppFactory);
    return ptrCreateDXGIFactory(__uuidof(IDXGIFactory4), ppFactory);
}

HRESULT WINAPI hk_D3D11CreateDeviceAndSwapChain(...)
{
    // HDR format modification and device creation
    if (globals::hdr->settings.enableHDR) {
        pSwapChainDesc->BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    }
    // ... additional processing
}

// From src/DX12SwapChain.cpp  
HRESULT DX12SwapChain::Present(UINT SyncInterval, UINT Flags)
```

## Reproduction Steps
1. Install Skyrim Community Shaders following standard installation procedure
2. Install SpecialK following standard installation procedure
3. Launch Skyrim with both mods active
4. Observe crashes, rendering issues, or failure to start

## Environment Information
- **Skyrim Version**: SSE/AE (all versions affected)
- **Graphics APIs**: DirectX 11/12
- **Affected Platforms**: Windows (primary platform for both tools)

## Potential Solutions

### Short-term Workarounds
1. **Mutual Exclusion**: Use only one tool at a time
2. **Detection and Warning**: Implement detection logic to warn users when both are present
3. **Load Order Dependency**: Document specific load order requirements (if any work)

### Long-term Solutions
1. **Hook Coordination**: Implement a shared hooking protocol between the tools
2. **API Abstraction**: Create a common DirectX abstraction layer
3. **Cooperative Mode**: Design a compatibility mode where both tools can coexist
4. **Hook Chain Management**: Implement proper hook chain management to avoid conflicts

## Implementation Considerations

### Detection Logic
```cpp
// Detection function for SpecialK presence
bool DetectSpecialK() {
    // Check for SpecialK modules
    static const wchar_t* specialKModules[] = {
        L"SpecialK64.dll",
        L"SpecialK32.dll", 
        L"dxgi.dll",      // SpecialK commonly proxies this
        L"d3d11.dll",     // SpecialK may proxy this too
        L"dinput8.dll"    // Another common SpecialK proxy
    };
    
    for (const auto& module : specialKModules) {
        HMODULE handle = GetModuleHandle(module);
        if (handle) {
            // Additional validation - check for SpecialK-specific exports
            if (GetProcAddress(handle, "SKX_GetCommandProcessor") ||
                GetProcAddress(handle, "SK_GetDLLRole")) {
                return true;
            }
        }
    }
    return false;
}

// Integration into Community Shaders initialization
void InstallD3DHooks() {
    if (DetectSpecialK()) {
        logger::warn("SpecialK detected! DirectX hook conflicts may occur.");
        logger::warn("Consider using only one graphics enhancement tool at a time.");
        
        // Optional: Skip certain hooks or use compatibility mode
        if (GetPrivateProfileIntW(L"Compatibility", L"SpecialKMode", 0, L"./CommunityShaders.ini")) {
            logger::info("Running in SpecialK compatibility mode");
            return; // Skip DirectX hooks
        }
    }
    
    // Original hook installation
    globals::fidelityFX->LoadFFX();
    *(uintptr_t*)&ptrD3D11CreateDeviceAndSwapChain = SKSE::PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    *(uintptr_t*)&ptrCreateDXGIFactory = SKSE::PatchIAT(hk_CreateDXGIFactory, "dxgi.dll", !REL::Module::IsVR() ? "CreateDXGIFactory" : "CreateDXGIFactory1");
}
```

### Warning System
- Display warning message during Community Shaders initialization
- Log compatibility warnings to help users troubleshoot
- Provide clear instructions for resolving conflicts

## Related Issues
This incompatibility may be related to other hooking conflicts with:
- ReShade (similar DirectX hooking)
- ENB Series (graphics modification)
- Other DirectX injection tools

## Priority
**High** - This affects users who want to combine visual enhancement tools and represents a significant compatibility issue for the modding community.

## Labels
- `bug`
- `compatibility` 
- `directx`
- `enhancement`
- `help wanted`

---

**Note**: This issue affects the broader Skyrim modding ecosystem. Resolving it would benefit not only users of both tools but also establish patterns for compatibility between DirectX-hooking modifications.