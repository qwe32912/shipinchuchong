#include <windows.h>
#include <stdio.h>

HMODULE g_hOriginalDll = NULL;
HMODULE g_hMyModule = NULL;

// 定义原函数的函数指针类型
typedef BOOL(WINAPI* pfnGetFileVersionInfoA)(LPTSTR, DWORD, DWORD, LPVOID);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeA)(LPTSTR, LPDWORD);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* pfnGetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL(WINAPI* pfnVerQueryValueA)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
typedef BOOL(WINAPI* pfnVerQueryValueW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

// 64位下 extern "C" 不会进行符号修饰，导出的函数名会与原版系统完全一致
extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA(LPTSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (pfnGetFileVersionInfoA)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }

    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeA(LPTSTR a, LPDWORD b) {
        auto fn = (pfnGetFileVersionInfoSizeA)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }

    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) {
        auto fn = (pfnGetFileVersionInfoSizeW)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeW");
        return fn ? fn(a, b) : 0;
    }

    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (pfnGetFileVersionInfoW)GetProcAddress(g_hOriginalDll, "GetFileVersionInfoW");
        return fn ? fn(a, b, c, d) : FALSE;
    }

    __declspec(dllexport) BOOL WINAPI VerQueryValueA(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (pfnVerQueryValueA)GetProcAddress(g_hOriginalDll, "VerQueryValueA");
        return fn ? fn(a, b, c, d) : FALSE;
    }

    __declspec(dllexport) BOOL WINAPI VerQueryValueW(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        auto fn = (pfnVerQueryValueW)GetProcAddress(g_hOriginalDll, "VerQueryValueW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
}

void WriteLog(const char* msg) {
    char path[MAX_PATH];
    if (g_hMyModule) {
        GetModuleFileNameA(g_hMyModule, path, MAX_PATH);
        char* lastSlash = strrchr(path, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
        strcat_s(path, "inject_debug.txt");
    } else {
        strcpy_s(path, "C:\\inject_debug.txt");
    }

    FILE* f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        g_hMyModule = hModule;
        DisableThreadLibraryCalls(hModule);

        // 加载系统真正的 version.dll
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        WriteLog("[+] SUCCESS: version.dll proxy loaded and functions forwarded!");
        break;
    }
    case DLL_PROCESS_DETACH:
        if (g_hOriginalDll) {
            FreeLibrary(g_hOriginalDll);
        }
        break;
    }
    return TRUE;
}
