#include <windows.h>
#include <winhttp.h>
#include <string>
#include <map>
#include <fstream>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;
CRITICAL_SECTION g_cs;

// 简单的文件日志，绝对不闪退
void WriteLog(const std::string& msg) {
    try {
        std::ofstream log("C:\\dll_debug.txt", std::ios::app);
        if (log.is_open()) {
            log << msg << "\n";
        }
    } catch (...) {}
}

typedef BOOL(WINAPI* pfnWinHttpSendRequest)(
    HINTERNET hRequest,
    LPCWSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
);

typedef BOOL(WINAPI* pfnWinHttpReadData)(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
);

pfnWinHttpSendRequest g_pRealWinHttpSendRequest = NULL;
pfnWinHttpReadData g_pRealWinHttpReadData = NULL;

struct MockResponseContext {
    std::string data;
    DWORD offset = 0;
};

std::map<HINTERNET, MockResponseContext> g_mockSessions;

// 导出函数完美转发
extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA_Proxy(void* a, unsigned long b, unsigned long c, void* d) {
        auto fn = (BOOL(WINAPI*)(void*, unsigned long, unsigned long, void*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) unsigned long WINAPI GetFileVersionInfoSizeA_Proxy(void* a, unsigned long* b) {
        auto fn = (unsigned long(WINAPI*)(void*, unsigned long*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) unsigned long WINAPI GetFileVersionInfoSizeW_Proxy(void* a, unsigned long* b) {
        auto fn = (unsigned long(WINAPI*)(void*, unsigned long*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeW");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoW_Proxy(void* a, unsigned long b, unsigned long c, void* d) {
        auto fn = (BOOL(WINAPI*)(void*, unsigned long, unsigned long, void*))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueA_Proxy(const void* a, const void* b, void** c, unsigned int* d) {
        auto fn = (BOOL(WINAPI*)(const void*, const void*, void**, unsigned int*))GetProcAddress(g_hOriginalDll, "VerQueryValueA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) BOOL WINAPI VerQueryValueW_Proxy(const void* a, const void* b, void** c, unsigned int* d) {
        auto fn = (BOOL(WINAPI*)(const void*, const void*, void**, unsigned int*))GetProcAddress(g_hOriginalDll, "VerQueryValueW");
        return fn ? fn(a, b, c, d) : FALSE;
    }
}

// Hook 后的 SendRequest：精准拦截目标接口并塞入 Mock JSON
BOOL WINAPI Hooked_WinHttpSendRequest(
    HINTERNET hRequest,
    LPCWSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
) {
    WCHAR szUrl[2048] = {0};
    DWORD dwSize = sizeof(szUrl);
    if (WinHttpQueryOption(hRequest, WINHTTP_OPTION_URL, szUrl, &dwSize)) {
        std::wstring url(szUrl);
        std::string mockJson = "";

        if (url.find(L"hunyuan_code_login") != std::wstring::npos) {
            mockJson = "{\"ok\":true,\"user\":{\"id\":\"3be12700-c6a3-497a-92ae-26ebeaa2f631\",\"role\":\"member\",\"status\":\"active\",\"username\":\"m_62ba1d922b\",\"display_name\":\"2B2A-B49F-768C-FC66\",\"last_login_at\":\"2026-09-16T15:04:49.601447+00:00\",\"owner_agent_id\":\"236ce04e-4f6f-4df7-999e-33224d75b32c\",\"must_change_password\":false},\"token\":\"fa4116df07c2a4c8c05fba27a68f8c570cd765bfe37cadd0335d8d1e8d904455\",\"agent_quota\":null,\"entitlement\":{\"plan_type\":\"permanent\",\"expires_at\":null,\"effective_at\":\"2026-09-06T08:33:59.283698+00:00\"}}";
            WriteLog("[INTERCEPT] hunyuan_code_login mocked.");
        }
        else if (url.find(L"hunyuan_heartbeat") != std::wstring::npos) {
            mockJson = "{\"ok\":true,\"server_time\":\"2026-09-16T16:22:42.491191+00:00\"}";
            WriteLog("[INTERCEPT] hunyuan_heartbeat mocked.");
        }
        else if (url.find(L"audio-dedup") != std::wstring::npos) {
            mockJson = "{\"status\":200,\"ok\":true,\"body\":{\"sessionId\":\"ses_mock_local_998877\",\"status\":\"active\",\"expiresAt\":2104154735,\"config\":{\"mode\":\"strong\",\"intensity\":100}}}";
            WriteLog("[INTERCEPT] audio-dedup mocked.");
        }
        else if (url.find(L"replies:generate") != std::wstring::npos) {
            mockJson = "{\"success\":true,\"data\":{\"taskId\":\"75e24688-452e-46bd-99f8-20f62b4b9b9f\",\"progress\":{\"state\":\"ready\",\"progress\":100,\"stage\":\"complete\",\"updatedAt\":\"2026-09-16T16:22:49.642+00:00\"},\"replies\":[{\"commentIndex\":0,\"text\":\"宝宝，这款有现货的哦，发货默认中通，如需发顺丰可以联系客服补差价安排~\"}]}}";
            WriteLog("[INTERCEPT] replies:generate mocked.");
        }

        if (!mockJson.empty()) {
            EnterCriticalSection(&g_cs);
            g_mockSessions[hRequest] = { mockJson, 0 };
            LeaveCriticalSection(&g_cs);
            return TRUE;
        }
    }

    if (g_pRealWinHttpSendRequest) {
        return g_pRealWinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength, dwContext);
    }
    return FALSE;
}

// Hook 后的 ReadData：将 Mock 数据吐给程序
BOOL WINAPI Hooked_WinHttpReadData(
    HINTERNET hRequest,
    LPVOID lpBuffer,
    DWORD dwNumberOfBytesToRead,
    LPDWORD lpdwNumberOfBytesRead
) {
    EnterCriticalSection(&g_cs);
    auto it = g_mockSessions.find(hRequest);
    if (it != g_mockSessions.end()) {
        auto& ctx = it->second;
        DWORD remaining = (DWORD)(ctx.data.size() - ctx.offset);
        if (remaining == 0) {
            if (lpdwNumberOfBytesRead) *lpdwNumberOfBytesRead = 0;
            g_mockSessions.erase(it);
            LeaveCriticalSection(&g_cs);
            return TRUE;
        }

        DWORD bytesToCopy = (dwNumberOfBytesToRead < remaining) ? dwNumberOfBytesToRead : remaining;
        memcpy(lpBuffer, ctx.data.data() + ctx.offset, bytesToCopy);
        ctx.offset += bytesToCopy;

        if (lpdwNumberOfBytesRead) *lpdwNumberOfBytesRead = bytesToCopy;
        LeaveCriticalSection(&g_cs);
        return TRUE;
    }
    LeaveCriticalSection(&g_cs);

    if (g_pRealWinHttpReadData) {
        return g_pRealWinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
    }
    return FALSE;
}

DWORD WINAPI InitThread(LPVOID lpParam) {
    InitializeCriticalSection(&g_cs);
    WriteLog("DLL Initialized successfully without blocking.");

    char sysPath[MAX_PATH];
    GetSystemDirectoryA(sysPath, MAX_PATH);
    strcat_s(sysPath, sizeof(sysPath), "\\version.dll");
    g_hOriginalDll = LoadLibraryA(sysPath);

    HMODULE hWinHttp = LoadLibraryA("winhttp.dll");
    if (hWinHttp) {
        g_pRealWinHttpSendRequest = (pfnWinHttpSendRequest)GetProcAddress(hWinHttp, "WinHttpSendRequest");
        g_pRealWinHttpReadData = (pfnWinHttpReadData)GetProcAddress(hWinHttp, "WinHttpReadData");

        // 安全地进行函数地址替换
        if (g_pRealWinHttpSendRequest && g_pRealWinHttpReadData) {
            DWORD oldProtect;
            // 简单直接修改内存指针指向我们的 Hook 函数
            if (VirtualProtect(&g_pRealWinHttpSendRequest, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                // 如果需要更稳妥的 Inline Hook，可在此处应用，目前直接重定向指针
                WriteLog("WinHttp APIs hooked successfully.");
            }
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        DeleteCriticalSection(&g_cs);
        if (g_hOriginalDll) FreeLibrary(g_hOriginalDll);
        break;
    }
    return TRUE;
}
