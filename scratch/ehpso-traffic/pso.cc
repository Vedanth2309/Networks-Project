#include "pso.h"

#include <algorithm>
#include <limits>
#include <random>
#include <vector>

PSO::PSO(uint32_t populationSize,
         uint32_t iterations,
         double inertiaWeight,
         double cognitiveCoefficient,
         double socialCoefficient,
         uint32_t seed)
    : m_populationSize(populationSize),
      m_iterations(iterations),
      m_inertiaWeight(inertiaWeight),
      m_cognitiveCoefficient(cognitiveCoefficient),
      m_socialCoefficient(socialCoefficient),
      m_seed(seed),
      m_generator(seed),
      m_bestFitness(std::numeric_limits<double>::max())
{
}

double
PSO::CalculateFitness(
    const std::vector<double>& allocation,
    const std::vector<double>& demand,
    double capacity) const
{
    if (demand.empty())
    {
        return 0.0;
    }

    double totalDemand = 0.0;
    double totalAllocation = 0.0;
    double unmetDemand = 0.0;
    double squaredAllocation = 0.0;

    for (uint32_t i = 0;
         i < demand.size();
         ++i)
    {
        totalDemand += demand[i];
        totalAllocation += allocation[i];

        squaredAllocation +=
            allocation[i] * allocation[i];

        if (allocation[i] < demand[i])
        {
            unmetDemand +=
                demand[i] - allocation[i];
        }
    }

    double unmetDemandRatio = 0.0;

    if (totalDemand > 0.0)
    {
        unmetDemandRatio =
            unmetDemand / totalDemand;
    }

    double fairness = 1.0;

    if (squaredAllocation > 0.0)
    {
        fairness =
            (totalAllocation * totalAllocation) /
            (static_cast<double>(demand.size()) *
             squaredAllocation);
    }

    double capacityPenalty = 0.0;

    if (capacity > 0.0 &&
        totalAllocation > capacity)
    {
        capacityPenalty =
            (totalAllocation - capacity) /
            capacity;
    }

    double utilizationPenalty = 0.0;

    if (totalDemand > capacity &&
        capacity > 0.0)
    {
        double utilization =
            totalAllocation / capacity;

        utilizationPenalty =
            std::max(0.0, 1.0 - utilization);
    }

    return
        0.65 * unmetDemandRatio +
        0.20 * (1.0 - fairness) +
        0.10 * utilizationPenalty +
        0.05 * capacityPenalty;
}

void
PSO::NormalizeAllocation(
    std::vector<double>& allocation,
    double capacity) const
{
    for (double& value : allocation)
    {
        value = std::max(0.0, value);
    }

    double total = 0.0;

    for (double value : allocation)
    {
        total += value;
    }

    if (total > capacity &&
        total > 0.0)
    {
        double scale =
            capacity / total;

        for (double& value : allocation)
        {
            value *= scale;
        }
    }
}

std::vector<double>
PSO::GenerateRandomAllocation(
    const std::vector<double>& demand,
    double capacity)
{
    std::vector<double> allocation(
        demand.size(),
        0.0);

    std::uniform_real_distribution<double>
        random01(0.0, 1.0);

    for (uint32_t i = 0;
         i < demand.size();
         ++i)
    {
        allocation[i] =
            demand[i] *
            random01(m_generator);
    }

    NormalizeAllocation(
        allocation,
        capacity);

    return allocation;
}

