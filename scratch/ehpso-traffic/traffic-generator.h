#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include <cstdint>
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

        uint32_t packetSize;

        double startTime;
        double stopTime;

        std::vector<TrafficPoint> profile;
    };

    TrafficGenerator(
        uint32_t numberOfSources,
        uint32_t packetSize,
        double minDemand,
        double maxDemand,
        double simulationTime,
        double profileInterval,
        uint32_t seed);

    std::vector<TrafficSource>
    GenerateTraffic();

    void PrintTraffic(
        const std::vector<TrafficSource>& traffic) const;

private:
    uint32_t m_numberOfSources;

    uint32_t m_packetSize;

    double m_minDemand;
    double m_maxDemand;

    double m_simulationTime;
    double m_profileInterval;

    uint32_t m_seed;
};

#endif