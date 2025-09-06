/*
 * Enhanced implementation for SpecialK cooperation in Community Shaders
 * This implementation includes both detection and cooperative hook management
 * 
 * File: src/Utils/CompatibilityDetection.h
 */

#pragma once

#include <Windows.h>
#include <string>

namespace Compatibility
{

	struct ConflictingTool
	{
		std::wstring name;
		std::vector<std::wstring> moduleNames;
		std::vector<std::string> exportNames;
		bool detected = false;
		bool cooperative = false;  // Can we cooperate with this tool?
	};

	// SpecialK cooperation API function pointers
	typedef DWORD(WINAPI* SK_GetDLLRole_pfn)();
	typedef HMODULE(WINAPI* SK_GetDLL_pfn)();
	typedef BOOL(WINAPI* SK_CreateFuncHook_pfn)(LPCWSTR pwszFuncName, LPVOID pTarget, LPVOID pDetour, LPVOID* ppOriginal);
	typedef BOOL(WINAPI* SK_EnableHook_pfn)(LPVOID pTarget);
	typedef BOOL(WINAPI* SK_DisableHook_pfn)(LPVOID pTarget);
	typedef BOOL(WINAPI* SK_RemoveHook_pfn)(LPVOID pTarget);
	typedef VOID(WINAPI* SK_ApplyQueuedHooks_pfn)();

	struct SpecialKAPI
	{
		SK_GetDLLRole_pfn GetDLLRole = nullptr;
		SK_GetDLL_pfn GetDLL = nullptr;
		SK_CreateFuncHook_pfn CreateFuncHook = nullptr;
		SK_EnableHook_pfn EnableHook = nullptr;
		SK_DisableHook_pfn DisableHook = nullptr;
		SK_RemoveHook_pfn RemoveHook = nullptr;
		SK_ApplyQueuedHooks_pfn ApplyQueuedHooks = nullptr;
		bool available = false;
	};

	class CompatibilityChecker
	{
	public:
		static CompatibilityChecker* GetSingleton()
		{
			static CompatibilityChecker instance;
			return &instance;
		}

		bool DetectSpecialK();
		bool InitializeSpecialKCooperation();
		bool DetectReShade();
		bool DetectENB();
		void CheckAllConflicts();
		void LogWarnings();
		bool ShouldSkipDirectXHooks();
		bool ShouldUseCooperativeHooks();

		// SpecialK cooperation methods
		bool CreateHookThroughSpecialK(LPCWSTR functionName, LPVOID target, LPVOID detour, LPVOID* original);
		const SpecialKAPI& GetSpecialKAPI() const { return specialK_API; }

	private:
		std::vector<ConflictingTool> knownConflicts;
		SpecialKAPI specialK_API;
		void InitializeConflictDatabase();
		bool CheckToolPresence(const ConflictingTool& tool);
		bool LoadSpecialKAPI(HMODULE hModule);
	};

	// Utility functions
	bool IsModuleSpecialK(HMODULE module);
	std::wstring GetModuleDescription(HMODULE module);
}

/*
 * Enhanced implementation for SpecialK cooperation in Community Shaders  
 * This implementation includes both detection and cooperative hook management
 * 
 * File: src/Utils/CompatibilityDetection.cpp
 */

#include "CompatibilityDetection.h"
#include <logger.h>

namespace Compatibility
{

