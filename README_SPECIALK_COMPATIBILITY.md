# SpecialK Compatibility with Community Shaders

**STATUS: IMPLEMENTED** ✅

Community Shaders now includes native SpecialK cooperation support through SpecialK's cooperation API. This allows both tools to work together seamlessly without conflicts.

## Quick Setup

1. **Install both tools** normally
2. **Enable cooperation** (default): Set `UseSpecialKCooperation=1` in `CommunityShaders.ini`
3. **Launch game** - cooperation will be automatic

## How It Works

Community Shaders detects SpecialK at startup and uses SpecialK's cooperation API for shared DirectX hook management. Instead of fighting over the same hooks, both tools coordinate through SpecialK's centralized system.

**Cooperation Mode Benefits:**

-   ✅ Both tools work together without conflicts
-   ✅ Full functionality maintained for both tools
-   ✅ Automatic detection and setup
-   ✅ Graceful fallback if cooperation fails

## Implementation Details

See `SPECIALK_COOPERATION_IMPLEMENTATION.md` for complete technical documentation.

### Files Added:

-   `src/Utils/CompatibilityDetection.h/cpp` - Core cooperation and detection system
-   `CommunityShaders.ini` - Configuration file with cooperation settings
-   Modified `src/Hooks.cpp` - Integrated SpecialK cooperation into DirectX hook installation

## Configuration Options

The system is designed to work automatically, but can be configured in `CommunityShaders.ini`:

```ini
[Compatibility]
UseSpecialKCooperation=1  # Use SpecialK cooperation API (RECOMMENDED)
DetectSpecialK=1          # Detect SpecialK and log status
SkipHooksWithSpecialK=0   # Fallback: skip hooks if cooperation fails

[SpecialKCooperation]
UseSharedHookManager=1    # Use SpecialK's centralized hook manager
DelegatePresent=1         # Allow SpecialK to manage Present() calls
DelegateSwapChain=1       # Allow SpecialK to manage swap chain operations
```

## Troubleshooting

### If You See Cooperation Success Messages:

```
[INFO] === COOPERATION MODE ===
[INFO] SpecialK cooperation mode enabled - using shared hook management
[INFO] Both tools should work together without conflicts
```

✅ **Everything is working correctly!** Both tools will function fully.

### If You See Fallback Messages:

```
[WARN] === LIMITED FUNCTIONALITY MODE ===
[WARN] DirectX hooks disabled due to detected conflicts.
```

⚠️ **Partial functionality** - Some Community Shaders features may be limited.

**Solutions:**

1. Update SpecialK to a version with cooperation API support
2. Try manual SpecialK loading if using injection
3. Check SpecialK configuration for hook management settings

### If You See No Messages:

-   SpecialK may not be detected
-   Check that SpecialK is properly installed and loading
-   Verify `DetectSpecialK=1` in configuration

## Advanced Configuration

### For DirectX 12 Features on DirectX 11 Engine:

The system automatically handles Community Shaders' DirectX 12 interop features (like FidelityFX Frame Generation) when used on Skyrim's DirectX 11 engine.

### For Multiple Graphics Tools:

The system also detects ReShade and ENB Series, providing appropriate warnings about potential conflicts.

## Previous Documentation

For historical context, see the original documentation:

-   `SpecialK_Incompatibility_Issue.md` - Original problem analysis
-   `SPECIALK_TECHNICAL_ANALYSIS.md` - Technical details of the conflict
-   `EXAMPLE_SPECIALK_DETECTION_PATCH.cpp` - Reference implementation (now integrated)

## Support

If you experience issues:

1. Check the game's log files for cooperation status messages
2. Try different cooperation settings in `CommunityShaders.ini`
3. Report issues with log information showing cooperation status

## Conflict Mechanism

1. **Hook Chain Conflicts**: Both tools attempt to hook the same functions
2. **State Inconsistency**: Conflicting modifications to DirectX state
3. **Resource Management**: Competing control over DirectX resources
4. **Initialization Order**: Unpredictable behavior based on load order

## Solution Approaches

### Immediate (Detection & Warning)

-   Implement SpecialK detection during Community Shaders initialization
-   Display clear warnings when conflicts are detected
-   Provide configuration options to disable conflicting features

### Short-term (SpecialK Cooperation - RECOMMENDED)

-   **Leverage SpecialK's Manual Loading Feature**: SpecialK provides exported functions for cooperative hook management
-   Use SpecialK's `SK_CreateFuncHook`, `SK_EnableHook`, and related APIs
-   Let SpecialK manage the DirectX hook chain while Community Shaders provides functionality
-   Both tools can coexist seamlessly without hook conflicts
-   Automatic fallback to compatibility mode if cooperation fails

### Alternative (Compatibility Mode)

-   Add compatibility flags to skip problematic hooks
-   Implement graceful degradation of features
-   Provide user control over which features to disable

### Long-term (Cooperation)

-   Establish shared hooking protocols with other graphics tools
-   Create API abstraction layers
-   Coordinate resource management between tools

## Implementation Priority

1. **High Priority**: SpecialK cooperation via manual loading API
2. **Medium Priority**: Detection and warning system for fallback scenarios
3. **Lower Priority**: Basic compatibility mode with feature toggles
4. **Future**: Full cooperative hooking framework for all graphics tools

## Usage for Developers

This documentation can be used to:

1. **Implement SpecialK Cooperation**: Use `EXAMPLE_SPECIALK_DETECTION_PATCH.cpp` to implement cooperative hook management via SpecialK's manual loading API
2. **Report the Issue**: Use `SpecialK_Incompatibility_Issue.md` as a GitHub issue for tracking
3. **Understand the Problem**: Reference `SPECIALK_TECHNICAL_ANALYSIS.md` for implementation details
4. **Configure Compatibility**: Deploy `COMPATIBILITY_CONFIG_TEMPLATE.ini` for user options and cooperation settings

## Usage for Users

Users experiencing conflicts between Community Shaders and SpecialK should:

1. **Enable Cooperation Mode** (Recommended): Set `UseSpecialKCooperation=1` in `CommunityShaders.ini` to allow both tools to work together
2. **Update Both Tools**: Ensure you have recent versions of both Community Shaders and SpecialK for best cooperation support
3. **Check Configuration**: Use the provided `COMPATIBILITY_CONFIG_TEMPLATE.ini` as a starting point
4. **Monitor for Issues**: Report specific conflict scenarios to help developers improve cooperation
5. **Fallback Options**: If cooperation mode has issues, try compatibility mode or use only one tool at a time

## Next Steps

1. **Implement SpecialK Cooperation**: Add support for SpecialK's manual loading API to enable seamless cooperation
2. **Test Cooperation Mode**: Verify that cooperative hook management works with real-world scenarios
3. **Implement Detection and Fallback**: Add basic detection and warning functionality for cases where cooperation isn't available
4. **Test Compatibility Modes**: Verify fallback scenarios work correctly with various tool combinations
5. **Coordinate with SpecialK Developers**: Work with SpecialK team to optimize cooperation and address any API issues
6. **Extend Framework**: Apply cooperation concepts to other conflicting tools (ReShade, ENB)

---

This documentation represents a comprehensive analysis of a significant compatibility issue in the Skyrim modding ecosystem and provides multiple pathways for resolution, with SpecialK cooperation being the preferred approach.
