#include <windows.h>

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

DWORD WINAPI PatchThread(LPVOID lpParam) {
    HMODULE hPython = NULL;
    while (!hPython) {
        hPython = GetModuleHandleA("python313.dll");
        Sleep(100);
    }

    typedef void* (*t_PyGILState_Ensure)();
    typedef void (*t_PyGILState_Release)(void*);
    typedef int (*t_PyRun_SimpleString)(const char*);

    t_PyGILState_Ensure p_PyGILState_Ensure = (t_PyGILState_Ensure)GetProcAddress(hPython, "PyGILState_Ensure");
    t_PyGILState_Release p_PyGILState_Release = (t_PyGILState_Release)GetProcAddress(hPython, "PyGILState_Release");
    t_PyRun_SimpleString p_PyRun_SimpleString = (t_PyRun_SimpleString)GetProcAddress(hPython, "PyRun_SimpleString");

    if (p_PyGILState_Ensure && p_PyRun_SimpleString) {
        void* gstate = p_PyGILState_Ensure();

        const char* pyCode = 
            "import os, sys, time, threading, json, traceback, http.client\n"
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
            "# 1. 记录指定域名的底层网络请求\n"
            "def hook_http():\n"
            "    try:\n"
            "        if getattr(http.client.HTTPConnection, '_net_hooked', False):\n"
            "            return\n"
            "        orig_request = http.client.HTTPConnection.request\n"
            "        orig_getresponse = http.client.HTTPConnection.getresponse\n"
            "        \n"
            "        def new_request(self, method, url, body=None, headers={}, *args, **kwargs):\n"
            "            host = getattr(self, 'host', 'unknown')\n"
            "            port = getattr(self, 'port', 80)\n"
            "            proto = 'https' if isinstance(self, http.client.HTTPSConnection) else 'http'\n"
            "            full_url = f\"{proto}://{host}:{port}{url}\" if not url.startswith('http') else url\n"
            "            self._last_url = full_url\n"
            "            \n"
            "            if 'tiantianzhushou.com' in host.lower():\n"
            "                body_str = ''\n"
            "                if body:\n"
            "                    if isinstance(body, bytes):\n"
            "                        try: body_str = body.decode('utf-8', errors='ignore')\n"
            "                        except: body_str = str(body)\n"
            "                    else:\n"
            "                        body_str = str(body)\n"
            "                log_io(f'[--> HTTP OUT] {method} {full_url}\\n  Headers: {headers}\\n  Body: {body_str}\\n' + '-'*50)\n"
            "                self._should_log = True\n"
            "            else:\n"
            "                self._should_log = False\n"
            "            return orig_request(self, method, url, body=body, headers=headers, *args, **kwargs)\n"
            "\n"
            "        def new_getresponse(self, *args, **kwargs):\n"
            "            response = orig_getresponse(self, *args, **kwargs)\n"
            "            if getattr(self, '_should_log', False):\n"
            "                url = getattr(self, '_last_url', 'unknown')\n"
            "                status = getattr(response, 'status', 'unknown')\n"
            "                reason = getattr(response, 'reason', '')\n"
            "                orig_read = response.read\n"
            "                def new_read(amt=None):\n"
            "                    data = orig_read(amt)\n"
            "                    try:\n"
            "                        resp_str = data.decode('utf-8', errors='ignore') if isinstance(data, bytes) else str(data)\n"
            "                        log_io(f'[<-- HTTP IN] {status} {reason} for {url}\\n  Response Body: {resp_str}\\n' + '='*50)\n"
            "                    except Exception:\n"
            "                        pass\n"
            "                    return data\n"
            "                response.read = new_read\n"
            "            return response\n"
            "\n"
            "        http.client.HTTPConnection.request = new_request\n"
            "        http.client.HTTPSConnection.request = new_request\n"
            "        http.client.HTTPConnection.getresponse = new_getresponse\n"
            "        http.client.HTTPSConnection.getresponse = new_getresponse\n"
            "        http.client.HTTPConnection._net_hooked = True\n"
            "    except Exception:\n"
            "        pass\n"
            "\n"
            "# 2. 完整恢复 FeiniaoClient 全套劫持逻辑\n"
            "def hook_client():\n"
            "    try:\n"
            "        for mod_name, mod_obj in list(sys.modules.items()):\n"
            "            if hasattr(mod_obj, 'FeiniaoClient'):\n"
            "                fc = mod_obj.FeiniaoClient\n"
            "                if not getattr(fc, '_full_hooked', False):\n"
            "                    \n"
            "                    if hasattr(fc, 'get_token'):\n"
            "                        def fake_get_token(self, *args, **kwargs):\n"
            "                            token = 'OZIVPHH1IQSXNIC1BKUWDNMWQZHKJU3L'\n"
            "                            self.token = token\n"
            "                            self._token = token\n"
            "                            return token\n"
            "                        fc.get_token = fake_get_token\n"
            "\n"
            "                    if hasattr(fc, 'login'):\n"
            "                        def fake_login(self, *args, **kwargs):\n"
            "                            token = 'OZIVPHH1IQSXNIC1BKUWDNMWQZHKJU3L'\n"
            "                            self.token = token\n"
            "                            self._token = token\n"
            "                            now_ts = int(time.time())\n"
            "                            res = {\n"
            "                                \"Data\": {\n"
            "                                    \"AgentUid\": 0,\n"
            "                                    \"Key\": args[1] if len(args) > 1 else 'ed0ab947e1871f719c5601cd538641c0',\n"
            "                                    \"LoginIp\": \"119.248.153.156\",\n"
            "                                    \"LoginTime\": now_ts,\n"
            "                                    \"NewAppUser\": False,\n"
            "                                    \"OutUser\": 1,\n"
            "                                    \"RegisterTime\": 1788794735,\n"
            "                                    \"User\": args[0] if len(args) > 0 else 'BypassedUser',\n"
            "                                    \"UserClassMark\": 0,\n"
            "                                    \"UserClassName\": \"VIP\",\n"
            "                                    \"VipNumber\": 1,\n"
            "                                    \"VipTime\": 2104154735\n"
            "                                },\n"
            "                                \"Msg\": \"ok\",\n"
            "                                \"Status\": 1379041306,\n"
            "                                \"Time\": now_ts\n"
            "                            }\n"
            "                            log_io(f'[HOOK] login -> {res}')\n"
            "                            return res\n"
            "                        fc.login = fake_login\n"
            "\n"
            "                    if hasattr(fc, 'heartbeat'):\n"
            "                        def fake_heartbeat(self, *args, **kwargs):\n"
            "                            return {\"Data\": {\"Status\": 1}, \"Msg\": \"ok\", \"Status\": 1755554210, \"Time\": int(time.time())}\n"
            "                        fc.heartbeat = fake_heartbeat\n"
            "\n"
            "                    if hasattr(fc, 'request'):\n"
            "                        orig_request = fc.request\n"
            "                        def fake_request(self, *args, **kwargs):\n"
            "                            try:\n"
            "                                args_str = ''\n"
            "                                try:\n"
            "                                    args_str = str(args)\n"
            "                                except Exception:\n"
            "                                    pass\n"
            "                                \n"
            "                                if '3bd13442288592fad17461c5a14f45d8' in args_str or 'dedupe.recipe' in args_str or 'jianying.storyboard' in args_str:\n"
            "                                    job_id = 'recipe-edc44df398cfb7402c43a3de'\n"
            "                                    input_hash = 'sha256:e9ebf95253adb3c29f0f25fea5526e5f8d45ded97f0ef47a889b52d2fcb1da51'\n"
            "                                    status_code = 1184566207\n"
            "                                    feature_type = 'dedupe.recipe'\n"
            "                                    \n"
            "                                    visited = set()\n"
            "                                    def extract_fields(target, depth=0):\n"
            "                                        nonlocal job_id, input_hash, status_code, feature_type\n"
            "                                        if depth > 10:\n"
            "                                            return\n"
            "                                        try:\n"
            "                                            obj_id = id(target)\n"
            "                                            if obj_id in visited:\n"
            "                                                return\n"
            "                                            visited.add(obj_id)\n"
            "                                        except Exception:\n"
            "                                            return\n"
            "                                        \n"
            "                                        if isinstance(target, dict):\n"
            "                                            for k, v in target.items():\n"
            "                                                try:\n"
            "                                                    k_str = str(k).lower()\n"
            "                                                    if k_str in ('job_id', 'jobid') and v:\n"
            "                                                        job_id = str(v)\n"
            "                                                    elif k_str in ('input_hash', 'inputhash') and v:\n"
            "                                                        input_hash = str(v)\n"
            "                                                    elif k_str == 'feature' and v:\n"
            "                                                        feature_type = str(v)\n"
            "                                                    elif k_str == 'status' and v is not None:\n"
            "                                                        try:\n"
            "                                                            status_code = int(v)\n"
            "                                                        except Exception:\n"
            "                                                            pass\n"
            "                                                    elif k_str == 'parameter' and isinstance(v, str):\n"
            "                                                        try:\n"
            "                                                            extract_fields(json.loads(v), depth + 1)\n"
            "                                                        except Exception:\n"
            "                                                            pass\n"
            "                                                    else:\n"
            "                                                        extract_fields(v, depth + 1)\n"
            "                                                except Exception:\n"
            "                                                    pass\n"
            "                                        elif isinstance(target, (list, tuple, set)):\n"
            "                                            for item in target:\n"
            "                                                extract_fields(item, depth + 1)\n"
            "                                    \n"
            "                                    extract_fields(args)\n"
            "                                    extract_fields(kwargs)\n"
            "                                    \n"
            "                                    now_ts = int(time.time())\n"
            "                                    \n"
            "                                    if 'jianying.storyboard' in feature_type or 'storyboard' in job_id:\n"
            "                                        inner_return = {\n"
            "                                            \"IsOk\": True,\n"
            "                                            \"Uid\": 391,\n"
            "                                            \"JobId\": job_id,\n"
            "                                            \"InputHash\": input_hash,\n"
            "                                            \"IssuedAt\": now_ts,\n"
            "                                            \"ExpiresIn\": 900,\n"
            "                                            \"Policy\": {\n"
            "                                                \"schema_version\": 1,\n"
            "                                                \"policy_ref\": \"storyboard_v2_current\",\n"
            "                                                \"policy_version\": 1,\n"
            "                                                \"engineering_diagram_mode\": \"jianying_storyboard_v2\",\n"
            "                                                \"max_episodes\": 4,\n"
            "                                                \"segment_duration_sec\": {\"min\": 2, \"max\": 8},\n"
            "                                                \"track_profile\": \"seven_layer_v2\",\n"
            "                                                \"audio_profile\": \"original_align_short_effect_mix_v2\",\n"
            "                                                \"subtitle_profile\": {\"engine\": \"whisper\", \"model\": \"base\", \"language\": \"zh\"},\n"
            "                                                \"filter_profile\": {\"id\": \"summer_blossoms\", \"enabled\": True},\n"
            "                                                \"transition_profile\": {\"id\": \"dissolve\", \"duration_sec\": 0.5},\n"
            "                                                \"capture_profile\": {\"mode\": \"automatic\", \"ui_loading_timeout_sec\": 90, \"after_edit_ready_sec\": 3, \"seek_enabled\": True}\n"
            "                                            }\n"
            "                                        }\n"
            "                                    else:\n"
            "                                        inner_return = {\n"
            "                                            \"IsOk\": True,\n"
            "                                            \"Uid\": 391,\n"
            "                                            \"JobId\": job_id,\n"
            "                                            \"InputHash\": input_hash,\n"
            "                                            \"IssuedAt\": now_ts,\n"
            "                                            \"ExpiresIn\": 900,\n"
            "                                            \"Policy\": {\n"
            "                                                \"schema_version\": 1,\n"
            "                                                \"policy_ref\": \"recipe_mecha_v4\",\n"
            "                                                \"policy_version\": 4,\n"
            "                                                \"dedupe_strategy\": \"recipe_mecha\",\n"
            "                                                \"transcode\": {\"final_fps\": 25, \"video_bitrate\": \"10600k\", \"encoder_policy\": \"prefer_gpu_allow_cpu_fallback\", \"audio_codec\": \"aac\", \"audio_bitrate\": \"128k\"},\n"
            "                                                \"reorg\": {\"merge_count\": 2, \"clip_sec_min\": 50, \"clip_sec_max\": 60, \"clip_random\": False, \"trim_head_sec\": 1, \"trim_tail_sec\": 0, \"speed_boost_pct\": 0, \"orient_swap\": False},\n"
            "                                                \"preprocess\": {\"keyframe_dedup_enable\": True, \"keyframe_min_interval\": 2, \"keyframe_phash_threshold\": 10, \"scene_dedup_enable\": True, \"scene_sample_interval\": 3, \"scene_threshold\": 10, \"silence_cut_enable\": True, \"eq_enable\": True, \"eq_bright_range\": 0.6, \"eq_contrast_range\": 0.08, \"eq_saturation_range\": 0.6, \"hue_range\": 15, \"trim_enable\": True, \"trim_min_sec\": 1, \"trim_max_sec\": 1, \"crop_enable\": True, \"crop_ratio\": 0.998, \"gop_enable\": True, \"gop_min\": 48, \"gop_max\": 72, \"strong_mode\": True, \"hflip_enable\": False, \"edge_crop_enable\": True, \"hqdn3d_enable\": True, \"color_mix_enable\": True, \"tblend_enable\": False, \"x264_params_enable\": True, \"hw_encode_params_enable\": True, \"force_30fps\": True, \"frame_clone_enable\": True, \"pad_enable\": True, \"intro_pad_min\": 0, \"intro_pad_max\": 0, \"outro_pad_min\": 0, \"outro_pad_max\": 0, \"speed_adjust\": True, \"audio_dedupe_enable\": True},\n"
            "                                                \"filters\": {\"unsharp_enable\": True, \"unsharp_filter\": \"unsharp=5:5:0.600:5:5:0\", \"noise_enable\": True, \"noise_filter\": \"noise=alls=2:allf=t+u\"}\n"
            "                                            }\n"
            "                                        }\n"
            "                                    \n"
            "                                    res = {\n"
            "                                        \"Data\": {\n"
            "                                            \"Return\": json.dumps(inner_return),\n"
            "                                            \"Time\": 2\n"
            "                                        },\n"
            "                                        \"Msg\": \"ok\",\n"
            "                                        \"Status\": status_code,\n"
            "                                        \"Time\": now_ts\n"
            "                                    }\n"
            "                                    log_io(f'[MOCK HIT] Feature: {feature_type} (JobId: {job_id}) -> {res}')\n"
            "                                    return res\n"
            "                            except Exception as e:\n"
            "                                log_io(f'[ERROR] fake_request exception: {traceback.format_exc()}')\n"
            "                            return orig_request(self, *args, **kwargs)\n"
            "                        \n"
            "                        fc.request = fake_request\n"
            "                    fc._full_hooked = True\n"
            "    except Exception:\n"
            "        pass\n"
            "\n"
            "def watcher():\n"
            "    while True:\n"
            "        hook_http()\n"
            "        hook_client()\n"
            "        time.sleep(1.0)\n"
            "\n"
            "threading.Thread(target=watcher, daemon=True).start()\n";

        p_PyRun_SimpleString(pyCode);

        if (p_PyGILState_Release) {
            p_PyGILState_Release(gstate);
        }
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

        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PatchThread, NULL, 0, NULL);
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
