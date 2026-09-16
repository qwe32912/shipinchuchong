#include <windows.h>
#include <stdio.h>

HMODULE g_hOriginalDll = NULL;
HMODULE g_hMyModule = NULL;

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

void WriteInjectLog(const char* fmt, ...) {
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
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

DWORD WINAPI PatchThread(LPVOID lpParam) {
    WriteInjectLog("[*] PatchThread started successfully in process ID: %d", GetCurrentProcessId());

    // 尝试遍历当前进程加载的所有模块，寻找任何可能的 Python 运行时 (支持 python3.dll, python310.dll, python313.dll 等)
    HMODULE hPython = NULL;
    int retry = 0;
    char modName[MAX_PATH];

    while (retry < 50) {
        // 常见可能的 Python 运行时名称
        const char* pyDlls[] = { "python313.dll", "python312.dll", "python311.dll", "python310.dll", "python3.dll" };
        for (int i = 0; i < 5; i++) {
            hPython = GetModuleHandleA(pyDlls[i]);
            if (hPython) {
                WriteInjectLog("[+] Found Python runtime module: %s at %p", pyDlls[i], hPython);
                break;
            }
        }
        if (hPython) break;
        Sleep(200);
        retry++;
    }

    if (!hPython) {
        WriteInjectLog("[-] Warning: No standard Python runtime DLL found. This application may be purely Rust-compiled.");
        return 0;
    }

    typedef void* (*t_PyGILState_Ensure)();
    typedef void (*t_PyGILState_Release)(void*);
    typedef int (*t_PyRun_SimpleString)(const char*);

    t_PyGILState_Ensure p_PyGILState_Ensure = (t_PyGILState_Ensure)GetProcAddress(hPython, "PyGILState_Ensure");
    t_PyGILState_Release p_PyGILState_Release = (t_PyGILState_Release)GetProcAddress(hPython, "PyGILState_Release");
    t_PyRun_SimpleString p_PyRun_SimpleString = (t_PyRun_SimpleString)GetProcAddress(hPython, "PyGILState_SimpleString");

    if (p_PyGILState_Ensure && p_PyRun_SimpleString) {
        void* gstate = p_PyGILState_Ensure();
        WriteInjectLog("[+] Python GIL acquired successfully.");

        // 执行内联 Python 脚本，并把所有异常完整记录到日志
        const char* pyCode = 
            "import os, sys, traceback\n"
            "try:\n"
            "    current_dir = os.path.dirname(os.path.abspath(sys.argv[0])) if sys.argv and sys.argv[0] else os.getcwd()\n"
            "    log_path = os.path.join(current_dir, 'net_debug.txt')\n"
            "    with open(log_path, 'a', encoding='utf-8') as f:\n"
            "        f.write('[PY-INIT] Python hook environment active.\\n')\n"
            "    \n"
            "    # 遍历已加载模块寻找 Feiniao / 鉴权相关的类并强制劫持\n"
            "    for mod_name, mod_obj in list(sys.modules.items()):\n"
            "        for attr_name in dir(mod_obj):\n"
            "            if any(k in attr_name.lower() for k in ['client', 'auth', 'license', 'feiniao']):\n"
            "                obj = getattr(mod_obj, attr_name, None)\n"
            "                if obj and hasattr(obj, 'login') and not getattr(obj, '_hooked_by_dll', False):\n"
            "                    orig = obj.login\n"
            "                    def make_fake(o=obj, orig_fn=orig):\n"
            "                        def fake(self, *args, **kwargs):\n"
            "                            with open(log_path, 'a', encoding='utf-8') as f:\n"
            "                                f.write(f'[HOOK] login intercepted on {o}, args={args}\\n')\n"
            "                            return {\n"
            "                                'ok': True,\n"
            "                                'token': '208814648088aa3e5a046cbc09ef4759c1840d776b635d076b17c1da7c3d2de8',\n"
            "                                'user': {'id': 'bypassed', 'role': 'member', 'status': 'active'},\n"
            "                                'entitlement': {'plan_type': 'permanent', 'expires_at': None}\n"
            "                            }\n"
            "                        return fake\n"
            "                    obj.login = make_fake()\n"
            "                    obj._hooked_by_dll = True\n"
            "                    with open(log_path, 'a', encoding='utf-8') as f:\n"
            "                        f.write(f'[HOOKED] Success on {mod_name}.{attr_name}\\n')\n"
            "except Exception as e:\n"
            "    with open(log_path, 'a', encoding='utf-8') as f:\n"
            "        f.write(f'[PY-ERROR] {traceback.format_exc()}\\n')\n";

        p_PyRun_SimpleString(pyCode);
        WriteInjectLog("[+] Python patch script executed via C++ wrapper.");

        if (p_PyGILState_Release) {
            p_PyGILState_Release(gstate);
        }
    } else {
        WriteInjectLog("[-] Failed to resolve Python C-API entry points.");
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        g_hMyModule = hModule;
        DisableThreadLibraryCalls(hModule);

        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        WriteInjectLog("[*] version.dll hijacked, DLL_PROCESS_ATTACH received.");
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PatchThread, NULL, 0, NULL);
        break;
    }
    case DLL_PROCESS_DETACH:
        WriteInjectLog("[*] DLL_PROCESS_DETACH.");
        if (g_hOriginalDll) {
            FreeLibrary(g_hOriginalDll);
        }
        break;
    }
    return TRUE;
}
