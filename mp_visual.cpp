#include "pch.h"
#include "mp_internal.h"
#include "mp_visual.h"
#include "resource.h"

static const WCHAR* NOTIFY_CLASS_NAME = L"MP_NotifyWnd_{A7B3C9D1}";

static LRESULT CALLBACK NotifyWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DISPLAYCHANGE)
    {
        PostThreadMessage(g_state.workerThreadId, WM_MP_MONITORS_CHANGED, 0, 0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void NotifyWindowCreate(void)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc   = NotifyWndProc;
    wc.hInstance     = g_hDllInstance;
    wc.lpszClassName = NOTIFY_CLASS_NAME;
    RegisterClassEx(&wc);

    g_state.notifyWnd = CreateWindowEx(
        0,
        NOTIFY_CLASS_NAME, L"",
        WS_POPUP,
        0, 0, 0, 0,
        NULL, NULL, g_hDllInstance, NULL);
}

void NotifyWindowDestroy(void)
{
    if (g_state.notifyWnd) {
        DestroyWindow(g_state.notifyWnd);
        g_state.notifyWnd = NULL;
    }
    UnregisterClass(NOTIFY_CLASS_NAME, g_hDllInstance);
}

static const struct
{
    DWORD   setCursorId;    // SetSystemCursor 的 id 参数
    LPCWSTR loadCursorName; // LoadCursor(NULL, ...) 的资源名
    LPCWSTR regValueName;   // HKCU\Control Panel\Cursors 下的注册表值名
} SYSTEM_CURSORS[] = {
    { 32512, IDC_ARROW,       L"Arrow"        },  // OCR_NORMAL
    { 32513, IDC_IBEAM,       L"IBeam"        },  // OCR_IBEAM
    { 32514, IDC_WAIT,        L"Wait"         },  // OCR_WAIT
    { 32515, IDC_CROSS,       L"Crosshair"    },  // OCR_CROSS
    { 32516, IDC_UPARROW,     L"UpArrow"      },  // OCR_UP
    { 32642, IDC_SIZENWSE,    L"SizeNWSE"     },  // OCR_SIZENWSE
    { 32643, IDC_SIZENESW,    L"SizeNESW"     },  // OCR_SIZENESW
    { 32644, IDC_SIZEWE,      L"SizeWE"       },  // OCR_SIZEWE
    { 32645, IDC_SIZENS,      L"SizeNS"       },  // OCR_SIZENS
    { 32646, IDC_SIZEALL,     L"SizeAll"      },  // OCR_SIZEALL
    { 32648, IDC_NO,          L"No"           },  // OCR_NO
    { 32649, IDC_HAND,        L"Hand"         },  // OCR_HAND
    { 32650, IDC_APPSTARTING, L"AppStarting"  },  // OCR_APPSTARTING
};
static_assert(_countof(SYSTEM_CURSORS) == MP_SYSTEM_CURSOR_COUNT,
    "SYSTEM_CURSORS[] count must match MPState::bigCursors[] count");


static void ApplyUnsharpMask(BYTE* pixels, int w, int h, float amount) 
{
    int stride = w * 4;
    int bufSize = stride * h;
    std::vector<BYTE> orig(pixels, pixels + bufSize);

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = (y * w + x) * 4;
            // 4 邻域索引
            int idxT = ((y - 1) * w + x) * 4;
            int idxB = ((y + 1) * w + x) * 4;
            int idxL = (y * w + (x - 1)) * 4;
            int idxR = (y * w + (x + 1)) * 4;

            for (int c = 0; c < 4; c++) { // B, G, R, A 均锐化
                float center = orig[idx + c];
                float neighbors = (orig[idxT + c] + orig[idxB + c] +
                                   orig[idxL + c] + orig[idxR + c]) * 0.25f;
                float sharpened = center + (center - neighbors) * amount;
                // 钳位到 [0, 255]
                if (sharpened < 0.0f) sharpened = 0.0f;
                if (sharpened > 255.0f) sharpened = 255.0f;
                pixels[idx + c] = (BYTE)(sharpened + 0.5f);
            }
        }
    }
}

static BOOL GetCursorSchemeFilePath(LPCWSTR regValueName, WCHAR outPath[MAX_PATH]) 
{
    HKEY hKey = NULL;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Cursors",
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return FALSE;
    }

    WCHAR rawPath[MAX_PATH] = {};
    DWORD cbData = sizeof(rawPath);
    DWORD dwType = 0;
    BOOL ok = FALSE;

    if (RegQueryValueEx(hKey, regValueName, NULL, &dwType,
                        (LPBYTE)rawPath, &cbData) == ERROR_SUCCESS &&
        (dwType == REG_SZ || dwType == REG_EXPAND_SZ) &&
        rawPath[0] != L'\0') 
    {
        if (ExpandEnvironmentStrings(rawPath, outPath, MAX_PATH) > 0) {
            ok = TRUE;
        }
    }

    RegCloseKey(hKey);
    return ok;
}


