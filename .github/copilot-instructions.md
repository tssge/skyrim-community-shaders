# Skyrim Community Shaders

SKSE core plugin providing community-driven advanced graphics modifications for Skyrim.

**CRITICAL**: Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

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
