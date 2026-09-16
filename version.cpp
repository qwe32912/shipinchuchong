#include <windows.h>
#include <stdio.h>

HMODULE g_hOriginalDll = NULL;
CRITICAL_SECTION g_LogLock;
char g_LogPath[MAX_PATH] = { 0 };

typedef BOOL(WINAPI* pfnGetFileVersionInfoA)(LPTSTR, DWORD, DWORD, LPVOID);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeA)(LPTSTR, LPDWORD);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* pfnGetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL(WINAPI* pfnVerQueryValueA)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
typedef BOOL(WINAPI* pfnVerQueryValueW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA_Proxy(LPTSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (pfnGetFileVersionInfoA)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeA_Proxy(LPTSTR a, LPDWORD b) {
        auto fn = (pfnGetFileVersionInfoSizeA)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeW_Proxy(LPCWSTR a, LPDWORD b) {
        auto fn = (pfnGetFileVersionInfoSizeW)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeW");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoW_Proxy(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (pfnGetFileVersionInfoW)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueA_Proxy(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (pfnVerQueryValueA)GetProcAddress(g_hOriginalDll, "VerQueryValueA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueW_Proxy(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (pfnVerQueryValueW)GetProcAddress(g_hOriginalDll, "VerQueryValueW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
}

// 仿照你给的 Python log_io：加锁、追加写入、立即 flush
void log_io(const char* msg) {
    EnterCriticalSection(&g_LogLock);
    __try {
        FILE* f = fopen(g_LogPath, "a");
        if (f) {
            fprintf(f, "%s\n", msg);
            fflush(f);
            fclose(f);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 防止意外崩溃
    }
    LeaveCriticalSection(&g_LogLock);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        InitializeCriticalSection(&g_LogLock);

        // 获取当前 DLL 所在的目录，并在同目录下生成 net_debug.txt
        GetModuleFileNameA(hModule, g_LogPath, MAX_PATH);
        char* lastSlash = strrchr(g_LogPath, '\\');
        if (lastSlash) {
            *(lastSlash + 1) = '\0';
        }
        strcat_s(g_LogPath, sizeof(g_LogPath), "net_debug.txt");

        // 加载系统真正的 version.dll
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, sizeof(sysPath), "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        log_io("[INIT] Native C++ version.dll loaded successfully.");
        break;
    }
    case DLL_PROCESS_DETACH:
        log_io("[DETACH] Unloading version.dll.");
        DeleteCriticalSection(&g_LogLock);
        if (g_hOriginalDll) {
            FreeLibrary(g_hOriginalDll);
        }
        break;
    }
    return TRUE;
}