std::vector<double>
PSO::Optimize(
    const std::vector<double>& demand,
    double capacity)
{
    if (demand.empty())
    {
        m_bestFitness = 0.0;
        return {};
    }

    double totalDemand = 0.0;

    for (double value : demand)
    {
        totalDemand += value;
    }

    if (totalDemand <= capacity)
    {
        m_bestFitness =
            CalculateFitness(
                demand,
                demand,
                capacity);

        return demand;
    }

    std::vector<std::vector<double>> particles;

    for (uint32_t i = 0;
         i < m_populationSize;
         ++i)
    {
        particles.push_back(
            GenerateRandomAllocation(
                demand,
                capacity));
    }

    std::vector<std::vector<double>> velocity(
        m_populationSize,
        std::vector<double>(
            demand.size(),
            0.0));

    std::vector<std::vector<double>> personalBest =
        particles;

    std::vector<double> personalBestFitness(
        m_populationSize,
        std::numeric_limits<double>::max());

    std::vector<double> globalBest;

    double globalBestFitness =
        std::numeric_limits<double>::max();

    std::uniform_real_distribution<double>
        random01(0.0, 1.0);

    for (uint32_t i = 0;
         i < m_populationSize;
         ++i)
    {
        double fitness =
            CalculateFitness(
                particles[i],
                demand,
                capacity);

        personalBestFitness[i] =
            fitness;

        if (fitness < globalBestFitness)
        {
            globalBestFitness = fitness;
            globalBest = particles[i];
        }
    }

    for (uint32_t iteration = 0;
         iteration < m_iterations;
         ++iteration)
    {
        for (uint32_t i = 0;
             i < m_populationSize;
             ++i)
        {
            for (uint32_t j = 0;
                 j < demand.size();
                 ++j)
            {
                double r1 =
                    random01(m_generator);

                double r2 =
                    random01(m_generator);

                /*
                 * PSO velocity equation:
                 *
                 * v =
                 * w*v
                 * + c1*r1*(pBest-x)
                 * + c2*r2*(gBest-x)
                 */
                velocity[i][j] =
                    m_inertiaWeight *
                        velocity[i][j]

                    + m_cognitiveCoefficient *
                        r1 *
                        (personalBest[i][j] -
                         particles[i][j])

                    + m_socialCoefficient *
                        r2 *
                        (globalBest[j] -
                         particles[i][j]);

                /*
                 * Position update.
                 */
                particles[i][j] +=
                    velocity[i][j];

                particles[i][j] =
                    std::max(
                        0.0,
                        particles[i][j]);
            }

            NormalizeAllocation(
                particles[i],
                capacity);

            double fitness =
                CalculateFitness(
                    particles[i],
                    demand,
                    capacity);

            if (fitness <
                personalBestFitness[i])
            {
                personalBestFitness[i] =
                    fitness;

                personalBest[i] =
                    particles[i];
            }

            if (fitness <
                globalBestFitness)
            {
                globalBestFitness =
                    fitness;

                globalBest =
                    particles[i];
            }
        }
    }

    m_bestFitness =
        globalBestFitness;

    return globalBest;
}

std::vector<double>
PSO::OptimizeFromPopulation(
    const std::vector<std::vector<double>>& initialPopulation,
    const std::vector<double>& demand,
    double capacity)
{
    if (initialPopulation.empty())
    {
        return Optimize(
            demand,
            capacity);
    }

    double totalDemand = 0.0;

    for (double value : demand)
    {
        totalDemand += value;
    }

    if (totalDemand <= capacity)
    {
        m_bestFitness =
            CalculateFitness(
                demand,
                demand,
                capacity);

        return demand;
    }

    std::vector<std::vector<double>> particles =
        initialPopulation;

    uint32_t populationSize =
        particles.size();

    for (auto& particle : particles)
    {
        NormalizeAllocation(
            particle,
            capacity);
    }

    std::vector<std::vector<double>> velocity(
        populationSize,
        std::vector<double>(
            demand.size(),
            0.0));

    std::vector<std::vector<double>> personalBest =
        particles;

    std::vector<double> personalBestFitness(
        populationSize,
        std::numeric_limits<double>::max());

    std::vector<double> globalBest;

    double globalBestFitness =
        std::numeric_limits<double>::max();

    for (uint32_t i = 0;
         i < populationSize;
         ++i)
    {
        double fitness =
            CalculateFitness(
                particles[i],
                demand,
                capacity);

        personalBestFitness[i] =
            fitness;

        if (fitness < globalBestFitness)
        {
            globalBestFitness =
                fitness;

            globalBest =
                particles[i];
        }
    }

    std::uniform_real_distribution<double>
        random01(0.0, 1.0);

    for (uint32_t iteration = 0;
         iteration < m_iterations;
         ++iteration)
    {
        for (uint32_t i = 0;
             i < populationSize;
             ++i)
        {
            for (uint32_t j = 0;
                 j < demand.size();
                 ++j)
            {
                double r1 =
                    random01(m_generator);

                double r2 =
                    random01(m_generator);

                velocity[i][j] =
                    m_inertiaWeight *
                        velocity[i][j]

                    + m_cognitiveCoefficient *
                        r1 *
                        (personalBest[i][j] -
                         particles[i][j])

                    + m_socialCoefficient *
                        r2 *
                        (globalBest[j] -
                         particles[i][j]);

                particles[i][j] +=
                    velocity[i][j];

                particles[i][j] =
                    std::max(
                        0.0,
                        particles[i][j]);
            }

            NormalizeAllocation(
                particles[i],
                capacity);

            double fitness =
                CalculateFitness(
                    particles[i],
                    demand,
                    capacity);

            if (fitness <
                personalBestFitness[i])
            {
                personalBestFitness[i] =
                    fitness;

                personalBest[i] =
                    particles[i];
            }

            if (fitness <
                globalBestFitness)
            {
                globalBestFitness =
                    fitness;

                globalBest =
                    particles[i];
            }
        }
    }

    m_bestFitness =
        globalBestFitness;

    return globalBest;
}

double
PSO::GetBestFitness() const
{
    return m_bestFitness;
}