#include "pch.h"
#include "sc_internal.h"
#include "sc_monitor.h"


static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC /*hdc*/, LPRECT /*lpRect*/, LPARAM lParam) 
{
    auto* monitors = reinterpret_cast<std::vector<InternalMonitorInfo>*>(lParam);

    MONITORINFOEX mi = {};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfo(hMonitor, &mi)) 
    {
        InternalMonitorInfo info;
        info.index       = (int)monitors->size();
        info.monitorRect = mi.rcMonitor;
        info.workRect    = mi.rcWork;
        monitors->push_back(info);
    }
    return TRUE;
}

void MonitorEnumerate(void) 
{
    g_state.monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)&g_state.monitors);

    std::sort(g_state.monitors.begin(), g_state.monitors.end(),
        [](const InternalMonitorInfo& a, const InternalMonitorInfo& b) {
            if (a.monitorRect.left != b.monitorRect.left)
                return a.monitorRect.left < b.monitorRect.left;
            return a.monitorRect.top < b.monitorRect.top;
        });

    for (int i = 0; i < (int)g_state.monitors.size(); i++) {
        g_state.monitors[i].index = i;
    }
}

static void DeduplicateEdgeZones(void)
{
    auto& zones = g_state.edgeZones;
    for (size_t i = 0; i < zones.size(); i++) {
        for (size_t j = i + 1; j < zones.size(); ) {
            if (EqualRect(&zones[i].zoneRect, &zones[j].zoneRect)) {
                zones.erase(zones.begin() + j);
            } else {
                j++;
            }
        }
    }
}

void EdgeZoneRebuild(void) 
{
    g_state.edgeZones.clear();

    int n = (int)g_state.monitors.size();
    if (n < 2) return;

    int edgeW  = g_state.edgeZoneWidth;         // 默认 20
    int offset = g_state.crossScreenOffset;     // 默认 40

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++) 
        {
            if (i == j) continue;

            const RECT& A = g_state.monitors[i].monitorRect;
            const RECT& B = g_state.monitors[j].monitorRect;

            RECT overlap;
            overlap.left   = max(A.left, B.left);
            overlap.right  = min(A.right, B.right);
            overlap.top    = max(A.top, B.top);
            overlap.bottom = min(A.bottom, B.bottom);

            if (A.right == B.left && overlap.bottom > overlap.top) 
            {
                if (A.top < overlap.top) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.right - edgeW, A.top, A.right + 5, overlap.top };
                    ez.crossTarget.x = overlap.left + offset;
                    ez.crossTarget.y = overlap.top + offset;
                    g_state.edgeZones.push_back(ez);
                }
                // 下方死角：overlap.bottom 到 A.bottom
                if (overlap.bottom < A.bottom) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.right - edgeW, overlap.bottom, A.right + 5, A.bottom };
                    ez.crossTarget.x = overlap.left + offset;
                    ez.crossTarget.y = overlap.bottom - offset;
                    g_state.edgeZones.push_back(ez);
                }
            }

            if (A.left == B.right && overlap.bottom > overlap.top) {
                if (A.top < overlap.top) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.left - 5, A.top, A.left + edgeW, overlap.top };
                    ez.crossTarget.x = overlap.right - offset;
                    ez.crossTarget.y = overlap.top + offset;
                    g_state.edgeZones.push_back(ez);
                }
                if (overlap.bottom < A.bottom) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.left - 5, overlap.bottom, A.left + edgeW, A.bottom };
                    ez.crossTarget.x = overlap.right - offset;
                    ez.crossTarget.y = overlap.bottom - offset;
                    g_state.edgeZones.push_back(ez);
                }
            }

            // ------ A 下边缘的死角 ------
            if (A.bottom == B.top && overlap.right > overlap.left) {
                if (A.left < overlap.left) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.left, A.bottom - edgeW, overlap.left, A.bottom + 5 };
                    ez.crossTarget.x = overlap.left + offset;
                    ez.crossTarget.y = overlap.top + offset;
                    g_state.edgeZones.push_back(ez);
                }
                if (overlap.right < A.right) {
                    EdgeZone ez;
                    ez.zoneRect  = { overlap.right, A.bottom - edgeW, A.right, A.bottom + 5 };
                    ez.crossTarget.x = overlap.right - offset;
                    ez.crossTarget.y = overlap.top + offset;
                    g_state.edgeZones.push_back(ez);
                }
            }

            // ------ A 上边缘的死角 ------
            if (A.top == B.bottom && overlap.right > overlap.left) {
                if (A.left < overlap.left) {
                    EdgeZone ez;
                    ez.zoneRect  = { A.left, A.top - 5, overlap.left, A.top + edgeW };
                    ez.crossTarget.x = overlap.left + offset;
                    ez.crossTarget.y = overlap.bottom - offset;
                    g_state.edgeZones.push_back(ez);
                }
                if (overlap.right < A.right) {
                    EdgeZone ez;
                    ez.zoneRect  = { overlap.right, A.top - 5, A.right, A.top + edgeW };
                    ez.crossTarget.x = overlap.right - offset;
                    ez.crossTarget.y = overlap.bottom - offset;
                    g_state.edgeZones.push_back(ez);
                }
            }
        }
    }

    // 去重
    DeduplicateEdgeZones();
}
