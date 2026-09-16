#include <windows.h>

void ForceLog(const char* msg) {
    HANDLE hFile = CreateFileA("C:\\inject_debug.txt", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, msg, (DWORD)lstrlenA(msg), &written, NULL);
        CloseHandle(hFile);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        ForceLog("[+] SUCCESS: DllMain executed via GitHub Actions build!\r\n");
        break;
    }
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
