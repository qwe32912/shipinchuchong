#include <windows.h>

#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;

// 使用原本的函数签名，避免包含 winver.h 冲突
extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA(LPTSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (BOOL(WINAPI*)(LPTSTR, DWORD, DWORD, LPVOID))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeA(LPTSTR a, LPDWORD b) {
        auto fn = (DWORD(WINAPI*)(LPTSTR, LPDWORD))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) {
        auto fn = (DWORD(WINAPI*)(LPCWSTR, LPDWORD))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeW");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueA(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT))GetProcAddress(g_hOriginalDll, "VerQueryValueA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueW(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT))GetProcAddress(g_hOriginalDll, "VerQueryValueW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
}

// 声明原始函数指针
typedef int (WINAPI* PFN_connect)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int (WINAPI* PFN_send)(SOCKET s, const char* buf, int len, int flags);

PFN_connect Real_connect = NULL;
PFN_send Real_send = NULL;

void InitHook() {
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) hWs2 = LoadLibraryA("ws2_32.dll");
    
    if (hWs2) {
        Real_connect = (PFN_connect)GetProcAddress(hWs2, "connect");
        Real_send = (PFN_send)GetProcAddress(hWs2, "send");
        MessageBoxA(NULL, "Ws2_32.dll loaded & functions resolved!", "Hook Status", MB_OK | MB_TOPMOST);
    } else {
        MessageBoxA(NULL, "Failed to load Ws2_32.dll!", "Hook Error", MB_OK | MB_TOPMOST);
    }
}

DWORD WINAPI ThreadProc(LPVOID lpParam) {
    Sleep(500);
    InitHook();
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
