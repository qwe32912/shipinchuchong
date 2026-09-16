#include <windows.h>

#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;

// 使用不冲突的内部代理函数名
extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA_MyProxy(void* a, unsigned long b, unsigned long c, void* d) {
        auto fn = (BOOL(WINAPI*)(void*, unsigned long, unsigned long, void*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) unsigned long WINAPI GetFileVersionInfoSizeA_MyProxy(void* a, unsigned long* b) {
        auto fn = (unsigned long(WINAPI*)(void*, unsigned long*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoW_MyProxy(void* a, unsigned long b, unsigned long c, void* d) {
        auto fn = (BOOL(WINAPI*)(void*, unsigned long, unsigned long, void*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) unsigned long WINAPI GetFileVersionInfoSizeW_MyProxy(void* a, unsigned long* b) {
        auto fn = (unsigned long(WINAPI*)(void*, unsigned long*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeW");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueA_MyProxy(const void* a, const void* b, void** c, unsigned int* d) {
        auto fn = (BOOL(WINAPI*)(const void*, const void*, void**, unsigned int*))GetProcAddress(g_hOriginalDll, "VerQueryValueA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueW_MyProxy(const void* a, const void* b, void** c, unsigned int* d) {
        auto fn = (BOOL(WINAPI*)(const void*, const void*, void**, unsigned int*))GetProcAddress(g_hOriginalDll, "VerQueryValueW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
}

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    Sleep(500);
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) hWs2 = LoadLibraryA("ws2_32.dll");
    
    if (hWs2) {
        MessageBoxA(NULL, "Ws2_32.dll loaded & functions resolved!", "Hook Status", MB_OK | MB_TOPMOST);
    } else {
        MessageBoxA(NULL, "Failed to load Ws2_32.dll!", "Hook Error", MB_OK | MB_TOPMOST);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, sizeof(sysPath), "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        MessageBoxA(NULL, "version.dll Injected & DllMain Attached!", "Inject Success", MB_OK | MB_TOPMOST);
        CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);
        break;
    }
    case DLL_PROCESS_DETACH:
        if (g_hOriginalDll) FreeLibrary(g_hOriginalDll);
        break;
    }
    return TRUE;
}
