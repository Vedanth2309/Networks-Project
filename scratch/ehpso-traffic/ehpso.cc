#include "ehpso.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
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

// Priority-aware overload retained for existing callers. If no per-device
// deadlines are supplied, use a common deadline so priority remains decisive.
std::vector<double>
EHPSO::Optimize(const std::vector<double>& demand,
                const std::vector<double>& priorities,
                double capacity)
{
    return Optimize(demand, priorities,
                   std::vector<double>(demand.size(), 1000.0), capacity);
}

// Deadline-aware EHPSO allocation.
// EHO+PSO first computes a candidate. The final feasibility stage then allocates
// bandwidth in deadline tiers: the shortest-deadline tier is served first,
// and a less urgent tier receives capacity only after the earlier tier's demand
// has been satisfied. Within a tier, priority and the EHPSO candidate influence
// weighted max-min sharing. This prevents long-deadline traffic from consuming
// capacity while urgent traffic remains unsatisfied.
std::vector<double>
EHPSO::Optimize(const std::vector<double>& demand,
                const std::vector<double>& priorities,
                const std::vector<double>& deadlinesMs,
                double capacity)
{
    if (demand.size() != priorities.size() || demand.size() != deadlinesMs.size())
        throw std::invalid_argument("Demand, priorities and deadlines must have equal sizes.");
    if (!std::isfinite(capacity) || capacity < 0.0)
        throw std::invalid_argument("Capacity must be finite and non-negative.");

    double totalDemand = 0.0;
    for (size_t i = 0; i < demand.size(); ++i)
    {
        if (!std::isfinite(demand[i]) || demand[i] < 0.0)
            throw std::invalid_argument("Demand must be finite and non-negative.");
        if (!std::isfinite(priorities[i]) || priorities[i] <= 0.0)
            throw std::invalid_argument("Priorities must be finite and positive.");
        if (!std::isfinite(deadlinesMs[i]) || deadlinesMs[i] <= 0.0)
            throw std::invalid_argument("Deadlines must be finite and positive.");
        totalDemand += demand[i];
    }

    if (demand.empty())
    {
        m_ehoAllocation.clear();
        m_finalAllocation.clear();
        m_ehoFitness = m_psoFitness = m_bestFitness = 0.0;
        return {};
    }

    if (totalDemand <= capacity)
    {
        m_ehoAllocation = demand;
        m_finalAllocation = demand;
        m_ehoFitness = m_psoFitness = m_bestFitness = 0.0;
        return demand;
    }

    // Keep the genuine EHPSO search in the loop. Its candidate affects sharing
    // among devices in the same deadline tier, but cannot let lower-urgency
    // traffic jump ahead of unsatisfied tighter-deadline traffic.
    const std::vector<double> optimizerAllocation = Optimize(demand, capacity);

    std::vector<double> weights(demand.size(), 0.0);
    for (size_t i = 0; i < demand.size(); ++i)
    {
        const double candidateRatio = demand[i] > 1e-12
            ? std::clamp(optimizerAllocation[i] / demand[i], 0.0, 1.0)
            : 0.0;
        weights[i] = priorities[i] * (1.0 + 0.5 * candidateRatio);
    }

    std::vector<size_t> order(demand.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (deadlinesMs[a] != deadlinesMs[b]) return deadlinesMs[a] < deadlinesMs[b];
        if (priorities[a] != priorities[b]) return priorities[a] > priorities[b];
        return a < b;
    });

    std::vector<double> result(demand.size(), 0.0);
    double remaining = capacity;
    size_t begin = 0;
    constexpr double kEpsilon = 1e-9;

    while (begin < order.size() && remaining > kEpsilon)
    {
        // Treat equal configured deadlines as one urgency tier.
        size_t end = begin + 1;
        while (end < order.size() &&
               std::abs(deadlinesMs[order[end]] - deadlinesMs[order[begin]]) <= 1e-6)
            ++end;

        double tierDemand = 0.0;
        for (size_t pos = begin; pos < end; ++pos)
            tierDemand += demand[order[pos]];

        if (tierDemand <= remaining + kEpsilon)
        {
            for (size_t pos = begin; pos < end; ++pos)
                result[order[pos]] = demand[order[pos]];
            remaining = std::max(0.0, remaining - tierDemand);
            begin = end;
            continue;
        }

        // This tier cannot be fully served. Weighted max-min allocation within
        // this tier uses all remaining capacity, subject to each demand cap.
        std::vector<bool> active(demand.size(), false);
        for (size_t pos = begin; pos < end; ++pos)
            active[order[pos]] = demand[order[pos]] > 0.0;

        double tierRemaining = remaining;
        for (size_t iteration = 0; iteration <= end - begin && tierRemaining > kEpsilon; ++iteration)
        {
            double totalWeight = 0.0;
            for (size_t pos = begin; pos < end; ++pos)
            {
                const size_t i = order[pos];
                if (active[i] && result[i] + kEpsilon < demand[i]) totalWeight += weights[i];
                else active[i] = false;
            }
            if (totalWeight <= kEpsilon) break;

            double used = 0.0;
            for (size_t pos = begin; pos < end; ++pos)
            {
                const size_t i = order[pos];
                if (!active[i]) continue;
                const double unmet = demand[i] - result[i];
                const double grant = std::min(unmet, tierRemaining * weights[i] / totalWeight);
                result[i] += grant;
                used += grant;
                if (result[i] + kEpsilon >= demand[i]) active[i] = false;
            }
            if (used <= kEpsilon) break;
            tierRemaining = std::max(0.0, tierRemaining - used);
        }
        remaining = tierRemaining;
        // Capacity was insufficient for this tier: no less-urgent tier gets any.
        break;
    }

    double allocatedTotal = 0.0;
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = std::clamp(result[i], 0.0, demand[i]);
        allocatedTotal += result[i];
    }
    if (allocatedTotal > capacity + kEpsilon && allocatedTotal > 0.0)
    {
        const double scale = capacity / allocatedTotal;
        for (double& value : result) value *= scale;
    }

    m_finalAllocation = result;
    return m_finalAllocation;
}
