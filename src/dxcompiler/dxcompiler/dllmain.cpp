#include "pch.h"

extern HMODULE g_hCurrentModule;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hCurrentModule = hModule;
        DisableThreadLibraryCalls(hModule);
        MH_Initialize();
        break;

    case DLL_PROCESS_DETACH:
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
