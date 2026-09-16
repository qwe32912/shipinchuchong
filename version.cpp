#include <windows.h>

#pragma comment(lib, "user32.lib")

HMODULE g_hOriginalDll = NULL;

// 声明原始函数指针
typedef int (WINAPI* PFN_connect)(SOCKET s, const struct sockaddr* name, int namelen);
typedef int (WINAPI* PFN_send)(SOCKET s, const char* buf, int len, int flags);

PFN_connect Real_connect = NULL;
PFN_send Real_send = NULL;

// 代理导出
extern "C" {
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoA(LPTSTR a, DWORD b, DWORD c, LPVOID d) {
        auto fn = (BOOL(WINAPI*)(LPTSTR, DWORD, DWORD, LPVOID))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoA");
        return fn ? fn(a, b, c, d) : FALSE;
    }
    __declspec(dllexport) DWORD WINAPI GetFileVersionInfoSizeA(LPTSTR a, LPDWORD b) {
        auto fn = (DWORD(WINAPI*)(LPTSTR, LPDWORD))GetProcAddress(g_hOriginalDll, "GetFileVersionInfoSizeA");
        return fn ? fn(a, b) : 0;
    }
    __declspec(dllexport) BOOL WINAPI GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
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

// 你的自定义 Hook 逻辑
int WINAPI Hooked_connect(SOCKET s, const struct sockaddr* name, int namelen) {
    // 可以在这里弹窗或者处理网络连接
    return Real_connect(s, name, namelen);
}

int WINAPI Hooked_send(SOCKET s, const char* buf, int len, int flags) {
    // 可以在这里篡改、替换或者查看发出的封包内容
    return Real_send(s, buf, len, flags);
}

void InitHook() {
    HMODULE hWs2 = GetModuleHandleA("ws2_32.dll");
    if (!hWs2) hWs2 = LoadLibraryA("ws2_32.dll");
    
    if (hWs2) {
        Real_connect = (PFN_connect)GetProcAddress(hWs2, "connect");
        Real_send = (PFN_send)GetProcAddress(hWs2, "send");
        
        // 弹窗提示：网络模块句柄获取成功，你可以开始写后续挂钩逻辑了
        MessageBoxA(NULL, "Ws2_32.dll loaded & functions resolved!", "Hook Status", MB_OK | MB_TOPMOST);
    } else {
        MessageBoxA(NULL, "Failed to load Ws2_32.dll!", "Hook Error", MB_OK | MB_TOPMOST);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);

        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, sizeof(sysPath), "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        // 1. 第一步弹窗：确认 DLL 已经被成功加载
        MessageBoxA(NULL, "version.dll Injected & DllMain Attached!", "Inject Success", MB_OK | MB_TOPMOST);

        // 异步初始化 Hook，防止阻塞主线程启动
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)([] (LPVOID) {
            Sleep(500);
            InitHook();
            return 0;
        }), NULL, 0, NULL);

        break;
    }
    case DLL_PROCESS_DETACH:
        if (g_hOriginalDll) FreeLibrary(g_hOriginalDll);
        break;
    }
    return TRUE;
}
