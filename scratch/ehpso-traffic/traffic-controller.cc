#include "traffic-controller.h"

#include "ehpso.h"
#include "traffic-application.h"
#include "traffic-monitor.h"

#include "ns3/core-module.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ns3;

// =============================================================
// CONSTRUCTOR
// =============================================================

TrafficController::TrafficController(
    const std::vector<Ptr<TrafficApplication>>& applications,
    TrafficMonitor* monitor,
    EHPSO* optimizer,
    double bottleneckCapacity,
    double controlInterval,
    double profileInterval,
    double simulationTime)

    : m_applications(applications),
      m_monitor(monitor),
      m_optimizer(optimizer),
      m_bottleneckCapacity(bottleneckCapacity),
      m_controlInterval(controlInterval),
      m_profileInterval(profileInterval),
      m_simulationTime(simulationTime)
{
}

// =============================================================
// START CONTROLLER
// =============================================================

void
TrafficController::Start()
{
    m_controllerEvent =
        Simulator::Schedule(
            Seconds(m_controlInterval),
            &TrafficController::RunController,
            this);
}

// =============================================================
// CONTROLLER
// =============================================================

void
TrafficController::RunController()
{
    double currentTime =
        Simulator::Now().GetSeconds();

    // =========================================================
    // STEP 1:
    // Get current traffic demand from every IoT application
    // =========================================================

    std::vector<double> demands;

    for (const auto& app : m_applications)
    {
        demands.push_back(
            app->GetDemand());
    }

    // =========================================================
    // STEP 2:
    // Run EHPSO
    // =========================================================

    std::vector<double> finalAllocation =
        m_optimizer->Optimize(
            demands,
            m_bottleneckCapacity);

    // Get EHO intermediate solution
    const std::vector<double>& ehoAllocation =
        m_optimizer->GetEhoAllocation();

    // Get final EHPSO solution
    const std::vector<double>& storedFinalAllocation =
        m_optimizer->GetFinalAllocation();

    // =========================================================
    // STEP 3:
    // Apply final allocation to traffic applications
    // =========================================================

    for (uint32_t i = 0;
         i < m_applications.size();
         ++i)
    {
        m_applications[i]->SetAllocatedRate(
            finalAllocation[i]);
    }

    // =========================================================
    // DETAILED OUTPUT ONLY AT FIRST CONTROL POINT
    // =========================================================

    if (std::abs(currentTime - 1.0) < 1e-6)
    {
        std::cout << "\n";
        std::cout << "============================================================\n";
        std::cout << "              EHPSO BANDWIDTH OPTIMIZATION\n";
        std::cout << "                     t = 1.0 seconds\n";
        std::cout << "============================================================\n";

        std::cout << std::fixed
                  << std::setprecision(3);

        // -----------------------------------------------------
        // Total demand
        // -----------------------------------------------------

        double totalDemand = 0.0;
        double totalEho = 0.0;
        double totalFinal = 0.0;

        for (double value : demands)
        {
            totalDemand += value;
        }

        for (double value : ehoAllocation)
        {
            totalEho += value;
        }

        for (double value : storedFinalAllocation)
        {
            totalFinal += value;
        }

        std::cout << "\n";
        std::cout << "Bottleneck Capacity : "
                  << m_bottleneckCapacity
                  << " Mbps\n";

        std::cout << "Total Traffic Demand : "
                  << totalDemand
                  << " Mbps\n";

        // -----------------------------------------------------
        // Allocation table
        // -----------------------------------------------------

        std::cout << "\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << " Source       Initial        EHO          PSO/Final\n";
        std::cout << "              Demand       Allocation     Allocation\n";
        std::cout << "------------------------------------------------------------\n";

        for (uint32_t i = 0;
             i < demands.size();
             ++i)
        {
            std::cout
                << " IoT-" << (i + 1)
                << "       "
                << std::setw(7)
                << demands[i]
                << "       "
                << std::setw(7)
                << ehoAllocation[i]
                << "       "
                << std::setw(7)
                << storedFinalAllocation[i]
                << "\n";
        }

        std::cout << "------------------------------------------------------------\n";

        std::cout
            << " TOTAL       "
            << std::setw(7)
            << totalDemand
            << "       "
            << std::setw(7)
            << totalEho
            << "       "
            << std::setw(7)
            << totalFinal
            << "\n";

        // -----------------------------------------------------
        // Capacity information
        // -----------------------------------------------------

        std::cout << "\n";
        std::cout << "Capacity Check:\n";

        std::cout << "Initial Demand : "
                  << totalDemand
                  << " Mbps\n";

        std::cout << "EHO Allocation : "
                  << totalEho
                  << " Mbps\n";

        std::cout << "Final Allocation : "
                  << totalFinal
                  << " Mbps\n";

        std::cout << "Bottleneck      : "
                  << m_bottleneckCapacity
                  << " Mbps\n";

        // -----------------------------------------------------
        // Fitness information
        // -----------------------------------------------------

        std::cout << "\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "                  OPTIMIZATION FITNESS\n";
        std::cout << "------------------------------------------------------------\n";

        std::cout << "EHO Fitness     : "
                  << m_optimizer->GetEhoFitness()
                  << "\n";

        std::cout << "PSO Fitness     : "
                  << m_optimizer->GetPsoFitness()
                  << "\n";

        std::cout << "Final Fitness   : "
                  << m_optimizer->GetBestFitness()
                  << "\n";

        // -----------------------------------------------------
        // Bandwidth movement
        // -----------------------------------------------------

        std::cout << "\n";
        std::cout << "------------------------------------------------------------\n";
        std::cout << "                BANDWIDTH MOVEMENT\n";
        std::cout << "------------------------------------------------------------\n";

        for (uint32_t i = 0;
             i < demands.size();
             ++i)
        {
            double ehoChange =
                ehoAllocation[i] -
                demands[i];

            double finalChange =
                storedFinalAllocation[i] -
                ehoAllocation[i];

            std::cout
                << "IoT-" << (i + 1)
                << " : "
                << demands[i]
                << " -> "
                << ehoAllocation[i]
                << " -> "
                << storedFinalAllocation[i]
                << " Mbps";

            std::cout << "   "
                      << "(EHO: ";

            if (ehoChange >= 0)
            {
                std::cout << "+";
            }

            std::cout << ehoChange
                      << ", PSO: ";

            if (finalChange >= 0)
            {
                std::cout << "+";
            }

            std::cout << finalChange
                      << ")\n";
        }

        std::cout << "------------------------------------------------------------\n";

        std::cout << "\nFinal EHPSO allocation has been applied to "
                     "all IoT traffic sources.\n";

        std::cout << "============================================================\n";
        std::cout << "                 END OF t = 1s OUTPUT\n";
        std::cout << "============================================================\n\n";
    }

    // =========================================================
    // Later control intervals
    //
    // Keep output short after t = 1s
    // =========================================================

    else
    {
        std::cout << std::fixed
                  << std::setprecision(3);

        std::cout
            << "[t="
            << currentTime
            << "s] EHPSO allocation: ";

        for (uint32_t i = 0;
             i < finalAllocation.size();
             ++i)
        {
            std::cout
                << "IoT"
                << (i + 1)
                << "="
                << finalAllocation[i]
                << " Mbps";

            if (i + 1 < finalAllocation.size())
            {
                std::cout << ", ";
            }
        }

        std::cout << "\n";
    }

    // =========================================================
    // Monitor current state
    // =========================================================

    if (m_monitor != nullptr)
    {
        m_monitor->PrintCurrentState(
            currentTime);
    }

    // =========================================================
    // Schedule next controller execution
    // =========================================================

    double nextTime =
        currentTime +
        m_controlInterval;

    if (nextTime < m_simulationTime)
    {
        m_controllerEvent =
            Simulator::Schedule(
                Seconds(m_controlInterval),
                &TrafficController::RunController,
                this);
    }
}