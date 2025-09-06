/*
 * SpecialK Compatibility Detection and Cooperation API Implementation
 * 
 * This file implements SpecialK cooperation using the public API.
 * SpecialK is licensed under the GNU General Public License v3.0
 * Copyright (c) 2015-2024 Andon "Kaldaien" Coleman
 * 
 * Function signatures and cooperation concepts adapted from:
 * https://github.com/SpecialKO/SpecialK
 * 
 * Detection logic and API integration designed to work with SpecialK's
 * hook cooperation system for seamless DirectX compatibility.
 */

#include "CompatibilityDetection.h"

#include <filesystem>
#include <winver.h>
#pragma comment(lib, "version.lib")

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
			// Note: Proxy DLLs are validated via IsModuleSpecialK() to avoid false positives
			// In modded Skyrim, these could be ReShade, ENB, or other proxy DLLs
			L"dxgi.dll",    // Validated proxy - could be SpecialK
			L"d3d11.dll",   // Validated proxy - could be SpecialK
			L"dinput8.dll"  // Validated proxy - could be SpecialK
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
		// First check for explicit SpecialK modules
		for (const auto& moduleName : { L"SpecialK64.dll", L"SpecialK32.dll" }) {
			HMODULE handle = GetModuleHandle(moduleName);
			if (handle && IsModuleSpecialK(handle)) {
				logger::info(L"SpecialK detected via module: {}", moduleName);
				return true;
			}
		}

		// Check for proxy DLLs that might be SpecialK
		// Note: In modded Skyrim, these are often other tools (ReShade, ENB, etc.)
		// So we use robust validation in IsModuleSpecialK()
		for (const auto& proxyName : { L"dxgi.dll", L"d3d11.dll", L"dinput8.dll" }) {
			HMODULE handle = GetModuleHandle(proxyName);
			if (handle && IsModuleSpecialK(handle)) {
				logger::info(L"SpecialK detected via validated proxy: {}", proxyName);
				return true;
			}
		}

		logger::debug("SpecialK not detected in any loaded modules");
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

		// First check for SpecialK-specific exports (most reliable)
		const char* specialKExports[] = {
			"SKX_GetCommandProcessor",
			"SK_GetDLLRole",
			"SK_GetCommandProcessor",
			"SK_GetFramerate",
			"SK_CreateFuncHook"  // Key indicator of cooperation support
		};

		int exportMatchCount = 0;
		for (const auto& exportName : specialKExports) {
			if (GetProcAddress(module, exportName)) {
				exportMatchCount++;
			}
		}

		// Require multiple SpecialK exports to avoid false positives
		// In modded Skyrim, proxy DLLs are common and may have some overlapping exports
		if (exportMatchCount >= 2) {
			logger::debug("Module identified as SpecialK via {} export matches", exportMatchCount);
			return true;
		}

		// If we only have one export match, be more cautious - check version info
		if (exportMatchCount == 1) {
			std::wstring description = GetModuleDescription(module);
			if (description.find(L"Special K") != std::wstring::npos ||
				description.find(L"SpecialK") != std::wstring::npos) {
				logger::debug("Module identified as SpecialK via export + version info");
				return true;
			} else {
				logger::debug("Single export match but no SpecialK in version info - likely different proxy DLL");
				return false;
			}
		}

		// Check module description/version info as fallback
		// This handles cases where SpecialK might be injected differently
		std::wstring description = GetModuleDescription(module);
		if (description.find(L"Special K") != std::wstring::npos ||
			description.find(L"SpecialK") != std::wstring::npos) {
			logger::debug("Module identified as SpecialK via version info only");
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

	bool CompatibilityChecker::DetectReShade()
	{
		for (const auto& tool : knownConflicts) {
			if (tool.name == L"ReShade") {
				return CheckToolPresence(tool);
			}
		}
		return false;
	}

	bool CompatibilityChecker::DetectENB()
	{
		for (const auto& tool : knownConflicts) {
			if (tool.name == L"ENB Series") {
				return CheckToolPresence(tool);
			}
		}
		return false;
	}
}