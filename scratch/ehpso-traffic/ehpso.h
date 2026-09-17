#ifndef EHPSO_H
#define EHPSO_H

#include "eho.h"
#include "pso.h"

#include <cstdint>
#include <vector>

class EHPSO
{
public:
    EHPSO(
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

        uint32_t seed);

    std::vector<double> Optimize(
        const std::vector<double>& demand,
        double capacity);

    double GetEhoFitness() const;
    double GetPsoFitness() const;
    double GetBestFitness() const;

    // ---------------------------------------------------------
    // Intermediate and final allocations
    // ---------------------------------------------------------

    // Allocation produced by EHO
    const std::vector<double>& GetEhoAllocation() const;

    // Final allocation produced by EHPSO
    const std::vector<double>& GetFinalAllocation() const;

private:

    EHO m_eho;
    PSO m_pso;

    uint32_t m_psoPopulationSize;

    double m_ehoFitness;
    double m_psoFitness;
    double m_bestFitness;

    // ---------------------------------------------------------
    // Stored allocations for displaying optimization movement
    // ---------------------------------------------------------

    std::vector<double> m_ehoAllocation;
    std::vector<double> m_finalAllocation;

    uint32_t m_seed;
};

#endif