static HCURSOR LoadCursorFromSchemeFile(LPCWSTR regValueName, int targetSize) 
{
    WCHAR filePath[MAX_PATH] = {};
    if (!GetCursorSchemeFilePath(regValueName, filePath))
        return NULL;

    return (HCURSOR)LoadImage(
        NULL, filePath, IMAGE_CURSOR,
        targetSize, targetSize, LR_LOADFROMFILE);
}

static HCURSOR CreateScaledCursorFromHandle(HCURSOR hSrcCursor, int targetSize) 
{
    ICONINFO srcInfo = {};
    if (!GetIconInfo(hSrcCursor, &srcInfo))
        return NULL;

    BITMAP bm = {};
    GetObject(srcInfo.hbmMask, sizeof(bm), &bm);
    int srcW = bm.bmWidth;
    int srcH = srcInfo.hbmColor ? bm.bmHeight : bm.bmHeight / 2;

    // 计算缩放后的 hotspot
    int hotX = MulDiv(srcInfo.xHotspot, targetSize, srcW);
    int hotY = MulDiv(srcInfo.yHotspot, targetSize, srcH);

    HDC hdcScreen = GetDC(NULL);

    // 1. 将光标绘制到 32bpp bitmap（预乘 alpha 格式）
    BITMAPINFO bmiSrc = {};
    bmiSrc.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmiSrc.bmiHeader.biWidth       = srcW;
    bmiSrc.bmiHeader.biHeight      = -srcH; // top-down
    bmiSrc.bmiHeader.biPlanes      = 1;
    bmiSrc.bmiHeader.biBitCount    = 32;
    bmiSrc.bmiHeader.biCompression = BI_RGB;

    void* srcBits = nullptr;
    HBITMAP hbmSrc = CreateDIBSection(hdcScreen, &bmiSrc, DIB_RGB_COLORS, &srcBits, NULL, 0);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldSrc = SelectObject(hdcSrc, hbmSrc);

    memset(srcBits, 0, srcW * srcH * 4);
    DrawIconEx(hdcSrc, 0, 0, hSrcCursor, srcW, srcH, 0, NULL, DI_NORMAL);

    SelectObject(hdcSrc, oldSrc);

    // 2. GDI+ 双三次插值缩放到目标尺寸
    BITMAPINFO bmiDst = {};
    bmiDst.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmiDst.bmiHeader.biWidth       = targetSize;
    bmiDst.bmiHeader.biHeight      = -targetSize; // top-down
    bmiDst.bmiHeader.biPlanes      = 1;
    bmiDst.bmiHeader.biBitCount    = 32;
    bmiDst.bmiHeader.biCompression = BI_RGB;

    void* dstBits = nullptr;
    HBITMAP hbmColor = CreateDIBSection(hdcScreen, &bmiDst, DIB_RGB_COLORS, &dstBits, NULL, 0);

    if (srcW == targetSize && srcH == targetSize) {
        memcpy(dstBits, srcBits, targetSize * targetSize * 4);
    } else {
        Gdiplus::Bitmap srcBitmap(srcW, srcH, srcW * 4,
            PixelFormat32bppPARGB, (BYTE*)srcBits);

        Gdiplus::Bitmap dstBitmap(targetSize, targetSize, targetSize * 4,
            PixelFormat32bppPARGB, (BYTE*)dstBits);
        Gdiplus::Graphics graphics(&dstBitmap);

        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);

        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.DrawImage(&srcBitmap, 0, 0, targetSize, targetSize);

        // 仅在放大倍率较高时锐化
        float scaleRatio = (float)targetSize / max(srcW, srcH);
        if (scaleRatio > 1.5f) {
            float amount = min(0.6f, 0.2f + (scaleRatio - 1.5f) * 0.1f);
            ApplyUnsharpMask((BYTE*)dstBits, targetSize, targetSize, amount);
        }
    }

    // 3. 从 alpha 通道生成 mask bitmap
    int maskStride = ((targetSize + 31) / 32) * 4;
    std::vector<BYTE> maskBits(maskStride * targetSize, 0xFF);

    BYTE* pDst = (BYTE*)dstBits;
    for (int y = 0; y < targetSize; y++) {
        for (int x = 0; x < targetSize; x++) {
            BYTE alpha = pDst[(y * targetSize + x) * 4 + 3];
            if (alpha > 0) {
                maskBits[y * maskStride + x / 8] &= ~(0x80 >> (x % 8));
            }
        }
    }

    HBITMAP hbmMask = CreateBitmap(targetSize, targetSize, 1, 1, maskBits.data());

    // 4. 创建目标光标
    ICONINFO dstInfo = {};
    dstInfo.fIcon    = FALSE;
    dstInfo.xHotspot = hotX;
    dstInfo.yHotspot = hotY;
    dstInfo.hbmMask  = hbmMask;
    dstInfo.hbmColor = hbmColor;

    HCURSOR hResult = (HCURSOR)CreateIconIndirect(&dstInfo);

    // 清理
    DeleteObject(hbmColor);
    DeleteObject(hbmMask);
    DeleteObject(hbmSrc);
    DeleteDC(hdcSrc);
    ReleaseDC(NULL, hdcScreen);
    DeleteObject(srcInfo.hbmMask);
    if (srcInfo.hbmColor) DeleteObject(srcInfo.hbmColor);

    return hResult;
}

