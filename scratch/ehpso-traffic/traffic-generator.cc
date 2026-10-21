#include "traffic-generator.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>

TrafficGenerator::TrafficGenerator(
    const std::vector<HospitalDeviceConfig>& devices,
    double simulationTime,
    double profileInterval,
    uint32_t seed)
    : m_devices(devices),
      m_simulationTime(simulationTime),
      m_profileInterval(profileInterval),
      m_seed(seed)
{
    if (simulationTime <= 0.0 || profileInterval <= 0.0)
    {
        throw std::invalid_argument(
            "Simulation time and profile interval must be positive.");
    }
}

std::vector<TrafficGenerator::TrafficSource>
TrafficGenerator::GenerateTraffic()
{
    std::mt19937 generator(m_seed);
    std::uniform_real_distribution<double> random01(0.0, 1.0);

    std::vector<TrafficSource> traffic;
    traffic.reserve(m_devices.size());

    for (std::size_t i = 0; i < m_devices.size(); ++i)
    {
        const HospitalDeviceConfig& device = m_devices[i];

        if (device.meanDemandMbps < 0.0 ||
            device.peakDemandMbps < device.meanDemandMbps)
        {
            throw std::invalid_argument(
                "Invalid mean/peak demand for device: " + device.deviceId);
        }

        TrafficSource source{};
        source.id = static_cast<uint32_t>(i + 1);
        source.deviceId = device.deviceId;
        source.category = device.category;
        source.priority = device.priority;
        source.packetSize = device.packetSizeBytes;
        source.startTime = device.startTimeSeconds;
        source.stopTime = std::min(
            device.stopTimeSeconds, m_simulationTime);
        source.maxDelayMs = device.maxDelayMs;

        const double mean = device.meanDemandMbps;
        const double peak = device.peakDemandMbps;

        for (double time = source.startTime;
             time < source.stopTime;
             time += m_profileInterval)
        {
            double demand = mean + random01(generator) * (peak - mean);

            // Simulate occasional high-demand hospital traffic bursts.
            if (random01(generator) < 0.15)
            {
                demand = peak;
            }

            TrafficPoint point{};
            point.time = time;
            point.demandMbps = demand;
            source.profile.push_back(point);
        }

        if (source.profile.empty())
        {
            TrafficPoint point{};
            point.time = source.startTime;
            point.demandMbps = mean;
            source.profile.push_back(point);
        }

        traffic.push_back(source);
    }

    return traffic;
}

void TrafficGenerator::PrintTraffic(
    const std::vector<TrafficSource>& traffic) const
{
    std::cout << "\nHospital traffic profiles\n"
              << "-------------------------\n"
              << std::fixed << std::setprecision(3);

    for (const auto& source : traffic)
    {
        std::cout << source.deviceId
                  << " | Category: " << source.category
                  << " | Priority: " << source.priority
                  << " | Packets: " << source.packetSize << " bytes";

        if (!source.profile.empty())
        {
            std::cout << " | Initial demand: "
                      << source.profile.front().demandMbps
                      << " Mbps";
        }

        std::cout << '\n';
    }
}
