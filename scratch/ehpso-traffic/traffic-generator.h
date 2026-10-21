#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include "hospital-config.h"

#include <cstdint>
#include <string>
#include <vector>

class TrafficGenerator
{
public:
    struct TrafficPoint
    {
        double time;
        double demandMbps;
    };

    struct TrafficSource
    {
        uint32_t id;
        std::string deviceId;
        std::string category;
        uint32_t priority;
        uint32_t packetSize;
        double startTime;
        double stopTime;
        double maxDelayMs;
        std::vector<TrafficPoint> profile;
    };

    TrafficGenerator(
        const std::vector<HospitalDeviceConfig>& devices,
        double simulationTime,
        double profileInterval,
        uint32_t seed);

    std::vector<TrafficSource> GenerateTraffic();

    void PrintTraffic(
        const std::vector<TrafficSource>& traffic) const;

private:
    std::vector<HospitalDeviceConfig> m_devices;
    double m_simulationTime;
    double m_profileInterval;
    uint32_t m_seed;
};

#endif
