#pragma once

void NotifyWindowCreate(void);
void NotifyWindowDestroy(void);
void CursorEnlarge(void);
void CursorRestore(void);
void LoadBigCursor(void);
void DestroyCursorCache(void);
void CrossScreenJump(int edgeIndex);
void FireCallback(int eventType, LPARAM lParam);
