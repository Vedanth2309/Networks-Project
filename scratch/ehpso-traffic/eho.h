#ifndef EHO_H
#define EHO_H

#include <cstdint>
#include <random>
#include <vector>

class EHO
{
public:
    EHO(uint32_t populationSize,
        uint32_t numberOfClans,
        uint32_t iterations,
        double alpha,
        double beta,
        uint32_t seed);

    std::vector<double> Optimize(
        const std::vector<double>& demand,
        double capacity);

    double GetBestFitness() const;

private:
    double CalculateFitness(
        const std::vector<double>& allocation,
        const std::vector<double>& demand,
        double capacity) const;

    void NormalizeAllocation(
        std::vector<double>& allocation,
        double capacity) const;

    std::vector<double> GenerateRandomAllocation(
        const std::vector<double>& demand,
        double capacity);

    uint32_t m_populationSize;
    uint32_t m_numberOfClans;
    uint32_t m_iterations;

    double m_alpha;
    double m_beta;

    uint32_t m_seed;

    std::mt19937 m_generator;

    double m_bestFitness;
};

#endif