	void CompatibilityChecker::InitializeConflictDatabase()
	{
		// SpecialK detection
		ConflictingTool specialK;
		specialK.name = L"SpecialK";
		specialK.moduleNames = {
			L"SpecialK64.dll",
			L"SpecialK32.dll",
			L"dxgi.dll",    // Common SpecialK proxy
			L"d3d11.dll",   // Possible SpecialK proxy
			L"dinput8.dll"  // Another SpecialK proxy option
		};
		specialK.exportNames = {
			"SKX_GetCommandProcessor",
			"SK_GetDLLRole",
			"SK_GetCommandProcessor",
			"SK_CreateFuncHook",  // Key cooperation API
			"SK_EnableHook",
			"SK_DisableHook"
		};
		specialK.cooperative = true;  // SpecialK supports cooperation
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
		reshade.cooperative = false;  // ReShade doesn't have cooperation API
		knownConflicts.push_back(reshade);

		// ENB Series detection
		ConflictingTool enb;
		enb.name = L"ENB Series";
		enb.moduleNames = {
			L"d3d11.dll",
			L"dxgi.dll"
		};
		enb.cooperative = false;  // ENB doesn't have cooperation API
		// ENB doesn't export specific functions, rely on file characteristics
		knownConflicts.push_back(enb);
	}

	bool CompatibilityChecker::DetectSpecialK()
	{
		for (const auto& moduleName : { L"SpecialK64.dll", L"SpecialK32.dll" }) {
			HMODULE handle = GetModuleHandle(moduleName);
			if (handle && IsModuleSpecialK(handle)) {
				return true;
			}
		}

		// Check for proxy DLLs that might be SpecialK
		for (const auto& proxyName : { L"dxgi.dll", L"d3d11.dll", L"dinput8.dll" }) {
			HMODULE handle = GetModuleHandle(proxyName);
			if (handle && IsModuleSpecialK(handle)) {
				return true;
			}
		}

		return false;
	}

	bool CompatibilityChecker::InitializeSpecialKCooperation()
	{
		HMODULE hSpecialK = nullptr;

		// Try to find SpecialK module
		for (const auto& moduleName : { L"SpecialK64.dll", L"SpecialK32.dll" }) {
			hSpecialK = GetModuleHandle(moduleName);
			if (hSpecialK && IsModuleSpecialK(hSpecialK)) {
				break;
			}
			hSpecialK = nullptr;
		}

		// Check for proxy DLLs
		if (!hSpecialK) {
			for (const auto& proxyName : { L"dxgi.dll", L"d3d11.dll", L"dinput8.dll" }) {
				hSpecialK = GetModuleHandle(proxyName);
				if (hSpecialK && IsModuleSpecialK(hSpecialK)) {
					break;
				}
				hSpecialK = nullptr;
			}
		}

		if (hSpecialK) {
			return LoadSpecialKAPI(hSpecialK);
		}

		return false;
	}

	bool CompatibilityChecker::LoadSpecialKAPI(HMODULE hModule)
	{
		specialK_API.GetDLLRole = reinterpret_cast<SK_GetDLLRole_pfn>(GetProcAddress(hModule, "SK_GetDLLRole"));
		specialK_API.GetDLL = reinterpret_cast<SK_GetDLL_pfn>(GetProcAddress(hModule, "SK_GetDLL"));
		specialK_API.CreateFuncHook = reinterpret_cast<SK_CreateFuncHook_pfn>(GetProcAddress(hModule, "SK_CreateFuncHook"));
		specialK_API.EnableHook = reinterpret_cast<SK_EnableHook_pfn>(GetProcAddress(hModule, "SK_EnableHook"));
		specialK_API.DisableHook = reinterpret_cast<SK_DisableHook_pfn>(GetProcAddress(hModule, "SK_DisableHook"));
		specialK_API.RemoveHook = reinterpret_cast<SK_RemoveHook_pfn>(GetProcAddress(hModule, "SK_RemoveHook"));
		specialK_API.ApplyQueuedHooks = reinterpret_cast<SK_ApplyQueuedHooks_pfn>(GetProcAddress(hModule, "SK_ApplyQueuedHooks"));

		specialK_API.available = (specialK_API.CreateFuncHook && specialK_API.EnableHook &&
								  specialK_API.DisableHook && specialK_API.GetDLLRole);

		if (specialK_API.available) {
			logger::info("SpecialK cooperation API successfully loaded");
			DWORD role = specialK_API.GetDLLRole();
			logger::info("SpecialK DLL Role: {}", role);
		}

		return specialK_API.available;
	}

