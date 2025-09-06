#pragma once

#include "PCH.h"
#include <vector>

namespace Compatibility {
    
    struct ConflictingTool {
        std::wstring name;
        std::vector<std::wstring> moduleNames;
        std::vector<std::string> exportNames;
        bool detected = false;
        bool cooperative = false;  // Can we cooperate with this tool?
    };

    // SpecialK cooperation API function pointers
    typedef DWORD   (WINAPI *SK_GetDLLRole_pfn)();
    typedef HMODULE (WINAPI *SK_GetDLL_pfn)();
    typedef BOOL    (WINAPI *SK_CreateFuncHook_pfn)(LPCWSTR pwszFuncName, LPVOID pTarget, LPVOID pDetour, LPVOID* ppOriginal);
    typedef BOOL    (WINAPI *SK_EnableHook_pfn)(LPVOID pTarget);
    typedef BOOL    (WINAPI *SK_DisableHook_pfn)(LPVOID pTarget);
    typedef BOOL    (WINAPI *SK_RemoveHook_pfn)(LPVOID pTarget);
    typedef VOID    (WINAPI *SK_ApplyQueuedHooks_pfn)();

    struct SpecialKAPI {
        SK_GetDLLRole_pfn GetDLLRole = nullptr;
        SK_GetDLL_pfn GetDLL = nullptr;
        SK_CreateFuncHook_pfn CreateFuncHook = nullptr;
        SK_EnableHook_pfn EnableHook = nullptr;
        SK_DisableHook_pfn DisableHook = nullptr;
        SK_RemoveHook_pfn RemoveHook = nullptr;
        SK_ApplyQueuedHooks_pfn ApplyQueuedHooks = nullptr;
        bool available = false;
    };

    class CompatibilityChecker {
    public:
        static CompatibilityChecker* GetSingleton() {
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
        bool IsModuleSpecialK(HMODULE module);
        std::wstring GetModuleDescription(HMODULE module);
    };
}