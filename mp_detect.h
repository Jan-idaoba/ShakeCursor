#pragma once

#include <Windows.h>

// 4-way reversal-count shake detector.
class ShakeDetector
{
public:
    void Configure(int thresholdPixels, int patternLength,
        int minIntervalMs, int maxIntervalMs);

    void Feed(POINT pt, DWORD time);

    bool IsShakeDetected();

    void Reset();

private:
    // 1=L, 2=R, 3=U, 4=D, 0=无
    static int CalcDirection4(int dx, int dy);

private:
    int     m_patternLen    = 4;
    int     m_thresholdSq   = 130 * 130;
    int     m_minInterval   = 100;
    int     m_maxInterval   = 300;

    POINT   m_lastPt        = {};
    DWORD   m_lastTime      = 0;
    bool    m_hasLastPt     = false;

    int     m_lastDir       = 0;    
    int     m_reversalCount = 0;    
    bool    m_triggered     = false;
};
