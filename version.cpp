#define SECURITY_WIN32
#include <windows.h>
#include <sspi.h>
#include <security.h>
#include <iostream>
#include <MinHook.h>

#pragma comment(linker, "/export:GetFileVersionInfoA=C:\\Windows\\System32\\version.GetFileVersionInfoA")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=C:\\Windows\\System32\\version.GetFileVersionInfoByHandle")
#pragma comment(linker, "/export:GetFileVersionInfoExW=C:\\Windows\\System32\\version.GetFileVersionInfoExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=C:\\Windows\\System32\\version.GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=C:\\Windows\\System32\\version.GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoW=C:\\Windows\\System32\\version.GetFileVersionInfoW")
#pragma comment(linker, "/export:VerFindFileA=C:\\Windows\\System32\\version.VerFindFileA")
#pragma comment(linker, "/export:VerFindFileW=C:\\Windows\\System32\\version.VerFindFileW")
#pragma comment(linker, "/export:VerInstallFileA=C:\\Windows\\System32\\version.VerInstallFileA")
#pragma comment(linker, "/export:VerInstallFileW=C:\\Windows\\System32\\version.VerInstallFileW")
#pragma comment(linker, "/export:VerQueryValueA=C:\\Windows\\System32\\version.VerQueryValueA")
#pragma comment(linker, "/export:VerQueryValueW=C:\\Windows\\System32\\version.VerQueryValueW")

typedef SECURITY_STATUS(SEC_ENTRY* DecryptMessage_t)(
    PCtxtHandle     phContext,
    PSecBufferDesc  pMessage,
    unsigned long   MessageSeqNo,
    unsigned long* pfQOP
);

DecryptMessage_t True_DecryptMessage = nullptr;

SECURITY_STATUS SEC_ENTRY Detour_DecryptMessage(
    PCtxtHandle     phContext,
    PSecBufferDesc  pMessage,
    unsigned long   MessageSeqNo,
    unsigned long* pfQOP
) {
    SECURITY_STATUS status = True_DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

    // 使用 __try 保护，且函数内无 std::string 等带析构的对象，完美绕过 C2712 编译错误
    __try {
        if (status == 0 && pMessage != nullptr && pMessage->pBuffers != nullptr) {
            for (unsigned long i = 0; i < pMessage->cBuffers; i++) {
                PSecBuffer pBuffer = &pMessage->pBuffers[i];
                if (pBuffer && pBuffer->BufferType == SECBUFFER_DATA && pBuffer->cbBuffer > 0 && pBuffer->pvBuffer != nullptr) {
                    std::cout << "\n[HTTPS RECV] >>>\n";
                    std::cout.write((char*)pBuffer->pvBuffer, pBuffer->cbBuffer);
                    std::cout << "\n<<<\n";
                    std::cout.flush();
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 捕获异常，防止因野指针导致客户端崩溃
    }

    return status;
}

void InitHook() {
    if (MH_Initialize() != MH_OK) return;

    HMODULE hSecur32 = LoadLibraryA("secur32.dll");
    if (hSecur32) {
        FARPROC pDecrypt = GetProcAddress(hSecur32, "DecryptMessage");
        if (pDecrypt) {
            if (MH_CreateHook(pDecrypt, &Detour_DecryptMessage, (LPVOID*)&True_DecryptMessage) == MH_OK) {
                MH_EnableHook(MH_ALL_HOOKS);
            }
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        InitHook();
        break;
    }
    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
