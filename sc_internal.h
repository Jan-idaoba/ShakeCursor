#pragma once

#include "pch.h"
#include "ShakeCursor.h"
#include <vector>

// 系统光标条目数
#define SC_SYSTEM_CURSOR_COUNT 13

extern HINSTANCE g_hDllInstance;

// ---- 内部自定义消息号 ----
// 用于 PostThreadMessage 到工作线程
#define WM_SC_ENABLE_POSITIONING    (WM_APP + 1)   // wParam = BOOL enable
#define WM_SC_ENABLE_CROSSSCREEN    (WM_APP + 2)   // wParam = BOOL enable
#define WM_SC_SHAKE_DETECTED        (WM_APP + 3)   // 检测到摇晃手势
#define WM_SC_EDGE_HIT              (WM_APP + 4)   // wParam = MAKEWPARAM(x, y), lParam = edgeIndex
#define WM_SC_MONITORS_CHANGED      (WM_APP + 5)   // 显示器配置变化

// ---- 内部显示器信息 ----
struct InternalMonitorInfo {
    int     index;
    RECT    monitorRect;
    RECT    workRect;
};

// ---- 边缘死角信息 ----
struct EdgeZone {
    RECT    zoneRect;       // 死角检测矩形
    POINT   crossTarget;    // 自动跨屏跳转目标点
};

// ---- 全局运行时状态 ----
struct SCState {
    // 工作线程
    HANDLE  workerThread;
    DWORD   workerThreadId;
    BOOL    initialized;

    volatile BOOL   enablePositioning;
    volatile BOOL   enableCrossScreen;

    // 钩子
    HHOOK   mouseHook;

    // 配置
    int     shakeThresholdPixels;
    int     shakePatternLength;
    int     shakeMinIntervalMs;
    int     shakeMaxIntervalMs;
    int     cursorSize;
    int     cursorResetTimeoutMs;
    int     edgeZoneWidth;
    int     crossScreenOffset;
    WCHAR   cursorFilePath[MAX_PATH];

    // 回调
    SRWLOCK     callbackLock;
    SC_Callback callback;
    LPVOID      userData;

    HWND    notifyWnd;

    // 光标
    BOOL    cursorEnlarged;
    UINT_PTR cursorResetTimer;

    // 放大光标缓存
    HCURSOR bigCursors[SC_SYSTEM_CURSOR_COUNT];
    BOOL    cursorCacheBuilt;

    // GDI+
    ULONG_PTR gdiplusToken;

    // 显示器 & 死角（仅工作线程访问，无需加锁）
    std::vector<InternalMonitorInfo> monitors;
    std::vector<EdgeZone>           edgeZones;
};

extern SCState g_state;