static HCURSOR LoadBigCursorCopy(LPCWSTR cursorName, LPCWSTR regValueName)
{
    // 1) 用户指定文件
    if (g_state.cursorFilePath[0] != L'\0')
    {
        HCURSOR h = (HCURSOR)LoadImage(NULL, g_state.cursorFilePath,
            IMAGE_CURSOR, g_state.cursorSize, g_state.cursorSize, LR_LOADFROMFILE);
        if (h) return h;
    }

    // 2) 用户注册表方案里的光标文件
    HCURSOR hCursor = LoadCursorFromSchemeFile(regValueName, g_state.cursorSize);
    if (hCursor) return hCursor;

    // 3) 拿系统光标
    HCURSOR hSrc = LoadCursor(NULL, cursorName);
    if (!hSrc) return NULL;

    return CreateScaledCursorFromHandle(hSrc, g_state.cursorSize);
}

static VOID CALLBACK CursorResetTimerProc(HWND /*hWnd*/, UINT /*msg*/, UINT_PTR idTimer, DWORD /*dwTime*/)
{
    KillTimer(NULL, idTimer);
    g_state.cursorResetTimer = 0;
    CursorRestore();
}

void LoadBigCursor(void)
{
    if (g_state.cursorFilePath[0] == L'\0')
        return;

    HCURSOR h = (HCURSOR)LoadImage(NULL, g_state.cursorFilePath,
        IMAGE_CURSOR, g_state.cursorSize, g_state.cursorSize, LR_LOADFROMFILE);
    if (!h) {
        g_state.cursorFilePath[0] = L'\0';
        return;
    }
    DestroyCursor(h);
}

void DestroyCursorCache(void)
{
    for (int i = 0; i < MP_SYSTEM_CURSOR_COUNT; i++) {
        if (g_state.bigCursors[i]) {
            DestroyCursor(g_state.bigCursors[i]);
            g_state.bigCursors[i] = NULL;
        }
    }
    g_state.cursorCacheBuilt = FALSE;
}

static void BuildCursorCache(void)
{
    if (g_state.cursorCacheBuilt) return;
    for (int i = 0; i < MP_SYSTEM_CURSOR_COUNT; i++) {
        g_state.bigCursors[i] = LoadBigCursorCopy(
            SYSTEM_CURSORS[i].loadCursorName,
            SYSTEM_CURSORS[i].regValueName);
    }
    g_state.cursorCacheBuilt = TRUE;
}

void CursorEnlarge(void)
{
    if (g_state.cursorEnlarged)
        return;

    BuildCursorCache();

    for (int i = 0; i < MP_SYSTEM_CURSOR_COUNT; i++)
    {
        if (!g_state.bigCursors[i]) 
            continue;

        HCURSOR hCopy = CopyCursor(g_state.bigCursors[i]);
        if (hCopy) {
            SetSystemCursor(hCopy, SYSTEM_CURSORS[i].setCursorId);
        }
    }

    g_state.cursorEnlarged = TRUE;

    FireCallback(MP_EVENT_CURSOR_ENLARGED, 0);

    if (g_state.cursorResetTimer) {
        KillTimer(NULL, g_state.cursorResetTimer);
    }
    g_state.cursorResetTimer = SetTimer(NULL, 0,
        (UINT)g_state.cursorResetTimeoutMs, CursorResetTimerProc);
}

void CursorRestore(void)
{
    if (!g_state.cursorEnlarged) return;

    SystemParametersInfo(SPI_SETCURSORS, 0, 0, SPIF_SENDWININICHANGE);
    g_state.cursorEnlarged = FALSE;

    FireCallback(MP_EVENT_CURSOR_RESTORED, 0);
}

void CrossScreenJump(int edgeIndex) 
{
    if (edgeIndex < 0 || edgeIndex >= (int)g_state.edgeZones.size()) 
        return;

    const EdgeZone& ez = g_state.edgeZones[edgeIndex];
    SetCursorPos(ez.crossTarget.x, ez.crossTarget.y);

    CursorEnlarge();

    FireCallback(MP_EVENT_AUTO_CROSS_SCREEN, (LPARAM)edgeIndex);
}

void FireCallback(int eventType, LPARAM lParam)
{
    MP_Callback cb;
    LPVOID      ud;
    AcquireSRWLockShared(&g_state.callbackLock);
    cb = g_state.callback;
    ud = g_state.userData;
    ReleaseSRWLockShared(&g_state.callbackLock);

    if (cb) 
        cb(eventType, lParam, ud);
}
