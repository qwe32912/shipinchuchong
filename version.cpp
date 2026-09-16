#include <windows.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;

// 原函数指针
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

// 存储每个请求对应的 Mock 返回数据及读取进度
struct MockResponseContext {
    std::string data;
    DWORD offset = 0;
};

// 简单的内存映射表，用来记录被拦截的请求句柄
#include <map>
std::map<HINTERNET, MockResponseContext> g_mockSessions;
CRITICAL_SECTION g_cs;

// 完美匹配的导出转发
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

// 拦截 SendRequest：判断请求路径，直接在本地准备好 Mock JSON
BOOL WINAPI Hooked_WinHttpSendRequest(
    HINTERNET hRequest,
    LPCWSTR lpszHeaders,
    DWORD dwHeadersLength,
    LPVOID lpOptional,
    DWORD dwOptionalLength,
    DWORD dwTotalLength,
    DWORD_PTR dwContext
) {
    // 获取当前请求的 URL 或相关信息，判断是否属于目标后端
    WCHAR szUrl[2048] = {0};
    DWORD dwSize = sizeof(szUrl);
    if (WinHttpQueryOption(hRequest, WINHTTP_OPTION_URL, szUrl, &dwSize)) {
        std::wstring url(szUrl);
        
        // 检查是否是我们需要 Mock 的几个核心接口
        std::string mockJson = "";

        if (url.find(L"hunyuan_code_login") != std::wstring::npos) {
            mockJson = "{\"ok\":true,\"user\":{\"id\":\"3be12700-c6a3-497a-92ae-26ebeaa2f631\",\"role\":\"member\",\"status\":\"active\",\"username\":\"m_62ba1d922b\",\"display_name\":\"2B2A-B49F-768C-FC66\",\"last_login_at\":\"2026-09-16T15:04:49.601447+00:00\",\"owner_agent_id\":\"236ce04e-4f6f-4df7-999e-33224d75b32c\",\"must_change_password\":false},\"token\":\"fa4116df07c2a4c8c05fba27a68f8c570cd765bfe37cadd0335d8d1e8d904455\",\"agent_quota\":null,\"entitlement\":{\"plan_type\":\"permanent\",\"expires_at\":null,\"effective_at\":\"2026-09-06T08:33:59.283698+00:00\"}}";
        }
        else if (url.find(L"hunyuan_heartbeat") != std::wstring::npos) {
            mockJson = "{\"ok\":true,\"server_time\":\"2026-09-16T16:22:42.491191+00:00\"}";
        }
        else if (url.find(L"audio-dedup") != std::wstring::npos) {
            mockJson = "{\"status\":200,\"ok\":true,\"body\":{\"sessionId\":\"ses_mock_local_998877\",\"status\":\"active\",\"expiresAt\":2104154735,\"config\":{\"mode\":\"strong\",\"intensity\":100}}}";
        }
        else if (url.find(L"replies:generate") != std::wstring::npos) {
            mockJson = "{\"success\":true,\"data\":{\"taskId\":\"75e24688-452e-46bd-99f8-20f62b4b9b9f\",\"progress\":{\"state\":\"ready\",\"progress\":100,\"stage\":\"complete\",\"updatedAt\":\"2026-09-16T16:22:49.642+00:00\"},\"replies\":[{\"commentIndex\":0,\"text\":\"宝宝，这款有现货的哦，发货默认中通，如需发顺丰可以联系客服补差价安排~\"}]}}";
        }

        if (!mockJson.empty()) {
            EnterCriticalSection(&g_cs);
            g_mockSessions[hRequest] = { mockJson, 0 };
            LeaveCriticalSection(&g_cs);

            // 伪造响应头，直接让 WinHTTP 认为请求成功返回了 200 OK
            // 注意：这里直接向后端发送原始请求或者直接返回 TRUE 伪造成功
            // 为了走通流程，我们直接返回 TRUE，并让后面的 ReadData 吐出我们的 Mock 数据
            return TRUE;
        }
    }

    // 其他请求走正常网络逻辑
    return g_pRealWinHttpSendRequest(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength, dwContext);
}

// 拦截 ReadData：如果当前句柄在我们的 Mock 列表里，直接把准备好的 JSON 吐给程序
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

    return g_pRealWinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

// 简单的 IAT Hook 替换函数
void HookWinHttp() {
    HMODULE hWinHttp = LoadLibraryA("winhttp.dll");
    if (!hWinHttp) return;

    g_pRealWinHttpSendRequest = (pfnWinHttpSendRequest)GetProcAddress(hWinHttp, "WinHttpSendRequest");
    g_pRealWinHttpReadData = (pfnWinHttpReadData)GetProcAddress(hWinHttp, "WinHttpReadData");

    if (g_pRealWinHttpSendRequest && g_pRealWinHttpReadData) {
        // 修改内存保护并替换函数指针
        DWORD oldProtect;
        VirtualProtect(&g_pRealWinHttpSendRequest, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
        
        // 实际项目中推荐使用成熟的 MinHook 库来进行 Hook，这里演示直接替换核心 API 导入表或通过 Detour
        // 为确保稳定，你可以把这部分换成 MinHook 的 MH_CreateHook 写法
    }
}

DWORD WINAPI InitThread(LPVOID lpParam) {
    InitializeCriticalSection(&g_cs);
    
    char sysPath[MAX_PATH];
    GetSystemDirectoryA(sysPath, MAX_PATH);
    strcat_s(sysPath, sizeof(sysPath), "\\version.dll");
    g_hOriginalDll = LoadLibraryA(sysPath);

    // 可以在这里初始化你的拦截逻辑
    // HookWinHttp();

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
