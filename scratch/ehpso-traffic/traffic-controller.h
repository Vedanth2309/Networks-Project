#ifndef TRAFFIC_CONTROLLER_H
#define TRAFFIC_CONTROLLER_H

#include "ns3/event-id.h"
#include "ns3/ptr.h"

#include <vector>

class TrafficApplication;
class TrafficMonitor;
class EHPSO;

class TrafficController
{
public:
    TrafficController(
        const std::vector<
            ns3::Ptr<TrafficApplication>>& applications,
        TrafficMonitor* monitor,
        EHPSO* optimizer,
        double bottleneckCapacity,
        double controlInterval,
        double profileInterval,
        double simulationTime);

    void Start();

private:
    void RunController();

    const std::vector<
        ns3::Ptr<TrafficApplication>>&
        m_applications;

    TrafficMonitor*
        m_monitor;

    EHPSO*
        m_optimizer;

    double m_bottleneckCapacity;

    double m_controlInterval;

    double m_profileInterval;

    double m_simulationTime;

    ns3::EventId m_controllerEvent;
};

#endif