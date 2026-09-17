#include "eho.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

EHO::EHO(uint32_t populationSize,
         uint32_t numberOfClans,
         uint32_t iterations,
         double alpha,
         double beta,
         uint32_t seed)
    : m_populationSize(populationSize),
      m_numberOfClans(numberOfClans),
      m_iterations(iterations),
      m_alpha(alpha),
      m_beta(beta),
      m_seed(seed),
      m_generator(seed),
      m_bestFitness(std::numeric_limits<double>::max())
{
}

double
EHO::CalculateFitness(
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

    for (uint32_t i = 0; i < demand.size(); ++i)
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

    /*
     * Lower fitness is better.
     */
    double fitness =
        0.65 * unmetDemandRatio +
        0.20 * (1.0 - fairness) +
        0.10 * utilizationPenalty +
        0.05 * capacityPenalty;

    return fitness;
}

void
EHO::NormalizeAllocation(
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
EHO::GenerateRandomAllocation(
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
EHO::Optimize(
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

    /*
     * If the bottleneck can satisfy everybody,
     * allocate the complete demand.
     */
    if (totalDemand <= capacity)
    {
        m_bestFitness =
            CalculateFitness(
                demand,
                demand,
                capacity);

        return demand;
    }

    /*
     * -----------------------------------------
     * INITIAL POPULATION
     * -----------------------------------------
     */

    std::vector<std::vector<double>> population;

    for (uint32_t i = 0;
         i < m_populationSize;
         ++i)
    {
        population.push_back(
            GenerateRandomAllocation(
                demand,
                capacity));
    }

    std::vector<double> globalBest;
    double globalBestFitness =
        std::numeric_limits<double>::max();

    /*
     * -----------------------------------------
     * EHO ITERATIONS
     * -----------------------------------------
     */

    for (uint32_t iteration = 0;
         iteration < m_iterations;
         ++iteration)
    {
        /*
         * Find global best.
         */
        for (uint32_t i = 0;
             i < population.size();
             ++i)
        {
            double fitness =
                CalculateFitness(
                    population[i],
                    demand,
                    capacity);

            if (fitness < globalBestFitness)
            {
                globalBestFitness = fitness;
                globalBest = population[i];
            }
        }

        /*
         * Number of elephants in each clan.
         */
        uint32_t clanCount =
            std::max<uint32_t>(1, m_numberOfClans);

        uint32_t clanSize =
            population.size() / clanCount;

        if (clanSize == 0)
        {
            clanSize = 1;
        }

        /*
         * Clan updating operator.
         */
        for (uint32_t clan = 0;
             clan < clanCount;
             ++clan)
        {
            uint32_t start =
                clan * clanSize;

            uint32_t end =
                std::min<uint32_t>(
                    population.size(),
                    start + clanSize);

            if (start >= end)
            {
                continue;
            }

            /*
             * Find clan leader.
             */
            uint32_t bestIndex = start;

            double bestFitness =
                CalculateFitness(
                    population[start],
                    demand,
                    capacity);

            for (uint32_t i = start + 1;
                 i < end;
                 ++i)
            {
                double fitness =
                    CalculateFitness(
                        population[i],
                        demand,
                        capacity);

                if (fitness < bestFitness)
                {
                    bestFitness = fitness;
                    bestIndex = i;
                }
            }

            std::vector<double> leader =
                population[bestIndex];

            std::uniform_real_distribution<double>
                random01(0.0, 1.0);

            /*
             * Move elephants toward
             * their clan leader.
             */
            for (uint32_t i = start;
                 i < end;
                 ++i)
            {
                if (i == bestIndex)
                {
                    continue;
                }

                for (uint32_t j = 0;
                     j < demand.size();
                     ++j)
                {
                    double r =
                        random01(m_generator);

                    population[i][j] +=
                        m_alpha *
                        r *
                        (leader[j] -
                         population[i][j]);
                }

                /*
                 * Global-best influence.
                 */
                for (uint32_t j = 0;
                     j < demand.size();
                     ++j)
                {
                    double r =
                        random01(m_generator);

                    population[i][j] +=
                        m_beta *
                        r *
                        (globalBest[j] -
                         population[i][j]);
                }

                NormalizeAllocation(
                    population[i],
                    capacity);
            }
        }

        /*
         * -------------------------------------
         * SEPARATING OPERATOR
         * -------------------------------------
         *
         * Replace the worst elephant.
         */

        uint32_t worstIndex = 0;

        double worstFitness =
            CalculateFitness(
                population[0],
                demand,
                capacity);

        for (uint32_t i = 1;
             i < population.size();
             ++i)
        {
            double fitness =
                CalculateFitness(
                    population[i],
                    demand,
                    capacity);

            if (fitness > worstFitness)
            {
                worstFitness = fitness;
                worstIndex = i;
            }
        }

        population[worstIndex] =
            GenerateRandomAllocation(
                demand,
                capacity);
    }

    /*
     * Final global-best search.
     */
    for (const auto& elephant : population)
    {
        double fitness =
            CalculateFitness(
                elephant,
                demand,
                capacity);

        if (fitness < globalBestFitness)
        {
            globalBestFitness = fitness;
            globalBest = elephant;
        }
    }

    m_bestFitness =
        globalBestFitness;

    return globalBest;
}

double
EHO::GetBestFitness() const
{
    return m_bestFitness;
}