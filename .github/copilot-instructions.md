# Skyrim Community Shaders

SKSE core plugin providing community-driven advanced graphics modifications for Skyrim.

**CRITICAL**: Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Project Context and Priorities

This repository houses Skyrim Community Shaders, dedicated to providing high-quality, performant visual enhancements for Skyrim (AE, SE, VR).

Our priorities are (in order):

1. **Stability:** Code must not cause Crash-to-Desktop (CTD). We operate within the game's process; stability is paramount.
2. **Performance:** Shaders and C++ logic must be highly optimized. We often prioritize performance over textbook readability, provided the code remains maintainable.
3. **Compatibility:** Must work across various hardware and adhere strictly to CommonLibSSE/VR patterns.
4. **Visual Quality:** Implementation of advanced rendering techniques.

## Working Effectively

### Prerequisites and Environment Setup

- **Windows ONLY**: This project requires Windows and cannot be built on Linux/macOS.
- Install Visual Studio Community 2022 with "Desktop development with C++" workload.
- Install CMake and add cmake.exe to PATH environment variable.
- Install Git and add Git.exe to PATH environment variable.
- Install vcpkg following their [Quick Start Guide](https://github.com/microsoft/vcpkg#quick-start-windows).
- Set environment variable `VCPKG_ROOT` to your vcpkg installation path.
- **CRITICAL**: Ensure your vcpkg baseline matches the commit in `vcpkg.json` (`98aa6396292d57e737a6ef999d4225ca488859d5`).

### Build Process

- **CRITICAL BUILD TIMING**: NEVER CANCEL builds. Set timeout to 60+ minutes.
- Clone: `git clone https://github.com/tssge/skyrim-community-shaders.git --recursive`
- Navigate: `cd skyrim-community-shaders`
- Build: `.\BuildRelease.bat` -- takes 15-25 minutes on typical hardware. NEVER CANCEL.
- Alternative Docker build available (see README) but requires Windows containers.

### CMake Configuration

- Multiple build presets available: `ALL`, `AE`, `SE`, `VR`, `PRE-AE`, `FLATRIM`
- `ALL` preset builds for all Skyrim variants (recommended for development)
- Custom presets can be added via `CMakeUserPresets.json` (copy from template)
- Build artifacts created in `/build/{preset}/` and `/dist/` directories

### Validation and Testing

- **NO TRADITIONAL UNIT TESTS**: This is a native Skyrim plugin - validation is build success + game loading.
- Run formatting: `clang-format --dry-run --Werror [files]` -- takes < 1 second
- JSON validation: `python -m json.tool [file]` for CMake/vcpkg configs -- takes < 1 second
- Pre-commit hooks available: `pre-commit run --all-files` -- takes 30-60 seconds
- **NEVER CANCEL**: Format checking via GitHub Actions takes 2-5 minutes
- **CI Validation**: `.github/workflows/build.yaml` runs full build - takes 20-30 minutes

### Cross-Platform Development

- **Code inspection/formatting**: Can be done on Linux/macOS using clang-format and prettier
- **Building**: Windows-only due to MSVC/DirectX dependencies
- Use WSL or Docker on non-Windows for code formatting/linting only

## Repository Structure and Navigation

### Key Directories (70 C++ files, 73 headers, 37 shaders, 31 features)

```
├── src/                     # Core plugin source (main development area)
│   ├── Features/            # Individual graphics features (54 files)
│   ├── ShaderTools/         # Shader compilation utilities
│   ├── Utils/               # Helper utilities
│   └── *.cpp/*.h           # Core plugin files (Globals, State, Menu, etc.)
├── features/               # Feature-specific shaders and configs (31 features)
├── package/                # Distribution package structure
│   ├── SKSE/Plugins/       # Where built DLL goes
│   └── Shaders/            # Base shader files
├── cmake/                  # CMake configuration and utilities
└── .github/workflows/      # CI/CD pipelines
```

### Common Development Patterns

- **Adding new features**: Create in `src/Features/` + corresponding `features/[FeatureName]/` directory
- **Shader modifications**: Edit `.hlsl` files in `features/[FeatureName]/Shaders/`
- **Core changes**: Modify files in `src/` root (State.cpp, Menu.cpp, etc.)
- **Feature configuration**: Edit `.ini` files in `features/[FeatureName]/Shaders/Features/`

### Build Outputs

- Debug DLL: `build/{preset}/CommunityShaders.dll`
- Release archives: `dist/CommunityShaders_AIO-{timestamp}.7z` (all features)
- Individual feature archives: `dist/{FeatureName}-{timestamp}.7z`

## Common Tasks

### Development Workflow

1. Make code changes in `src/` or `features/`
2. Run formatting: `clang-format -i [specific changed files only]` -- takes < 5 seconds
3. Build: `.\BuildRelease.bat` -- takes 15-25 minutes. NEVER CANCEL. Set timeout to 60+ minutes.
4. Test plugin loads in Skyrim (Windows game required)
5. Run pre-commit: `pre-commit run --all-files` -- takes 30-60 seconds

### Change Scope Management

- **CRITICAL**: Keep changes focused on the specific issue or feature being implemented
- **NEVER** make unrelated changes, formatting fixes, or refactoring outside the scope of your task
- **NEVER** run broad formatting commands that affect multiple unrelated files
- If you discover formatting issues in unrelated files, ignore them - they are not your responsibility to fix
- Use `git status` and `git diff` frequently to ensure only intended files are modified
- Each PR should have a clear, minimal scope focused on one specific issue or feature

### Code Style and Formatting

- **CRITICAL SCOPE RULE**: ONLY format files that are directly related to your changes. NEVER run formatting tools on the entire repository or unrelated files.
- **ALWAYS** run `clang-format -i [specific changed files only]` before committing - specify exact file paths
- **ALWAYS** run `npx prettier --write [specific files]` for JSON/YAML files you're modifying
- **NEVER** run `clang-format -i src/` or broad directory formatting commands that affect unrelated files
- **NEVER** run `npx prettier --write .` or similar broad commands that format the entire repository
- Pre-commit hooks enforce formatting automatically
- CI will fail if formatting is incorrect

### Build Troubleshooting

- **vcpkg issues**: Verify `VCPKG_ROOT` environment variable and baseline commit match
- **CMake errors**: Ensure Visual Studio 2022 C++ tools installed and vcvarsall.bat accessible
- **Dependency issues**: Run `git submodule update --init --recursive`
- **Clean build**: Delete `build/` directory and rebuild

### Key Files to Monitor

- `CMakeLists.txt`: Build configuration changes
- `vcpkg.json`: Dependency updates (requires vcpkg baseline sync)
- `src/Feature.h`: Base feature interface
- `src/State.cpp`: Core plugin state management
- `cmake/FeatureVersions.h.in`: Auto-generated feature version tracking

## Validation Scenarios

### Build Validation

- **Success criteria**: Build completes without errors, DLL and 7z archives created
- **Timing**: 15-25 minutes typical, up to 40 minutes on slower hardware
- **Outputs to verify**: `build/{preset}/CommunityShaders.dll` and `dist/*.7z` files exist

### Code Quality Validation

- All C++/header files pass clang-format without changes
- All JSON files validate with `python -m json.tool`
- No build warnings in Release configuration
- Pre-commit hooks pass completely

### Feature Validation

- New features have corresponding `.ini` config in `features/[Name]/Shaders/Features/`
- Feature versions auto-detected from `.ini` files during build
- Shader files compile without HLSL syntax errors
- Plugin loads in Skyrim without crashes (requires game testing)

**REMINDER**: NEVER CANCEL long-running builds or CI workflows. Build times of 15-40 minutes are normal for this complex DirectX/HLSL project.

## C++ Style and Conventions

### Code Style

- **Braces:** Allman style (braces on new lines) enforced by clang-format
- **Naming:** `m_` prefix for member variables, PascalCase for types and functions
- **Indentation:** 4 spaces, tabs for alignment (configured in .clang-format)
- **Line endings:** CRLF (Windows standard)

### C++ Constraints and Frameworks

- **STRICTLY NO:** STL containers (e.g., `std::vector`, `std::string`), exceptions, or RTTI in performance-critical paths or code that interacts with the game engine.
- **Framework Adherence:** Heavily prioritize `RE/SkyrimSE` structures (e.g., `RE::BSTArray`) for engine interaction and `CommonLibSSE` patterns for architecture. Do not introduce patterns that deviate from these frameworks.
- **Version Independence:** Always use CommonLibSSE/VR and the Address Library. **Never** use hardcoded memory offsets.
- **Pointer Safety:** Treat all pointers retrieved from the game engine (`RE::*`) as potentially invalid. Always null-check before dereferencing. Be extremely cautious with object lifetimes managed by the engine.
- **Memory Management:** Avoid standard `new`/`delete` unless wrapped in framework-provided managers. Adhere strictly to RAII principles where applicable within the constraints of the framework.

### Example: Preferred Framework Usage

**❌ Bad (Avoid):**

```cpp
// Using STL and raw pointers unsafely
std::vector<RE::Actor*> actors;
auto* player = RE::PlayerCharacter::GetSingleton();
actors.push_back(player); // No null check, STL usage
```

**✅ Good (Prefer):**

```cpp
// Using RE framework patterns with safety checks
RE::BSTArray<RE::Actor*> actors;
if (auto* player = RE::PlayerCharacter::GetSingleton()) {
    actors.push_back(player);
}
```

## HLSL/Shaders

### Code Style

- **Braces:** K&R style for HLSL (opening brace on same line) enforced by clang-format
- **Naming:** camelCase for variables and functions, PascalCase for types and textures
- **Precision:** Explicit precision qualifiers where performance matters

### HLSL Practices and Optimization

- **Performance First:** Prioritize Arithmetic Logic Unit (ALU) operations (calculations) over texture fetches (memory bandwidth) when feasible.
- **Precision:** Be mindful of floating-point precision. Use `float` (32-bit) for critical calculations (e.g., view/projection transforms, texture coordinates, temporal calculations). Use `half` (16-bit) only for optimization where precision loss is visually acceptable (e.g., normalized vectors, simple color storage).
- **Branching:** Prefer arithmetic solutions (e.g., `lerp`, `step`, `saturate`) over dynamic branching (`if` statements) in pixel shaders. Prefer static or constant-buffer-driven branching.
- **Intrinsics:** Utilize HLSL intrinsics (like `mad`, `rcp`, `rsq`) effectively.
- **Color Spaces:** Be rigorous about color space conversions. All lighting calculations MUST occur in linear space. Clearly comment conversions between linear and gamma/sRGB space.
- **Documentation:** For complex mathematical operations or novel rendering techniques, comments must explain the _why_. If implementing a technique from a paper (e.g., GDC presentation, academic journal), include a reference or link in the comments.

### Example: Prefer Arithmetic over Dynamic Branching

Dynamic 'if' statements based on per-pixel data cause GPU divergence and harm performance. Use interpolation or arithmetic instead.

**❌ Bad (Avoid):**

```hlsl
float4 CalculateColor(float2 uv) {
    float4 color = TextureA.Sample(Sampler, uv);
    if (uv.x > 0.5) {
        // Expensive divergent branch
        color *= 2.0;
    }
    return color;
}
```

**✅ Good (Prefer):**

```hlsl
float4 CalculateColor(float2 uv) {
    float4 color = TextureA.Sample(Sampler, uv);
    // Create a mask using step (returns 0 or 1)
    float multiplierMask = step(0.5, uv.x);
    // Use lerp to choose the multiplier (1.0 if false, 2.0 if true)
    float multiplier = lerp(1.0, 2.0, multiplierMask);
    color *= multiplier;
    return color;
}
```

## C++/HLSL Interface

- **Constant Buffer Alignment:** C++ structs and HLSL Constant Buffers (`cbuffer`) layouts must match exactly. Adhere strictly to HLSL 16-byte packing rules.
- **Verification:** Use `static_assert` in the C++ definition to verify alignment.
    - Example: `static_assert(sizeof(MyCBufferStruct) % 16 == 0, "Constant Buffer must be 16-byte aligned.");`
- **Padding:** Explicitly pad structs to maintain alignment. Use `float pad[N];` or similar for clarity.

### Example: Proper Constant Buffer Alignment

**❌ Bad (Avoid):**

```cpp
struct Settings {
    float opacity;        // 4 bytes
    bool enableFeature;   // 1 byte - misaligned!
};                       // Total: 5 bytes - not 16-byte aligned
```

**✅ Good (Prefer):**

```cpp
struct alignas(16) Settings {
    float opacity;        // 4 bytes
    float enableFeature;  // 4 bytes (use float instead of bool)
    float pad[2];         // 8 bytes padding
};                       // Total: 16 bytes - properly aligned
static_assert(sizeof(Settings) % 16 == 0, "Settings must be 16-byte aligned");
```

## Changes and Refactoring

- **Context is King:** Always prioritize the conventions and patterns found in the surrounding code over general best practices. Do not change styles mid-file.
- **Incremental Changes:** Prefer smaller, focused changes over large, sweeping refactors.
- **Adherence:** Ensure all refactored code strictly adheres to the constraints above. Do not introduce modern C++ features or libraries that violate the project's constraints during refactoring.
- **Performance Impact:** Consider the performance implications of any changes, especially in shader code or frequently-called C++ functions.
- **Framework Consistency:** When refactoring, ensure the code continues to follow CommonLibSSE/VR patterns and RE framework usage.
