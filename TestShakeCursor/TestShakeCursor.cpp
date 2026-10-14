#include <Windows.h>
#include <cstdio>
#include "../ShakeCursor.h"

static void CALLBACK OnSCEvent(int eventType, LPARAM lParam, LPVOID /*userData*/) 
{
    switch (eventType) 
    {
    case SC_EVENT_CURSOR_ENLARGED:
        printf("[Event] Cursor enlarged (shake detected)\n");
        break;
    case SC_EVENT_CURSOR_RESTORED:
        printf("[Event] Cursor restored\n");
        break;
    case SC_EVENT_AUTO_CROSS_SCREEN:
        printf("[Event] Auto cross-screen jump, edge index = %lld\n", (long long)lParam);
        break;
    default:
        printf("[Event] Unknown event %d\n", eventType);
        break;
    }
}

static void PrintStatus() 
{
    printf("  Shake Cursor: %s\n", SC_IsShakeCursorEnabled() ? "ON" : "OFF");
    printf("  Auto Cross-Screen: %s\n", SC_IsAutoCrossScreenEnabled() ? "ON" : "OFF");
}

static void PrintMenu() 
{
    printf("\n========================================\n");
    printf("  ShakeCursor DLL Test\n");
    printf("========================================\n");
    printf("  1 - Toggle Shake Cursor (shake-to-find)\n");
    printf("  2 - Toggle Auto Cross-Screen\n");
    printf("  3 - Show current status\n");
    printf("  Q - Quit\n");
    printf("========================================\n");
    PrintStatus();
    printf("\nPress key> ");
}

int main() 
{
    printf("Initializing ShakeCursor DLL...\n");

    SC_Config config = {};
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

    BOOL ok = SC_Initialize(&config);
    if (!ok) 
    {
        printf("SC_Initialize failed!\n");
        return 1;
    }
    printf("SC_Initialize OK\n");

    SC_SetCallback(OnSCEvent, NULL);

    BOOL ok2 = SC_Initialize(&config);
    printf("Double SC_Initialize returned: %s (expected: FALSE)\n", ok2 ? "TRUE" : "FALSE");

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
            BOOL cur = SC_IsShakeCursorEnabled();
            SC_EnableShakeCursor(!cur);

            Sleep(50);
            printf("Shake Cursor: %s -> %s\n",
                cur ? "ON" : "OFF",
                SC_IsShakeCursorEnabled() ? "ON" : "OFF");
            if (SC_IsShakeCursorEnabled()) {
                printf("  >>> Shake your mouse rapidly back and forth to test! <<<\n");
            }
            break;
        }
        case '2': 
        {
            BOOL cur = SC_IsAutoCrossScreenEnabled();
            SC_EnableAutoCrossScreen(!cur);
            Sleep(50);
            printf("Auto Cross-Screen: %s -> %s\n",
                cur ? "ON" : "OFF",
                SC_IsAutoCrossScreenEnabled() ? "ON" : "OFF");
            if (SC_IsAutoCrossScreenEnabled()) {
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
    SC_Uninitialize();
    printf("Done. Bye!\n");

    SC_Uninitialize();
    printf("Double SC_Uninitialize: OK (no crash)\n");

    return 0;
}
