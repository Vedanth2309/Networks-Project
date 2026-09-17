#ifndef PSO_H
#define PSO_H

#include <cstdint>
#include <random>
#include <vector>

class PSO
{
public:
    PSO(uint32_t populationSize,
        uint32_t iterations,
        double inertiaWeight,
        double cognitiveCoefficient,
        double socialCoefficient,
        uint32_t seed);

    std::vector<double> Optimize(
        const std::vector<double>& demand,
        double capacity);

    std::vector<double> OptimizeFromPopulation(
        const std::vector<std::vector<double>>& initialPopulation,
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
    uint32_t m_iterations;

    double m_inertiaWeight;
    double m_cognitiveCoefficient;
    double m_socialCoefficient;

    uint32_t m_seed;

    std::mt19937 m_generator;

    double m_bestFitness;
};

#endif