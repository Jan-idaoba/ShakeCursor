#include <Windows.h>
#include <cstdio>
#include "../MousePositioning.h"

static void CALLBACK OnMPEvent(int eventType, LPARAM lParam, LPVOID /*userData*/) 
{
    switch (eventType) 
    {
    case MP_EVENT_CURSOR_ENLARGED:
        printf("[Event] Cursor enlarged (shake detected)\n");
        break;
    case MP_EVENT_CURSOR_RESTORED:
        printf("[Event] Cursor restored\n");
        break;
    case MP_EVENT_AUTO_CROSS_SCREEN:
        printf("[Event] Auto cross-screen jump, edge index = %lld\n", (long long)lParam);
        break;
    default:
        printf("[Event] Unknown event %d\n", eventType);
        break;
    }
}

static void PrintStatus() 
{
    printf("  Mouse Positioning: %s\n", MP_IsMousePositioningEnabled() ? "ON" : "OFF");
    printf("  Auto Cross-Screen: %s\n", MP_IsAutoCrossScreenEnabled() ? "ON" : "OFF");
}

static void PrintMenu() 
{
    printf("\n========================================\n");
    printf("  MousePositioning DLL Test\n");
    printf("========================================\n");
    printf("  1 - Toggle Mouse Positioning (shake-to-find)\n");
    printf("  2 - Toggle Auto Cross-Screen\n");
    printf("  3 - Show current status\n");
    printf("  Q - Quit\n");
    printf("========================================\n");
    PrintStatus();
    printf("\nPress key> ");
}

int main() 
{
    printf("Initializing MousePositioning DLL...\n");

    MP_Config config = {};
    config.cbSize               = sizeof(config);
    config.shakeThresholdPixels = 130;
    config.shakePatternLength   = 4;
    config.shakeMinIntervalMs   = 100;
    config.shakeMaxIntervalMs   = 300;
    config.cursorSize           = 128;
    config.cursorResetTimeoutMs = 3000;
    config.edgeZoneWidth        = 20;
    config.crossScreenOffset    = 40;
    config.cursorFilePath       = NULL;

    BOOL ok = MP_Initialize(&config);
    if (!ok) 
    {
        printf("MP_Initialize failed!\n");
        return 1;
    }
    printf("MP_Initialize OK\n");

    MP_SetCallback(OnMPEvent, NULL);

    BOOL ok2 = MP_Initialize(&config);
    printf("Double MP_Initialize returned: %s (expected: FALSE)\n", ok2 ? "TRUE" : "FALSE");

    PrintMenu();

    bool running = true;
    while (running) 
    {
        int ch = getchar();
        if (ch == '\n' || ch == '\r') continue;

        switch (ch)
        {
        case '1':
        {
            BOOL cur = MP_IsMousePositioningEnabled();
            MP_EnableMousePositioning(!cur);

            Sleep(50);
            printf("Mouse Positioning: %s -> %s\n",
                cur ? "ON" : "OFF",
                MP_IsMousePositioningEnabled() ? "ON" : "OFF");
            if (MP_IsMousePositioningEnabled()) {
                printf("  >>> Shake your mouse rapidly back and forth to test! <<<\n");
            }
            break;
        }
        case '2': 
        {
            BOOL cur = MP_IsAutoCrossScreenEnabled();
            MP_EnableAutoCrossScreen(!cur);
            Sleep(50);
            printf("Auto Cross-Screen: %s -> %s\n",
                cur ? "ON" : "OFF",
                MP_IsAutoCrossScreenEnabled() ? "ON" : "OFF");
            if (MP_IsAutoCrossScreenEnabled()) {
                printf("  >>> Move mouse to monitor edge dead zones to test! <<<\n");
            }
            break;
        }
        case '3':
            PrintStatus();
            break;
        case 'q':
        case 'Q':
            running = false;
            break;
        default:
            PrintMenu();
            break;
        }
    }

    printf("\nUninitializing...\n");
    MP_Uninitialize();
    printf("Done. Bye!\n");

    MP_Uninitialize();
    printf("Double MP_Uninitialize: OK (no crash)\n");

    return 0;
}
