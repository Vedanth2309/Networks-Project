#include "traffic-generator.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

TrafficGenerator::TrafficGenerator(
    uint32_t numberOfSources,
    uint32_t packetSize,
    double minDemand,
    double maxDemand,
    double simulationTime,
    double profileInterval,
    uint32_t seed)
    : m_numberOfSources(numberOfSources),
      m_packetSize(packetSize),
      m_minDemand(minDemand),
      m_maxDemand(maxDemand),
      m_simulationTime(simulationTime),
      m_profileInterval(profileInterval),
      m_seed(seed)
{
}

std::vector<TrafficGenerator::TrafficSource>
TrafficGenerator::GenerateTraffic()
{
    std::mt19937 generator(m_seed);

    std::uniform_real_distribution<double>
        baseDistribution(
            m_minDemand,
            m_maxDemand);

    std::uniform_real_distribution<double>
        variationDistribution(
            0.75,
            1.25);

    std::uniform_real_distribution<double>
        eventDistribution(
            0.0,
            1.0);

    std::vector<TrafficSource> traffic;

    for (uint32_t i = 0;
         i < m_numberOfSources;
         ++i)
    {
        TrafficSource source;

        source.id = i + 1;

        source.packetSize =
            m_packetSize;

        source.startTime =
            1.0 + (0.05 * i);

        source.stopTime =
            m_simulationTime - 0.1;

        double baseDemand =
            baseDistribution(generator);

        for (double time = 1.0;
             time < m_simulationTime;
             time += m_profileInterval)
        {
            double demand =
                baseDemand;

            double event =
                eventDistribution(generator);

            /*
             * 15% chance of burst.
             */
            if (event < 0.15)
            {
                demand =
                    std::min(
                        m_maxDemand,
                        baseDemand * 1.8);
            }

            /*
             * Next 10% chance of low traffic.
             */
            else if (event < 0.25)
            {
                demand =
                    std::max(
                        m_minDemand,
                        baseDemand * 0.45);
            }

            /*
             * Normal variation.
             */
            else
            {
                demand =
                    baseDemand *
                    variationDistribution(generator);

                demand =
                    std::max(
                        m_minDemand,
                        std::min(
                            m_maxDemand,
                            demand));
            }

            TrafficPoint point;

            point.time = time;
            point.demandMbps = demand;

            source.profile.push_back(point);
        }

        traffic.push_back(source);
    }

    return traffic;
}

void
TrafficGenerator::PrintTraffic(
    const std::vector<TrafficSource>& traffic) const
{
    std::cout
        << "\nDynamic traffic profile\n"
        << "-----------------------\n";

    std::cout
        << std::fixed
        << std::setprecision(3);

    for (const auto& source : traffic)
    {
        if (source.profile.empty())
        {
            continue;
        }

        const auto& first =
            source.profile.front();

        const auto& last =
            source.profile.back();

        std::cout
            << "IoT "
            << source.id
            << " : "
            << first.demandMbps
            << " Mbps -> "
            << last.demandMbps
            << " Mbps\n";
    }
}