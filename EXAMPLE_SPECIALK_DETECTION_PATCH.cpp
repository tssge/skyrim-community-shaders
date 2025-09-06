/*
 * Example implementation for SpecialK detection in Community Shaders
 * This is a demonstration patch showing how to detect and warn about SpecialK conflicts
 * 
 * File: src/Utils/CompatibilityDetection.h
 */

#pragma once

#include <Windows.h>
#include <string>

namespace Compatibility {
    
    struct ConflictingTool {
        std::wstring name;
        std::vector<std::wstring> moduleNames;
        std::vector<std::string> exportNames;
        bool detected = false;
    };

    class CompatibilityChecker {
    public:
        static CompatibilityChecker* GetSingleton() {
            static CompatibilityChecker instance;
            return &instance;
        }

        bool DetectSpecialK();
        bool DetectReShade();
        bool DetectENB();
        void CheckAllConflicts();
        void LogWarnings();
        bool ShouldSkipDirectXHooks();

    private:
        std::vector<ConflictingTool> knownConflicts;
        void InitializeConflictDatabase();
        bool CheckToolPresence(const ConflictingTool& tool);
    };

    // Utility functions
    bool IsModuleSpecialK(HMODULE module);
    std::wstring GetModuleDescription(HMODULE module);
}

/*
 * Example implementation for SpecialK detection in Community Shaders  
 * This is a demonstration patch showing how to detect and warn about SpecialK conflicts
 * 
 * File: src/Utils/CompatibilityDetection.cpp
 */

#include "CompatibilityDetection.h"
#include <logger.h>

namespace Compatibility {

    void CompatibilityChecker::InitializeConflictDatabase() {
        // SpecialK detection
        ConflictingTool specialK;
        specialK.name = L"SpecialK";
        specialK.moduleNames = {
            L"SpecialK64.dll",
            L"SpecialK32.dll",
            L"dxgi.dll",        // Common SpecialK proxy
            L"d3d11.dll",       // Possible SpecialK proxy
            L"dinput8.dll"      // Another SpecialK proxy option
        };
        specialK.exportNames = {
            "SKX_GetCommandProcessor",
            "SK_GetDLLRole",
            "SK_GetCommandProcessor"
        };
        knownConflicts.push_back(specialK);

        // ReShade detection
        ConflictingTool reshade;
        reshade.name = L"ReShade";
        reshade.moduleNames = {
            L"dxgi.dll",
            L"d3d11.dll", 
            L"d3d12.dll",
            L"opengl32.dll"
        };
        reshade.exportNames = {
            "ReShadeRegisterAddon",
            "ReShadeUnregisterAddon"
        };
        knownConflicts.push_back(reshade);

        // ENB Series detection  
        ConflictingTool enb;
        enb.name = L"ENB Series";
        enb.moduleNames = {
            L"d3d11.dll",
            L"dxgi.dll"
        };
        // ENB doesn't export specific functions, rely on file characteristics
        knownConflicts.push_back(enb);
    }

    bool CompatibilityChecker::DetectSpecialK() {
        for (const auto& moduleName : {L"SpecialK64.dll", L"SpecialK32.dll"}) {
            HMODULE handle = GetModuleHandle(moduleName);
            if (handle && IsModuleSpecialK(handle)) {
                return true;
            }
        }

        // Check for proxy DLLs that might be SpecialK
        for (const auto& proxyName : {L"dxgi.dll", L"d3d11.dll", L"dinput8.dll"}) {
            HMODULE handle = GetModuleHandle(proxyName);
            if (handle && IsModuleSpecialK(handle)) {
                return true;
            }
        }

        return false;
    }

    bool CompatibilityChecker::IsModuleSpecialK(HMODULE module) {
        if (!module) return false;

        // Check for SpecialK-specific exports
        const char* specialKExports[] = {
            "SKX_GetCommandProcessor",
            "SK_GetDLLRole", 
            "SK_GetCommandProcessor",
            "SK_GetFramerate"
        };

        for (const auto& exportName : specialKExports) {
            if (GetProcAddress(module, exportName)) {
                return true;
            }
        }

        // Check module description/version info
        std::wstring description = GetModuleDescription(module);
        if (description.find(L"Special K") != std::wstring::npos ||
            description.find(L"SpecialK") != std::wstring::npos) {
            return true;
        }

        return false;
    }

