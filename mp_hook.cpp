#include "pch.h"
#include "mp_internal.h"
#include "mp_hook.h"
#include "mp_detect.h"

static ShakeDetector g_shakeDetector;
static bool g_shakeDetectorConfigured = false;

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

void HookInstall(void)
{
    if (g_state.mouseHook)
        return;

    if (!g_shakeDetectorConfigured) {
        g_shakeDetector.Configure(
            g_state.shakeThresholdPixels,
            g_state.shakePatternLength,
            g_state.shakeMinIntervalMs,
            g_state.shakeMaxIntervalMs);
        g_shakeDetectorConfigured = true;
    }

    g_state.mouseHook = SetWindowsHookEx(
        WH_MOUSE_LL, LowLevelMouseProc, g_hDllInstance, 0);
}

void HookUninstall(void) 
{
    if (g_state.mouseHook) {
        UnhookWindowsHookEx(g_state.mouseHook);
        g_state.mouseHook = NULL;
    }
}

void HookUpdateState(void)
{
    BOOL needHook = g_state.enablePositioning || g_state.enableCrossScreen;
    if (needHook && !g_state.mouseHook)
    {
        HookInstall();
    } 
    else if (!needHook && g_state.mouseHook)
    {
        HookUninstall();
    }
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE)
    {
        PMSLLHOOKSTRUCT pMouse = (PMSLLHOOKSTRUCT)lParam;
        POINT pt = pMouse->pt;
        DWORD time = pMouse->time;

        if (g_state.enableCrossScreen || g_state.enablePositioning)
        {
            for (int i = 0; i < (int)g_state.edgeZones.size(); i++) 
            {
                const EdgeZone& ez = g_state.edgeZones[i];
                if (pt.x >= ez.zoneRect.left  && pt.x <= ez.zoneRect.right &&
                    pt.y >= ez.zoneRect.top    && pt.y <= ez.zoneRect.bottom)
                {
                    PostThreadMessage(g_state.workerThreadId,
                        WM_MP_EDGE_HIT, MAKEWPARAM(pt.x, pt.y), (LPARAM)i);
                    break;
                }
            }
        }

        if (g_state.enablePositioning) 
        {
            g_shakeDetector.Feed(pt, time);
            if (g_shakeDetector.IsShakeDetected()) 
            {
                PostThreadMessage(g_state.workerThreadId, WM_MP_SHAKE_DETECTED, 0, 0);
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
