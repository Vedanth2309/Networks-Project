#ifndef TRAFFIC_MONITOR_H
#define TRAFFIC_MONITOR_H

#include "ns3/point-to-point-net-device.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <vector>

class TrafficApplication;
class TrafficReceiver;

class TrafficMonitor
{
public:
    TrafficMonitor(
        const std::vector<
            ns3::Ptr<TrafficApplication>>& applications,
        TrafficReceiver* receiver,
        double bottleneckCapacity);

    void PrintCurrentState(
        double currentTime) const;

    void PrintFinalResults(
        double simulationTime) const;

    double CalculateFairness() const;

private:
    const std::vector<
        ns3::Ptr<TrafficApplication>>&
        m_applications;

    TrafficReceiver* m_receiver;

    double m_bottleneckCapacity;
};

#endif