#include "traffic-monitor.h"

#include "traffic-application.h"
#include "traffic-receiver.h"

#include <cmath>
#include <iomanip>
#include <iostream>

TrafficMonitor::TrafficMonitor(
    const std::vector<
        ns3::Ptr<TrafficApplication>>& applications,
    TrafficReceiver* receiver,
    double bottleneckCapacity)
    : m_applications(applications),
      m_receiver(receiver),
      m_bottleneckCapacity(
          bottleneckCapacity)
{
}

double
TrafficMonitor::CalculateFairness() const
{
    if (m_applications.empty())
    {
        return 1.0;
    }

    double sum = 0.0;

    double sumSquares = 0.0;

    for (const auto& app :
         m_applications)
    {
        double rate =
            app->GetEffectiveRate();

        sum += rate;

        sumSquares +=
            rate * rate;
    }

    if (sumSquares <= 0.0)
    {
        return 1.0;
    }

    return
        (sum * sum) /
        (static_cast<double>(
             m_applications.size()) *
         sumSquares);
}

void
TrafficMonitor::PrintCurrentState(
    double currentTime) const
{
    double totalDemand = 0.0;

    double totalAllocation = 0.0;

    for (const auto& app :
         m_applications)
    {
        totalDemand +=
            app->GetDemand();

        totalAllocation +=
            app->GetEffectiveRate();
    }

    std::cout
        << "[Monitor "
        << std::fixed
        << std::setprecision(1)
        << currentTime
        << "s] Demand = "
        << std::setprecision(3)
        << totalDemand
        << " Mbps"
        << " | Allocation = "
        << totalAllocation
        << " Mbps"
        << " | Fairness = "
        << std::setprecision(3)
        << CalculateFairness()
        << "\n";
}

void
TrafficMonitor::PrintFinalResults(
    double simulationTime) const
{
    uint64_t totalSent = 0;

    for (const auto& app :
         m_applications)
    {
        totalSent +=
            app->GetPacketsSent();
    }

    uint64_t totalReceived = 0;

    uint64_t totalBytes = 0;

    if (m_receiver)
    {
        totalReceived =
            m_receiver->GetPacketsReceived();

        totalBytes =
            m_receiver->GetBytesReceived();
    }

    uint64_t lost = 0;

    if (totalSent > totalReceived)
    {
        lost =
            totalSent - totalReceived;
    }

    double lossPercentage = 0.0;

    if (totalSent > 0)
    {
        lossPercentage =
            100.0 *
            static_cast<double>(lost) /
            static_cast<double>(totalSent);
    }

    double throughputMbps = 0.0;

    if (simulationTime > 0.0)
    {
        throughputMbps =
            static_cast<double>(
                totalBytes * 8ULL) /
            (simulationTime *
             1000000.0);
    }

    std::cout
        << "\n========================================\n"
        << " FINAL PERFORMANCE\n"
        << "========================================\n";

    std::cout
        << std::fixed
        << std::setprecision(3);

    std::cout
        << "Packets Sent     : "
        << totalSent
        << "\n";

    std::cout
        << "Packets Received : "
        << totalReceived
        << "\n";

    std::cout
        << "Packet Loss      : "
        << lossPercentage
        << " %\n";

    std::cout
        << "Throughput       : "
        << throughputMbps
        << " Mbps\n";

    std::cout
        << "Fairness         : "
        << CalculateFairness()
        << "\n";

    std::cout
        << "========================================\n";
}