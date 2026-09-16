#include <windows.h>
#include <iostream>
#include "MinHook.h" // 需要引入 MinHook 库

// 1. 使用 MSVC 链接器指令，将所有 version.dll 的原生导出函数直接转发给系统真正的 version.dll
// 这样可以确保程序原有的文件版本检查功能完全不受影响，不会报错崩溃。
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

// 2. 定义 Windows Schannel 底层解密函数指针（针对 Rust 程序使用的系统级 TLS 加密通信）
typedef SECURITY_STATUS(SEC_ENTRY* DecryptMessage_t)(
    PCtxtHandle     phContext,
    PSecBufferDesc  pMessage,
    unsigned long   MessageSeqNo,
    unsigned long* pfQOP
);

DecryptMessage_t True_DecryptMessage = nullptr;

// 3. Hook 后的解密拦截函数（在这里可以直接拿到 HTTPS 响应明文）
SECURITY_STATUS SEC_ENTRY Detour_DecryptMessage(
    PCtxtHandle     phContext,
    PSecBufferDesc  pMessage,
    unsigned long   MessageSeqNo,
    unsigned long* pfQOP
) {
    SECURITY_STATUS status = True_DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);

    if (status == SEC_E_OK && pMessage != nullptr) {
        for (unsigned long i = 0; i < pMessage->cBuffers; i++) {
            PSecBuffer pBuffer = &pMessage->pBuffers[i];
            // 判断是否为解密后的数据载荷
            if (pBuffer->BufferType == SECBUFFER_DATA && pBuffer->cbBuffer > 0) {
                std::string decryptedResponse((char*)pBuffer->pvBuffer, pBuffer->cbBuffer);
                std::cout << "\n[HTTPS RECV CLEAR TEXT] >>>\n" << decryptedResponse << "\n<<<\n";
            }
        }
    }
    return status;
}

// 4. 初始化 Hook 线程
void InitHook() {
    if (MH_Initialize() != MH_OK) return;

    // Windows 的 TLS 加密实际上在 secur32.dll 中实现
    HMODULE hSecur32 = LoadLibraryA("secur32.dll");
    if (hSecur32) {
        FARPROC pDecrypt = GetProcAddress(hSecur32, "DecryptMessage");
        if (pDecrypt) {
            MH_CreateHook(pDecrypt, &Detour_DecryptMessage, (LPVOID*)&True_DecryptMessage);
            MH_EnableHook(MH_ALL_HOOKS);
            std::cout << "[+] Version Hijack & Schannel Hook Success!" << std::endl;
        }
    }
}

// 5. 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        // 开启控制台窗口用于实时打印捕获的明文日志
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);

        // 启动 Hook 初始化
        InitHook();
        break;

    case DLL_PROCESS_DETACH:
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
