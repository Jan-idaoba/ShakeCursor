#pragma once

#include <Windows.h>

#ifdef MOUSEPOSITIONING_EXPORTS
#define MOUSEPOS_API __declspec(dllexport)
#else
#define MOUSEPOS_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// @config
typedef struct MP_Config
{
    UINT    cbSize;                 // 用于 ABI 版本识别
    int     shakeThresholdPixels;   // 摇晃检测：两次采样间距离阈值（像素），默认 130
    int     shakePatternLength;     // 摇晃检测：交替方向匹配次数，默认 4
    int     shakeMinIntervalMs;     // 摇晃检测：两次采样最小间隔（ms），默认 100
    int     shakeMaxIntervalMs;     // 摇晃检测：两次采样最大间隔（ms），超过则重置，默认 300
    int     cursorSize;             // 放大光标尺寸（像素），默认 128
    int     cursorResetTimeoutMs;   // 光标自动恢复超时（ms），默认 2000
    int     edgeZoneWidth;          // 边缘死角检测带宽度（像素），默认 20
    int     crossScreenOffset;      // 自动跨屏后光标偏移量（像素），默认 40
    LPCWSTR cursorFilePath;         // 自定义大光标文件路径（.cur），NULL 则使用 DLL 内嵌资源
} MP_Config;

// @callback event
typedef enum MP_EventType
{
    MP_EVENT_CURSOR_ENLARGED    = 1,    // 光标已放大（检测到摇晃手势）
    MP_EVENT_CURSOR_RESTORED    = 2,    // 光标已恢复正常
    MP_EVENT_AUTO_CROSS_SCREEN  = 3,    // 已触发自动跨屏，lParam = 目标显示器索引
} MP_EventType;

// @Callback function prototype
//   eventType: MP_EventType
//   lParam:    事件附加数据
//   userData:  用户自定义上下文指针
typedef void (CALLBACK *MP_Callback)(int eventType, LPARAM lParam, LPVOID userData);

//@init
//@param config
MOUSEPOS_API BOOL  MP_Initialize(const MP_Config* config);

//@uninit
MOUSEPOS_API void  MP_Uninitialize(void);

//@enable MP
MOUSEPOS_API void  MP_EnableMousePositioning(BOOL enable);

//@enable CrossScreen
MOUSEPOS_API void  MP_EnableAutoCrossScreen(BOOL enable);

//@query status
MOUSEPOS_API BOOL  MP_IsMousePositioningEnabled(void);
MOUSEPOS_API BOOL  MP_IsAutoCrossScreenEnabled(void);

//@Callback
MOUSEPOS_API void  MP_SetCallback(MP_Callback callback, LPVOID userData);

#ifdef __cplusplus
}
#endif
