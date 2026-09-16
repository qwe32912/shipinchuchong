#include <windows.h>
#include <stdio.h>

HMODULE g_hOriginalDll = NULL;

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

void WriteInjectLog(const char* msg) {
    char path[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, path);
    strcat_s(path, "\\inject_debug.txt");
    FILE* f = fopen(path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

DWORD WINAPI PatchThread(LPVOID lpParam) {
    WriteInjectLog("[*] PatchThread started, waiting for python313.dll...");
    HMODULE hPython = NULL;
    int retry = 0;
    while (!hPython && retry < 100) {
        hPython = GetModuleHandleA("python313.dll");
        if (!hPython) {
            Sleep(200);
            retry++;
        }
    }

    if (!hPython) {
        WriteInjectLog("[-] Failed to find python313.dll timeout.");
        return 0;
    }
    WriteInjectLog("[+] Found python313.dll successfully.");

    typedef void* (*t_PyGILState_Ensure)();
    typedef void (*t_PyGILState_Release)(void*);
    typedef int (*t_PyRun_SimpleString)(const char*);

    t_PyGILState_Ensure p_PyGILState_Ensure = (t_PyGILState_Ensure)GetProcAddress(hPython, "PyGILState_Ensure");
    t_PyGILState_Release p_PyGILState_Release = (t_PyGILState_Release)GetProcAddress(hPython, "PyGILState_Release");
    t_PyRun_SimpleString p_PyRun_SimpleString = (t_PyRun_SimpleString)GetProcAddress(hPython, "PyGILState_SimpleString");

    if (p_PyGILState_Ensure && p_PyRun_SimpleString) {
        void* gstate = p_PyGILState_Ensure();
        WriteInjectLog("[+] Python GIL acquired, running initialization script...");

        const char* pyCode = 
            "import os, sys, time, threading, json, traceback\n"
            "current_dir = os.path.dirname(os.path.abspath(sys.argv[0])) if sys.argv and sys.argv[0] else os.getcwd()\n"
            "log_path = os.path.join(current_dir, 'net_debug.txt')\n"
            "log_lock = threading.Lock()\n"
            "\n"
            "def log_io(msg):\n"
            "    try:\n"
            "        with log_lock:\n"
            "            with open(log_path, 'a', encoding='utf-8') as f:\n"
            "                f.write(msg + '\\n')\n"
            "                f.flush()\n"
            "    except Exception:\n"
            "        pass\n"
            "\n"
            "log_io('[INIT] Python patch script injected successfully.')\n"
            "\n"
            "def hook_all():\n"
            "    try:\n"
            "        for mod_name, mod_obj in list(sys.modules.items()):\n"
            "            # 拦截包含 client 或 auth 或 login 的模块\n"
            "            for attr_name in dir(mod_obj):\n"
            "                if 'client' in attr_name.lower() or 'auth' in attr_name.lower() or 'feiniao' in attr_name.lower():\n"
            "                    obj = getattr(mod_obj, attr_name, None)\n"
            "                    if obj and hasattr(obj, 'login') and not getattr(obj, '_hooked_login', False):\n"
            "                        orig_login = obj.login\n"
            "                        def make_fake_login(o=obj, orig=orig_login):\n"
            "                            def fake_login(self, *args, **kwargs):\n"
            "                                log_io(f'[HOOK LOGIN] called on {o} with args: {args}, kwargs: {kwargs}')\n"
            "                                token = 'OZIVPHH1IQSXNIC1BKUWDNMWQZHKJU3L'\n"
            "                                now_ts = int(time.time())\n"
            "                                res = {\n"
            "                                    \"Data\": {\n"
            "                                        \"AgentUid\": 0,\n"
            "                                        \"Key\": args[1] if len(args) > 1 else 'ed0ab947e1871f719c5601cd538641c0',\n"
            "                                        \"LoginIp\": \"119.248.153.156\",\n"
            "                                        \"LoginTime\": now_ts,\n"
            "                                        \"NewAppUser\": False,\n"
            "                                        \"OutUser\": 1,\n"
            "                                        \"RegisterTime\": 1788794735,\n"
            "                                        \"User\": args[0] if len(args) > 0 else 'BypassedUser',\n"
            "                                        \"UserClassMark\": 0,\n"
            "                                        \"UserClassName\": \"VIP\",\n"
            "                                        \"VipNumber\": 1,\n"
            "                                        \"VipTime\": 2104154735\n"
            "                                    },\n"
            "                                    \"Msg\": \"ok\",\n"
            "                                    \"Status\": 1379041306,\n"
            "                                    \"Time\": now_ts\n"
            "                                }\n"
            "                                return res\n"
            "                            return fake_login\n"
            "                        obj.login = make_fake_login()\n"
            "                        obj._hooked_login = True\n"
            "                        log_io(f'[HOOKED] Successfully hooked login on {mod_name}.{attr_name}')\n"
            "    except Exception as e:\n"
            "        log_io(f'[ERROR] hook_all exception: {traceback.format_exc()}')\n"
            "\n"
            "def watcher():\n"
            "    while True:\n"
            "        hook_all()\n"
            "        time.sleep(1.0)\n"
            "\n"
            "threading.Thread(target=watcher, daemon=True).start()\n";

        p_PyRun_SimpleString(pyCode);
        WriteInjectLog("[+] Python script executed successfully.");

        if (p_PyGILState_Release) {
            p_PyGILState_Release(gstate);
        }
    } else {
        WriteInjectLog("[-] Failed to get Python C-API functions.");
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\version.dll");
        g_hOriginalDll = LoadLibraryA(sysPath);

        WriteInjectLog("[*] DLL_PROCESS_ATTACH triggered.");
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PatchThread, NULL, 0, NULL);
        break;
    }
    case DLL_PROCESS_DETACH:
        WriteInjectLog("[*] DLL_PROCESS_DETACH triggered.");
        if (g_hOriginalDll) {
            FreeLibrary(g_hOriginalDll);
        }
        break;
    }
    return TRUE;
}