	bool CompatibilityChecker::CreateHookThroughSpecialK(LPCWSTR functionName, LPVOID target, LPVOID detour, LPVOID* original)
	{
		if (!specialK_API.available) {
			return false;
		}

		if (specialK_API.CreateFuncHook(functionName, target, detour, original)) {
			if (specialK_API.EnableHook(target)) {
				logger::info(L"Successfully created hook through SpecialK: {}", functionName);
				return true;
			} else {
				logger::warn(L"Failed to enable hook through SpecialK: {}", functionName);
				if (specialK_API.RemoveHook) {
					specialK_API.RemoveHook(target);
				}
			}
		} else {
			logger::warn(L"Failed to create hook through SpecialK: {}", functionName);
		}

		return false;
	}

	bool CompatibilityChecker::IsModuleSpecialK(HMODULE module)
	{
		if (!module)
			return false;

		// Check for SpecialK-specific exports
		const char* specialKExports[] = {
			"SKX_GetCommandProcessor",
			"SK_GetDLLRole",
			"SK_GetCommandProcessor",
			"SK_GetFramerate",
			"SK_CreateFuncHook"  // Key indicator of cooperation support
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

	std::wstring CompatibilityChecker::GetModuleDescription(HMODULE module)
	{
		wchar_t path[MAX_PATH];
		if (!GetModuleFileName(module, path, MAX_PATH)) {
			return L"";
		}

		DWORD dummy;
		DWORD size = GetFileVersionInfoSize(path, &dummy);
		if (size == 0)
			return L"";

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

	void CompatibilityChecker::CheckAllConflicts()
	{
		if (knownConflicts.empty()) {
			InitializeConflictDatabase();
		}

		for (auto& tool : knownConflicts) {
			tool.detected = CheckToolPresence(tool);
			if (tool.detected && tool.name == L"SpecialK" && tool.cooperative) {
				// Try to initialize cooperation
				InitializeSpecialKCooperation();
			}
		}
	}

	bool CompatibilityChecker::CheckToolPresence(const ConflictingTool& tool)
	{
		for (const auto& moduleName : tool.moduleNames) {
			HMODULE handle = GetModuleHandle(moduleName.c_str());
			if (!handle)
				continue;

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

	void CompatibilityChecker::LogWarnings()
	{
		bool foundConflicts = false;
		bool cooperationAvailable = false;

		for (const auto& tool : knownConflicts) {
			if (tool.detected) {
				foundConflicts = true;

				if (tool.cooperative && tool.name == L"SpecialK" && specialK_API.available) {
					logger::info(L"Detected cooperative tool: {} - cooperation mode available", tool.name);
					cooperationAvailable = true;
				} else {
					logger::warn(L"Detected conflicting tool: {}", tool.name);
					logger::warn("This may cause DirectX hook conflicts and instability.");
				}
			}
		}

		if (foundConflicts && !cooperationAvailable) {
			logger::warn("=== COMPATIBILITY WARNING ===");
			logger::warn("Multiple graphics enhancement tools detected!");
			logger::warn("Consider using only one tool at a time to avoid conflicts.");
			logger::warn("If issues occur, try disabling other graphics mods.");
			logger::warn("=============================");
		} else if (cooperationAvailable) {
			logger::info("=== COOPERATION MODE ===");
			logger::info("SpecialK cooperation mode enabled - using shared hook management");
			logger::info("Both tools should work together without conflicts");
			logger::info("========================");
		}
	}

	bool CompatibilityChecker::ShouldSkipDirectXHooks()
	{
		CheckAllConflicts();

		// Skip direct hooks if we're using SpecialK cooperation
		if (ShouldUseCooperativeHooks()) {
			return true;
		}

		// Skip hooks if SpecialK is detected and compatibility mode is enabled
		for (const auto& tool : knownConflicts) {
			if (tool.detected && tool.name == L"SpecialK" && !tool.cooperative) {
				// Check if compatibility mode is enabled in config
				return GetPrivateProfileIntW(L"Compatibility", L"SkipHooksWithSpecialK", 0,
						   L"./CommunityShaders.ini") != 0;
			}
		}
		return false;
	}

	bool CompatibilityChecker::ShouldUseCooperativeHooks()
	{
		return specialK_API.available && GetPrivateProfileIntW(L"Compatibility", L"UseSpecialKCooperation", 1,
											 L"./CommunityShaders.ini") != 0;
	}
}

/*
 * Enhanced integration into existing Hooks.cpp with SpecialK cooperation
 * Add this to the InstallD3DHooks function
 */

/*
#include "Utils/CompatibilityDetection.h"

void InstallD3DHooks()
{
    auto* compatibility = Compatibility::CompatibilityChecker::GetSingleton();
    compatibility->CheckAllConflicts();
    compatibility->LogWarnings();

    // Attempt SpecialK cooperation first
    if (compatibility->ShouldUseCooperativeHooks()) {
        logger::info("Using SpecialK cooperation mode for DirectX hooks");
        
        // Use SpecialK's hook management API
        const auto& skAPI = compatibility->GetSpecialKAPI();
        
        if (skAPI.available) {
            // Install hooks through SpecialK
            bool success = true;
            
            success &= compatibility->CreateHookThroughSpecialK(
                L"D3D11CreateDeviceAndSwapChain",
                GetProcAddress(GetModuleHandle(L"d3d11.dll"), "D3D11CreateDeviceAndSwapChain"),
                hk_D3D11CreateDeviceAndSwapChain,
                (LPVOID*)&ptrD3D11CreateDeviceAndSwapChain
            );
            
            success &= compatibility->CreateHookThroughSpecialK(
                L"CreateDXGIFactory1", 
                GetProcAddress(GetModuleHandle(L"dxgi.dll"), 
                               !REL::Module::IsVR() ? "CreateDXGIFactory" : "CreateDXGIFactory1"),
                hk_CreateDXGIFactory,
                (LPVOID*)&ptrCreateDXGIFactory
            );
            
            if (success) {
                logger::info("Successfully installed DirectX hooks through SpecialK cooperation");
                // Apply queued hooks if function is available
                if (skAPI.ApplyQueuedHooks) {
                    skAPI.ApplyQueuedHooks();
                }
                
                // Still initialize FidelityFX, but it will work through the cooperative hooks
                globals::fidelityFX->LoadFFX();
                return;
            } else {
                logger::warn("Failed to install hooks through SpecialK, falling back to compatibility mode");
            }
        }
    }

    // Fallback: Check if we should skip direct hooks due to conflicts
    if (compatibility->ShouldSkipDirectXHooks()) {
        logger::info("Skipping DirectX hooks due to compatibility mode");
        
        // Load FidelityFX in limited mode without DirectX hooks
        globals::fidelityFX->LoadFFX();
        
        // Show user notification about limited functionality
        logger::warn("=== LIMITED FUNCTIONALITY MODE ===");
        logger::warn("DirectX hooks disabled due to detected conflicts.");
        logger::warn("Some Community Shaders features may not be available.");
        logger::warn("Consider using only one graphics enhancement tool for full functionality.");
        logger::warn("====================================");
        return;
    }

    // Standard mode: Install hooks directly (original behavior)
    logger::info("Installing DirectX hooks in standard mode");
    globals::fidelityFX->LoadFFX();
    *(uintptr_t*)&ptrD3D11CreateDeviceAndSwapChain = SKSE::PatchIAT(hk_D3D11CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain");
    *(uintptr_t*)&ptrCreateDXGIFactory = SKSE::PatchIAT(hk_CreateDXGIFactory, "dxgi.dll", !REL::Module::IsVR() ? "CreateDXGIFactory" : "CreateDXGIFactory1");
}
*/