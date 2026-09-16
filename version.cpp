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
volatile long g_hookHitCount = 0; // 记录 Hook 触发次数

SECURITY_STATUS SEC_ENTRY Detour_DecryptMessage(
    PCtxtHandle     phContext,
    PSecBufferDesc  pMessage,
    unsigned long   MessageSeqNo,
    unsigned long* pfQOP
) {
    // 只要系统解密函数被调用，这里就会自增并强制打印，哪怕内容为空也能证明 Hook 生效了
    long count = InterlockedIncrement(&g_hookHitCount);
    std::cout << "[*] DecryptMessage called! Hit count: " << count << std::endl;
    std::cout.flush(); // 强制刷新缓冲区

    SECURITY_STATUS status = True_DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

    if (status == SEC_E_OK && pMessage != nullptr) {
        for (unsigned long i = 0; i < pMessage->cBuffers; i++) {
            PSecBuffer pBuffer = &pMessage->pBuffers[i];
            if (pBuffer->BufferType == SECBUFFER_DATA && pBuffer->cbBuffer > 0) {
                std::string decryptedResponse((char*)pBuffer->pvBuffer, pBuffer->cbBuffer);
                std::cout << "\n[HTTPS RECV CLEAR TEXT] >>>\n" << decryptedResponse << "\n<<<\n";
                std::cout.flush();
            }
        }
    }
    return status;
}

void InitHook() {
    // 打印一行确认 DLL 已经成功加载并执行了初始化
    std::cout << "[+] version.dll injected and InitHook started..." << std::endl;
    std::cout.flush();

    if (MH_Initialize() != MH_OK) {
        std::cout << "[-] MH_Initialize failed!" << std::endl;
        return;
    }

    HMODULE hSecur32 = LoadLibraryA("secur32.dll");
    if (hSecur32) {
        FARPROC pDecrypt = GetProcAddress(hSecur32, "DecryptMessage");
        if (pDecrypt) {
            if (MH_CreateHook(pDecrypt, &Detour_DecryptMessage, (LPVOID*)&True_DecryptMessage) == MH_OK) {
                MH_EnableHook(MH_ALL_HOOKS);
                std::cout << "[+] DecryptMessage Hook Enabled Successfully!" << std::endl;
            } else {
                std::cout << "[-] MH_CreateHook failed!" << std::endl;
            }
        } else {
            std::cout << "[-] GetProcAddress DecryptMessage failed!" << std::endl;
        }
    } else {
        std::cout << "[-] LoadLibrary secur32.dll failed!" << std::endl;
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
