#include "ehpso.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

EHPSO::EHPSO(
    uint32_t ehoPopulationSize,
    uint32_t ehoNumberOfClans,
    uint32_t ehoIterations,
    double ehoAlpha,
    double ehoBeta,

    uint32_t psoPopulationSize,
    uint32_t psoIterations,
    double psoInertia,
    double psoCognitive,
    double psoSocial,

    uint32_t seed)

    : m_eho(
          ehoPopulationSize,
          ehoNumberOfClans,
          ehoIterations,
          ehoAlpha,
          ehoBeta,
          seed),

      m_pso(
          psoPopulationSize,
          psoIterations,
          psoInertia,
          psoCognitive,
          psoSocial,
          seed + 1),

      m_psoPopulationSize(psoPopulationSize),
      m_ehoFitness(0.0),
      m_psoFitness(0.0),
      m_bestFitness(0.0),
      m_seed(seed)
{
}

// =============================================================
// EHPSO OPTIMIZATION
// =============================================================

std::vector<double>
EHPSO::Optimize(
    const std::vector<double>& demand,
    double capacity)
{
    // ---------------------------------------------------------
    // Calculate total demand
    // ---------------------------------------------------------

    double totalDemand = 0.0;

    for (double d : demand)
    {
        totalDemand += d;
    }

    // ---------------------------------------------------------
    // CASE 1:
    // Traffic demand is already within bottleneck capacity.
    //
    // No optimization is required.
    // Give every source its complete demand.
    // ---------------------------------------------------------

    if (totalDemand <= capacity)
    {
        m_ehoAllocation = demand;
        m_finalAllocation = demand;

        m_ehoFitness = 0.0;
        m_psoFitness = 0.0;
        m_bestFitness = 0.0;

        return demand;
    }

    // =========================================================
    // STEP 1: EHO
    // =========================================================

    m_ehoAllocation =
        m_eho.Optimize(
            demand,
            capacity);

    m_ehoFitness =
        m_eho.GetBestFitness();

    // =========================================================
    // STEP 2: Create PSO initial population
    //
    // First particle = EHO solution.
    // Remaining particles are small variations around EHO.
    // =========================================================

    std::vector<std::vector<double>> initialPopulation;

    initialPopulation.push_back(m_ehoAllocation);

    std::mt19937 generator(m_seed + 1000);

    std::uniform_real_distribution<double> variation(-0.15, 0.15);

    for (uint32_t p = 1;
         p < m_psoPopulationSize;
         ++p)
    {
        std::vector<double> particle =
            m_ehoAllocation;

        for (uint32_t i = 0;
             i < particle.size();
             ++i)
        {
            // Apply a small variation around EHO solution
            particle[i] *= (1.0 + variation(generator));

            // Never allow negative bandwidth
            if (particle[i] < 0.0)
            {
                particle[i] = 0.0;
            }
        }

        // -----------------------------------------------------
        // Normalize particle so total allocation <= capacity
        // -----------------------------------------------------

        double particleTotal = 0.0;

        for (double value : particle)
        {
            particleTotal += value;
        }

        if (particleTotal > capacity &&
            particleTotal > 0.0)
        {
            double scale =
                capacity / particleTotal;

            for (double& value : particle)
            {
                value *= scale;
            }
        }

        initialPopulation.push_back(particle);
    }

    // =========================================================
    // STEP 3: PSO refinement
    // =========================================================

    std::vector<double> psoAllocation =
        m_pso.OptimizeFromPopulation(
            initialPopulation,
            demand,
            capacity);

    m_psoFitness =
        m_pso.GetBestFitness();

    // =========================================================
    // STEP 4: Select final EHPSO solution
    // =========================================================

    if (m_psoFitness <= m_ehoFitness)
    {
        m_finalAllocation =
            psoAllocation;

        m_bestFitness =
            m_psoFitness;
    }
    else
    {
        m_finalAllocation =
            m_ehoAllocation;

        m_bestFitness =
            m_ehoFitness;
    }

    return m_finalAllocation;
}

// =============================================================
// GET EHO FITNESS
// =============================================================

double
EHPSO::GetEhoFitness() const
{
    return m_ehoFitness;
}

// =============================================================
// GET PSO FITNESS
// =============================================================

double
EHPSO::GetPsoFitness() const
{
    return m_psoFitness;
}

// =============================================================
// GET FINAL FITNESS
// =============================================================

double
EHPSO::GetBestFitness() const
{
    return m_bestFitness;
}

// =============================================================
// GET EHO ALLOCATION
// =============================================================

const std::vector<double>&
EHPSO::GetEhoAllocation() const
{
    return m_ehoAllocation;
}

// =============================================================
// GET FINAL EHPSO ALLOCATION
// =============================================================

const std::vector<double>&
EHPSO::GetFinalAllocation() const
{
    return m_finalAllocation;
}