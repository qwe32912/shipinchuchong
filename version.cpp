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
    // 必须先调用原函数让程序正常解密，否则网络直接瘫痪
    SECURITY_STATUS status = True_DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

    // 使用 __try / __except 保护，防止野指针直接导致客户端闪退
    __try {
        if (status == SEC_E_OK && pMessage != nullptr && pMessage->pBuffers != nullptr) {
            std::cout << "[*] DecryptMessage Success, cBuffers = " << pMessage->cBuffers << std::endl;
            std::cout.flush();

            for (unsigned long i = 0; i < pMessage->cBuffers; i++) {
                PSecBuffer pBuffer = &pMessage->pBuffers[i];
                if (pBuffer && pBuffer->BufferType == SECBUFFER_DATA && pBuffer->cbBuffer > 0 && pBuffer->pvBuffer != nullptr) {
                    std::string decryptedResponse((char*)pBuffer->pvBuffer, pBuffer->cbBuffer);
                    std::cout << "\n[HTTPS RECV CLEAR TEXT] >>>\n" << decryptedResponse << "\n<<<\n";
                    std::cout.flush();
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // 如果读取内存崩溃，在这里拦截并打印，保护程序不闪退
        std::cout << "[-] Exception caught inside Detour_DecryptMessage!" << std::endl;
        std::cout.flush();
    }

    return status;
}

void InitHook() {
    std::cout << "[+] version.dll injected and InitHook started..." << std::endl;
    std::cout.flush();

    if (MH_Initialize() != MH_OK) return;

    HMODULE hSecur32 = LoadLibraryA("secur32.dll");
    if (hSecur32) {
        FARPROC pDecrypt = GetProcAddress(hSecur32, "DecryptMessage");
        if (pDecrypt) {
            if (MH_CreateHook(pDecrypt, &Detour_DecryptMessage, (LPVOID*)&True_DecryptMessage) == MH_OK) {
                MH_EnableHook(MH_ALL_HOOKS);
                std::cout << "[+] DecryptMessage Hook Enabled Successfully!" << std::endl;
            }
        }
    }
    std::cout.flush();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        
        std::cout << "[=] DLL_PROCESS_ATTACH triggered." << std::endl;
        std::cout.flush();

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
