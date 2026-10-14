#include "pch.h"
#include <new>
#include "sc_internal.h"
#include "sc_core.h"
#include "sc_hook.h"
#include "sc_monitor.h"
#include "sc_visual.h"

SCState g_state = {};
// config
static const SC_Config SC_DEFAULT_CONFIG =
{
    sizeof(SC_Config),  // cbSize
    130,    // shakeThresholdPixels
    4,      // shakePatternLength
    100,    // shakeMinIntervalMs
    300,    // shakeMaxIntervalMs
    128,    // cursorSize
    2000,   // cursorResetTimeoutMs
    20,     // edgeZoneWidth
    40,     // crossScreenOffset
    NULL,   // cursorFilePath
};

// cursorSize 上限
static const int SC_CURSOR_SIZE_MAX = 512;

void ApplyConfig(const SC_Config* config) 
{
    const SC_Config* c = config ? config : &SC_DEFAULT_CONFIG;

    g_state.shakeThresholdPixels = c->shakeThresholdPixels > 0 ? c->shakeThresholdPixels : SC_DEFAULT_CONFIG.shakeThresholdPixels;
    g_state.shakePatternLength   = c->shakePatternLength   > 0 ? c->shakePatternLength   : SC_DEFAULT_CONFIG.shakePatternLength;
    g_state.shakeMinIntervalMs   = c->shakeMinIntervalMs   > 0 ? c->shakeMinIntervalMs   : SC_DEFAULT_CONFIG.shakeMinIntervalMs;
    g_state.shakeMaxIntervalMs   = c->shakeMaxIntervalMs   > 0 ? c->shakeMaxIntervalMs   : SC_DEFAULT_CONFIG.shakeMaxIntervalMs;
    g_state.cursorSize           = c->cursorSize           > 0 ? c->cursorSize           : SC_DEFAULT_CONFIG.cursorSize;
    g_state.cursorResetTimeoutMs = c->cursorResetTimeoutMs  > 0 ? c->cursorResetTimeoutMs  : SC_DEFAULT_CONFIG.cursorResetTimeoutMs;
    g_state.edgeZoneWidth        = c->edgeZoneWidth        > 0 ? c->edgeZoneWidth        : SC_DEFAULT_CONFIG.edgeZoneWidth;
    g_state.crossScreenOffset    = c->crossScreenOffset    > 0 ? c->crossScreenOffset    : SC_DEFAULT_CONFIG.crossScreenOffset;

    if (c->cursorFilePath && c->cursorFilePath[0] != L'\0')
    {
        wcsncpy_s(g_state.cursorFilePath, c->cursorFilePath, _TRUNCATE);
    } else
    {
        g_state.cursorFilePath[0] = L'\0';
    }

    if (g_state.shakeMaxIntervalMs <= g_state.shakeMinIntervalMs) {
        g_state.shakeMinIntervalMs = SC_DEFAULT_CONFIG.shakeMinIntervalMs;
        g_state.shakeMaxIntervalMs = SC_DEFAULT_CONFIG.shakeMaxIntervalMs;
    }

    if (g_state.cursorSize > SC_CURSOR_SIZE_MAX) {
        g_state.cursorSize = SC_CURSOR_SIZE_MAX;
    }
}

static DWORD WINAPI WorkerThreadProc(LPVOID /*lpParam*/) 
{
    {
        typedef DPI_AWARENESS_CONTEXT (WINAPI *PFN_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
        HMODULE hUser32 = GetModuleHandle(L"user32.dll");
        if (hUser32) {
            auto pfn = (PFN_SetThreadDpiAwarenessContext)
                GetProcAddress(hUser32, "SetThreadDpiAwarenessContext");
            if (pfn) {
                pfn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            }
        }
    }

    // 1. 枚举显示器、计算死角
    MonitorEnumerate();
    EdgeZoneRebuild();

    // 2. 创建通知窗口（接收 WM_DISPLAYCHANGE）
    NotifyWindowCreate();

    // 3. 安装低级鼠标钩子
    HookInstall();

    // 4. 进入消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) 
    {
        switch (msg.message) 
        {
        case WM_SC_ENABLE_POSITIONING:
        case WM_SC_ENABLE_CROSSSCREEN:
            HookUpdateState();
            break;

        case WM_SC_SHAKE_DETECTED:
            if (g_state.enablePositioning && !g_state.cursorEnlarged) {
                CursorEnlarge();
            }
            break;

        case WM_SC_EDGE_HIT:
            if (g_state.enableCrossScreen) {
                int edgeIdx = (int)msg.lParam;
                CrossScreenJump(edgeIdx);
            } else if (g_state.enablePositioning && !g_state.cursorEnlarged) {
                CursorEnlarge();
            }
            break;

        case WM_SC_MONITORS_CHANGED:
            MonitorEnumerate();
            EdgeZoneRebuild();
            break;

        default:
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;
        }
    }

    // 5. 清理
    HookUninstall();
    NotifyWindowDestroy();
    CursorRestore();
    DestroyCursorCache();

    return 0;
}

SHAKECURSOR_API BOOL SC_Initialize(const SC_Config* config)
{
    if (g_state.initialized)
        return FALSE;

    // cbSize 不匹配 return
    if (config && config->cbSize != sizeof(SC_Config))
        return FALSE;

    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&g_state.gdiplusToken, &gdiplusInput, NULL);

    ApplyConfig(config);
    LoadBigCursor();

    g_state.workerThread = CreateThread(
        NULL, 0, WorkerThreadProc, NULL, 0, &g_state.workerThreadId);

    if (!g_state.workerThread) 
        return FALSE;

    g_state.initialized = TRUE;
    return TRUE;
}

SHAKECURSOR_API void SC_Uninitialize(void) 
{
    if (!g_state.initialized)
        return;

    PostThreadMessage(g_state.workerThreadId, WM_QUIT, 0, 0);
    WaitForSingleObject(g_state.workerThread, 3000);
    CloseHandle(g_state.workerThread);

    if (g_state.cursorEnlarged) {
        SystemParametersInfo(SPI_SETCURSORS, 0, 0, SPIF_SENDWININICHANGE);
    }

    if (g_state.gdiplusToken) {
        Gdiplus::GdiplusShutdown(g_state.gdiplusToken);
    }

    g_state.~SCState();
    new (&g_state) SCState{};
}

SHAKECURSOR_API void SC_EnableShakeCursor(BOOL enable)
{
    if (!g_state.initialized)
        return;

    g_state.enablePositioning = enable;
    PostThreadMessage(g_state.workerThreadId, WM_SC_ENABLE_POSITIONING, enable, 0);
}

SHAKECURSOR_API void SC_EnableAutoCrossScreen(BOOL enable)
{
    if (!g_state.initialized)
        return;

    g_state.enableCrossScreen = enable;
    PostThreadMessage(g_state.workerThreadId, WM_SC_ENABLE_CROSSSCREEN, enable, 0);
}

SHAKECURSOR_API BOOL SC_IsShakeCursorEnabled(void)
{
    return g_state.enablePositioning;
}

SHAKECURSOR_API BOOL SC_IsAutoCrossScreenEnabled(void)
{
    return g_state.enableCrossScreen;
}

SHAKECURSOR_API void SC_SetCallback(SC_Callback callback, LPVOID userData)
{
    AcquireSRWLockExclusive(&g_state.callbackLock);
    g_state.callback = callback;
    g_state.userData = userData;
    ReleaseSRWLockExclusive(&g_state.callbackLock);
}