    std::wstring CompatibilityChecker::GetModuleDescription(HMODULE module) {
        wchar_t path[MAX_PATH];
        if (!GetModuleFileName(module, path, MAX_PATH)) {
            return L"";
        }

        DWORD dummy;
        DWORD size = GetFileVersionInfoSize(path, &dummy);
        if (size == 0) return L"";

        std::vector<BYTE> buffer(size);
        if (!GetFileVersionInfo(path, 0, size, buffer.data())) {
            return L"";
        }

        VS_FIXEDFILEINFO* fileInfo;
        UINT len;
        if (VerQueryValue(buffer.data(), L"\\", (LPVOID*)&fileInfo, &len)) {
            // Additional version checks could go here
        }

        // Get description string
        LPWSTR description;
        UINT descLen;
        if (VerQueryValue(buffer.data(), L"\\StringFileInfo\\040904b0\\FileDescription", 
                         (LPVOID*)&description, &descLen)) {
            return std::wstring(description, descLen - 1);
        }

        return L"";
    }

    void CompatibilityChecker::CheckAllConflicts() {
        if (knownConflicts.empty()) {
            InitializeConflictDatabase();
        }

        for (auto& tool : knownConflicts) {
            tool.detected = CheckToolPresence(tool);
        }
    }

    bool CompatibilityChecker::CheckToolPresence(const ConflictingTool& tool) {
        for (const auto& moduleName : tool.moduleNames) {
            HMODULE handle = GetModuleHandle(moduleName.c_str());
            if (!handle) continue;

            // For SpecialK, use specialized detection
            if (tool.name == L"SpecialK") {
                return IsModuleSpecialK(handle);
            }

            // For other tools, check exports
            for (const auto& exportName : tool.exportNames) {
                if (GetProcAddress(handle, exportName.c_str())) {
                    return true;
                }
            }

            // Check file description
            std::wstring description = GetModuleDescription(handle);
            if (description.find(tool.name) != std::wstring::npos) {
                return true;
            }
        }
        return false;
    }

    void CompatibilityChecker::LogWarnings() {
        bool foundConflicts = false;

        for (const auto& tool : knownConflicts) {
            if (tool.detected) {
                foundConflicts = true;
                logger::warn(L"Detected conflicting tool: {}", tool.name);
                logger::warn("This may cause DirectX hook conflicts and instability.");
            }
        }

        if (foundConflicts) {
            logger::warn("=== COMPATIBILITY WARNING ===");
            logger::warn("Multiple graphics enhancement tools detected!");
            logger::warn("Consider using only one tool at a time to avoid conflicts.");
            logger::warn("If issues occur, try disabling other graphics mods.");
            logger::warn("=============================");
        }
    }

    bool CompatibilityChecker::ShouldSkipDirectXHooks() {
        CheckAllConflicts();
        
        // Skip hooks if SpecialK is detected and compatibility mode is enabled
        for (const auto& tool : knownConflicts) {
            if (tool.detected && tool.name == L"SpecialK") {
                // Check if compatibility mode is enabled in config
                return GetPrivateProfileIntW(L"Compatibility", L"SkipHooksWithSpecialK", 0, 
                                           L"./CommunityShaders.ini") != 0;
            }
        }
        return false;
    }
}

/*
 * Example integration into existing Hooks.cpp
 * Add this to the InstallD3DHooks function
 */

/*
#include "Utils/CompatibilityDetection.h"

void InstallD3DHooks()
{
    auto* compatibility = Compatibility::CompatibilityChecker::GetSingleton();
    compatibility->CheckAllConflicts();
    compatibility->LogWarnings();

    if (compatibility->ShouldSkipDirectXHooks()) {
        logger::info("Skipping DirectX hooks due to compatibility mode");
        return;
    }

    // Original hook installation code
    globals::fidelityFX->LoadFFX();
    *(uintptr_t*)&ptrD3D11CreateDeviceAndSwapChain = SKSE::PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    *(uintptr_t*)&ptrCreateDXGIFactory = SKSE::PatchIAT(hk_CreateDXGIFactory, "dxgi.dll", !REL::Module::IsVR() ? "CreateDXGIFactory" : "CreateDXGIFactory1");
}
*/