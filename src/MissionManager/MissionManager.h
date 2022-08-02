/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "PlanManager.h"

Q_DECLARE_LOGGING_CATEGORY(MissionManagerLog)

class MissionManager : public PlanManager
{
    Q_OBJECT
    
public:
    MissionManager(Vehicle* vehicle);
    ~MissionManager();
        
    /// Current mission item as reported by MISSION_CURRENT
    int currentIndex(void) const { return _currentMissionIndex; }

    /// Last current mission item reported while in Mission flight mode
    int lastCurrentIndex(void) const { return _lastCurrentIndex; }

private slots:
    void _shenHangMessageReceived(const ShenHangProtocolMessage& message);

private:
    void _handleHighLatency(const mavlink_message_t& message);
    void _handleHighLatency2(const mavlink_message_t& message);
    void _handleMissionCurrent(const mavlink_message_t& message);
    void _updateMissionIndex(int index);
    void _handleHeartbeat(const mavlink_message_t& message);

    int _cachedLastCurrentIndex;
};
