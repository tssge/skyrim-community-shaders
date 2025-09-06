# SpecialK Cooperation Implementation Guide

This implementation enables Community Shaders to work cooperatively with SpecialK using SpecialK's built-in cooperation API, resolving the DirectX hook conflicts that previously caused crashes and rendering issues.

## How It Works

### Primary Approach: SpecialK Cooperation API

When both Community Shaders and SpecialK are loaded:

1. **Automatic Detection**: Community Shaders detects SpecialK presence at startup
2. **API Loading**: Loads SpecialK's cooperation API functions (`SK_CreateFuncHook`, `SK_EnableHook`, etc.)
3. **Cooperative Hooking**: Uses SpecialK's centralized hook manager instead of direct DirectX hooking
4. **Shared Management**: Both tools coordinate through SpecialK's hook management system

### Fallback Options

If cooperation fails or isn't available:

-   **Compatibility Mode**: Disables conflicting features but maintains basic functionality
-   **Detection Warnings**: Alerts users to potential conflicts
-   **Configuration Options**: Allows users to adjust behavior based on their setup

## Configuration

The system uses `CommunityShaders.ini` for configuration:

```ini
[Compatibility]
# Enable SpecialK cooperation (RECOMMENDED)
UseSpecialKCooperation=1

# Detect SpecialK and warn about conflicts
DetectSpecialK=1

# Fallback: Skip hooks if cooperation fails
SkipHooksWithSpecialK=0

[SpecialKCooperation]
# Use SpecialK's shared hook manager
UseSharedHookManager=1

# Allow SpecialK to manage DirectX operations
DelegatePresent=1
DelegateSwapChain=1
DelegateDeviceCreation=1
```

## Technical Implementation

### Code Structure

-   `src/Utils/CompatibilityDetection.h/cpp`: Core compatibility detection and cooperation API
-   `src/Hooks.cpp`: Modified DirectX hook installation with cooperation support
-   `CommunityShaders.ini`: Configuration file for cooperation settings

### Hook Installation Process

```cpp
void InstallD3DHooks() {
    auto* compatibility = Compatibility::CompatibilityChecker::GetSingleton();
    compatibility->CheckAllConflicts();
    compatibility->LogWarnings();

    // Try SpecialK cooperation first
    if (compatibility->ShouldUseCooperativeHooks()) {
        // Use SpecialK's hook management API
        bool success = compatibility->CreateHookThroughSpecialK(
            L"D3D11CreateDeviceAndSwapChain",
            target, detour, &original
        );
        if (success) return; // Cooperation successful
    }

    // Fallback to standard or compatibility mode
    // ... existing hook installation
}
```

### SpecialK API Integration

The implementation uses SpecialK's exported cooperation functions:

```cpp
// SpecialK cooperation API
SK_CreateFuncHook(functionName, target, detour, &original);
SK_EnableHook(target);
SK_ApplyQueuedHooks(); // Apply all hooks together
```

## User Experience

### Successful Cooperation

```
[INFO] SpecialK cooperation API successfully loaded
[INFO] SpecialK DLL Role: 1
[INFO] Successfully created hook through SpecialK: D3D11CreateDeviceAndSwapChain
[INFO] Successfully created hook through SpecialK: CreateDXGIFactory
[INFO] === COOPERATION MODE ===
[INFO] SpecialK cooperation mode enabled - using shared hook management
[INFO] Both tools should work together without conflicts
[INFO] ========================
```

### Fallback Mode

```
[WARN] Failed to install hooks through SpecialK, falling back to compatibility mode
[WARN] === LIMITED FUNCTIONALITY MODE ===
[WARN] DirectX hooks disabled due to detected conflicts.
[WARN] Some Community Shaders features may not be available.
[WARN] Consider using SpecialK cooperation mode for full functionality.
[WARN] ====================================
```

## Benefits

1. **Seamless Integration**: Both tools work together without conflicts
2. **Full Functionality**: All features of both tools remain available
3. **Automatic Detection**: No manual configuration required for basic operation
4. **Graceful Fallback**: Maintains functionality even if cooperation fails
5. **User Choice**: Configurable behavior for different scenarios

## Edge Cases Handled

### DirectX 12 on DirectX 11 Engine

The system detects when Community Shaders uses DirectX 12 interop features on the DirectX 11 engine and can:

-   Attempt cooperation with additional validation
-   Fall back to limited functionality mode if conflicts arise
-   Provide specific warnings about mixed DirectX version scenarios

### Manual SpecialK Loading

Works with SpecialK's manual loading feature where SpecialK is loaded explicitly rather than through injection.

### Proxy DLL Detection

Detects SpecialK when it's loaded as a proxy DLL (dxgi.dll, d3d11.dll, dinput8.dll) rather than directly.

## Future Enhancements

This framework also enables detection and potential cooperation with other graphics tools:

-   ReShade detection and warnings
-   ENB Series conflict detection
-   Other graphics enhancement tools

The cooperation API approach provides a scalable foundation for resolving conflicts with multiple graphics enhancement tools in the Skyrim modding ecosystem.
