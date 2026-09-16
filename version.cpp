#include <windows.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;

// 标准导出函数转发代理
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

// 记录调试日志到 C 盘根目录
void WriteLog(const char* text) {
    HANDLE hFile = CreateFileA("C:\\net_debug.txt", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, text, (DWORD)lstrlenA(text), &written, NULL);
        WriteFile(hFile, "\r\n", 2, &written, NULL);
        CloseHandle(hFile);
    }
}

// Hook WinHttpSendRequest 来捕获并篡改后端请求
typedef BOOL(WINAPI* pfnWinHttpSendRequest)(
    HINTERNET hRequest,
    LPCWSTR   lpszHeaders,
    DWORD     dwHeadersLength,
    LPVOID    lpOptional,
    DWORD     dwOptionalLength,
    DWORD     dwTotalLength,
    DWORD_PTR dwContext
);

pfnWinHttpSendRequest Real_WinHttpSendRequest = NULL;

BOOL WINAPI Hooked_WinHttpSendRequest(
    HINTERNET hRequest,
    LPCWSTR   lpszHeaders,
    DWORD     dwHeadersLength,
    LPVOID    lpOptional,
    DWORD     dwOptionalLength,
    DWORD     dwTotalLength,
    DWORD_PTR dwContext
) {
    if (lpOptional && dwOptionalLength > 0) {
        std::string body((char*)lpOptional, dwOptionalLength);
        WriteLog("[WinHttp Outbound Body]:");
        WriteLog(body.c_str());
    }
    return Real_WinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength, dwContext);
}

void InitWinHttpHook() {
    HMODULE hWinHttp = GetModuleHandleA("winhttp.dll");
    if (!hWinHttp) {
        hWinHttp = LoadLibraryA("winhttp.dll");
    }
    if (hWinHttp) {
        LPVOID pTarget = (LPVOID)GetProcAddress(hWinHttp, "WinHttpSendRequest");
        if (pTarget) {
            // 简单的内存修改实现 Hook（或者你可以直接记录日志观察请求）
            Real_WinHttpSendRequest = (pfnWinHttpSendRequest)pTarget;
            WriteLog("[+] WinHttpSendRequest found.");
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        WriteLog("[+] version.dll (Native C++ Hook) Loaded.");
        InitWinHttpHook();
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
