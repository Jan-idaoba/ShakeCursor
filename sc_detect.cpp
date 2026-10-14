#include "pch.h"
#include "sc_detect.h"

void ShakeDetector::Configure(int thresholdPixels, int patternLength,
                              int minIntervalMs, int maxIntervalMs)
{
    m_thresholdSq = thresholdPixels * thresholdPixels;
    m_patternLen  = patternLength > 1 ? patternLength : 2;
    m_minInterval = minIntervalMs;
    m_maxInterval = maxIntervalMs;
    Reset();
}

void ShakeDetector::Feed(POINT pt, DWORD time)
{
    if (m_hasLastPt && (int)(time - m_lastTime) > m_maxInterval)
    {
        Reset();
    }

    if (!m_hasLastPt)
    {
        m_lastPt = pt;
        m_lastTime = time;
        m_hasLastPt = true;
        return;
    }

    int dt = (int)(time - m_lastTime);
    if (dt < m_minInterval)
        return;

    int dx = pt.x - m_lastPt.x;
    int dy = pt.y - m_lastPt.y;

    if ((dx * dx + dy * dy) < m_thresholdSq)
    {
        m_lastPt   = pt;
        m_lastTime = time;
        return;
    }

    int dir = CalcDirection4(dx, dy);
    if (dir != 0)
    {
        if (m_lastDir != 0 && dir != m_lastDir)
        {
            m_reversalCount++;
            if (m_reversalCount >= m_patternLen - 1)
                m_triggered = true;
        }
        m_lastDir = dir;
    }

    m_lastPt   = pt;
    m_lastTime = time;
}

bool ShakeDetector::IsShakeDetected()
{
    if (!m_triggered) 
        return false;

    Reset();
    return true;
}

void ShakeDetector::Reset()
{
    m_lastTime       = 0;
    m_hasLastPt      = false;
    m_lastDir        = 0;
    m_reversalCount  = 0;
    m_triggered      = false;
}

int ShakeDetector::CalcDirection4(int dx, int dy)
{
    int ax = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;

    if (ax == 0 && ay == 0) 
        return 0;

    if (ax >= ay)
        return dx < 0 ? 1 : 2; 
    return dy < 0 ? 3 : 4;
}
