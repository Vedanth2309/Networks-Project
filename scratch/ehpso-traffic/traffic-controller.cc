#include "traffic-controller.h"
#include "ehpso.h"
#include "traffic-application.h"
#include "traffic-monitor.h"
#include "ns3/core-module.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <cmath>

using namespace ns3;

namespace {
std::string Quote(const std::string& value) { return "\"" + value + "\""; }
std::vector<std::string> SplitCsv(const std::string& line) {
    std::vector<std::string> out; std::stringstream ss(line); std::string part;
    while (std::getline(ss, part, ',')) out.push_back(part);
    return out;
}
}

TrafficController::TrafficController(
    const std::vector<Ptr<TrafficApplication>>& applications,
    const std::vector<double>& priorities, const std::vector<double>& deadlinesMs,
    const std::vector<std::string>& deviceIds,
    const std::vector<std::string>& categories, TrafficMonitor* monitor, EHPSO* optimizer,
    double bottleneckCapacity, double controlInterval, double profileInterval,
    double simulationTime, bool useEho, bool useMl, const std::string& projectRoot)
    : m_applications(applications), m_priorities(priorities), m_deadlinesMs(deadlinesMs), m_deviceIds(deviceIds),
      m_categories(categories), m_monitor(monitor), m_optimizer(optimizer),
      m_bottleneckCapacity(bottleneckCapacity), m_controlInterval(controlInterval),
      m_profileInterval(profileInterval), m_simulationTime(simulationTime),
      m_useEho(useEho), m_useMl(useMl), m_warnedMlFallback(false),
      m_projectRoot(projectRoot), m_historyPath(projectRoot + "/data/traffic_history.csv"),
      m_predictionPath(projectRoot + "/data/traffic_predictions.csv"),
      m_modelPath(projectRoot + "/ml/traffic_model.joblib"),
      m_evaluationPath(projectRoot + "/data/prediction_evaluation.csv"),
      m_allocationEvaluationPath(projectRoot + "/data/allocation_evaluation.csv"),
      m_pendingPredictionTime(0.0),
      m_hasPendingPrediction(false)
{
    {
        std::ofstream allocationLog(m_allocationEvaluationPath, std::ios::trunc);
        if (allocationLog) {
            allocationLog
                << "time_s,device_id,category,priority,deadline_ms,"
                << "configured_demand_mbps,optimizer_input_demand_mbps,"
                << "proposed_allocation_mbps,applied_rate_mbps,ml_active\n";
        } else {
            std::cerr << "[Allocation Evaluation] Could not initialize "
                      << m_allocationEvaluationPath << "\n";
        }
    }

    if (m_useMl) {
        std::ofstream history(m_historyPath, std::ios::trunc);
        if (history) history << "time_s,device_id,category,priority,demand_mbps\n";

        std::ofstream evaluation(m_evaluationPath, std::ios::trunc);
        if (evaluation) {
            evaluation
                << "forecast_origin_time_s,target_time_s,device_id,category,priority,"
                << "predicted_demand_mbps,baseline_prediction_mbps,"
                << "actual_demand_mbps,absolute_error_mbps,squared_error_mbps,"
                << "baseline_absolute_error_mbps,baseline_squared_error_mbps\n";
        }
    }
}

void TrafficController::Start() {
    m_controllerEvent = Simulator::Schedule(Seconds(m_controlInterval), &TrafficController::RunController, this);
}

void TrafficController::ApplyInitialAllocation()
{
    std::vector<double> initialDemand;
    initialDemand.reserve(m_applications.size());

    for (const auto& app : m_applications)
    {
        initialDemand.push_back(app->GetDemand());
    }

    std::vector<double> allocation;

    if (m_useEho && m_optimizer)
    {
        allocation = m_optimizer->Optimize(
            initialDemand,
            m_priorities,
            m_deadlinesMs,
            m_bottleneckCapacity);
    }
    else
    {
        allocation = ReservedAllocation(initialDemand);
    }

    for (size_t i = 0; i < m_applications.size(); ++i)
    {
        m_applications[i]->SetAllocatedRate(
            std::min(initialDemand[i], allocation[i]));
    }

    std::cout << "[Controller] Initial allocation applied at t=0s.\n";
}

bool TrafficController::AppendHistory(double t, const std::vector<double>& demand) {
    std::ofstream out(m_historyPath, std::ios::app);
    if (!out) return false;
    out << std::fixed << std::setprecision(6);
    for (size_t i = 0; i < demand.size(); ++i)
        out << t << ',' << m_deviceIds[i] << ',' << m_categories[i] << ',' << m_priorities[i] << ',' << demand[i] << '\n';
    return static_cast<bool>(out);
}

bool TrafficController::RunPrediction(std::vector<double>& predictedDemand) {
    std::ifstream model(m_modelPath);
    if (!model) return false;
    std::string script = m_projectRoot + "/ml/predict_traffic.py";
    std::string devices = m_projectRoot + "/config/hospital_devices.csv";
    std::string python = m_projectRoot + "/.venv/bin/python";
    std::string command = Quote(python) + " " + Quote(script) + " --history " + Quote(m_historyPath) +
        " --model " + Quote(m_modelPath) + " --output " + Quote(m_predictionPath) +
        " --devices " + Quote(devices) + " --interval " + std::to_string(m_controlInterval) +
        " > /dev/null 2>&1";
    if (std::system(command.c_str()) != 0) return false;
    std::ifstream in(m_predictionPath);
    std::string line;
    if (!std::getline(in, line)) return false;
    std::map<std::string, double> predictions;
    while (std::getline(in, line)) {
        auto fields = SplitCsv(line);
        if (fields.size() < 6) continue;
        try { predictions[fields[0]] = std::stod(fields[5]); } catch (...) {}
    }
    if (predictions.size() < m_deviceIds.size()) return false;
    for (size_t i = 0; i < m_deviceIds.size(); ++i) {
        auto it = predictions.find(m_deviceIds[i]);
        if (it == predictions.end()) return false;
        predictedDemand[i] = std::max(0.0, it->second);
    }
    return true;
}


std::vector<double> TrafficController::WeightedAllocation(
    const std::vector<double>& demand) const
{
    const size_t n = demand.size();
    std::vector<double> allocation(n, 0.0);
    std::vector<bool> active(n, true);

    double remaining = m_bottleneckCapacity;

    // Weighted max-min allocation:
    // higher-priority devices receive a larger share,
    // but no device receives more than its predicted demand.
    while (remaining > 1e-9) {
        double totalWeight = 0.0;

        for (size_t i = 0; i < n; ++i) {
            if (active[i] &&
                allocation[i] + 1e-9 < std::max(0.0, demand[i])) {
                totalWeight += std::max(0.0, m_priorities[i]);
            } else {
                active[i] = false;
            }
        }

        if (totalWeight <= 1e-12) {
            break;
        }

        double used = 0.0;

        for (size_t i = 0; i < n; ++i) {
            if (!active[i]) {
                continue;
            }

            const double weight = std::max(0.0, m_priorities[i]);
            const double share = remaining * weight / totalWeight;
            const double unmet = std::max(0.0, demand[i]) - allocation[i];
            const double grant = std::min(unmet, share);

            allocation[i] += grant;
            used += grant;

            if (allocation[i] + 1e-9 >= std::max(0.0, demand[i])) {
                active[i] = false;
            }
        }

        if (used <= 1e-12) {
            break;
        }

        remaining -= used;
    }

    return allocation;
}


std::vector<double> TrafficController::ReservedAllocation(
    const std::vector<double>& demand) const
{
    const size_t n = demand.size();
    std::vector<double> allocation(n, 0.0);

    // Initial experimental policy: 70% for critical/control,
    // 30% for diagnostics, imaging and routine IoT.
    const double criticalBudget = 0.70 * m_bottleneckCapacity;
    const double otherBudget = m_bottleneckCapacity - criticalBudget;

    std::vector<size_t> critical;
    std::vector<size_t> other;

    for (size_t i = 0; i < n; ++i) {
        if (m_categories[i] == "critical_monitoring" ||
            m_categories[i] == "time_sensitive_control") {
            critical.push_back(i);
        } else {
            other.push_back(i);
        }
    }

    auto allocateGroup = [&](const std::vector<size_t>& group,
                             double budget) {
        std::vector<bool> active(n, false);

        for (size_t i : group) {
            active[i] = demand[i] > 0.0;
        }

        double remaining = budget;

        while (remaining > 1e-9) {
            double totalWeight = 0.0;

            for (size_t i : group) {
                if (active[i] &&
                    allocation[i] + 1e-9 < std::max(0.0, demand[i])) {
                    totalWeight += std::max(0.0, m_priorities[i]);
                } else {
                    active[i] = false;
                }
            }

            if (totalWeight <= 1e-12) {
                break;
            }

            double used = 0.0;

            for (size_t i : group) {
                if (!active[i]) {
                    continue;
                }

                double weight = std::max(0.0, m_priorities[i]);
                double share = remaining * weight / totalWeight;
                double unmet = std::max(0.0, demand[i]) - allocation[i];
                double grant = std::min(unmet, share);

                allocation[i] += grant;
                used += grant;

                if (allocation[i] + 1e-9 >= std::max(0.0, demand[i])) {
                    active[i] = false;
                }
            }

            if (used <= 1e-12) {
                break;
            }

            remaining -= used;
        }

        return remaining;
    };

    // First distribute each group's initial budget.
    double unusedCritical = allocateGroup(critical, criticalBudget);
    double unusedOther = allocateGroup(other, otherBudget);

    // Reallocate unused capacity to whichever group still has demand.
    std::vector<size_t> all(n);
    for (size_t i = 0; i < n; ++i) {
        all[i] = i;
    }

    allocateGroup(all, unusedCritical + unusedOther);

    return allocation;
}

bool TrafficController::AppendPredictionEvaluation(
    double actualTime,
    const std::vector<double>& actualDemand)
{
    if (!m_hasPendingPrediction ||
        actualDemand.size() != m_pendingPredictions.size() ||
        actualDemand.size() != m_deviceIds.size())
    {
        return false;
    }

    // Only compare forecasts against their intended target interval.
    const double tolerance = std::max(1e-6, m_controlInterval * 0.1);
    if (std::abs(actualTime - m_pendingPredictionTime) > tolerance) {
        return false;
    }

    std::ofstream out(m_evaluationPath, std::ios::app);
    if (!out) {
        return false;
    }

    out << std::fixed << std::setprecision(6);

    for (size_t i = 0; i < actualDemand.size(); ++i) {
        const double predicted = m_pendingPredictions[i];
        const double baseline = m_pendingBaseline[i];
        const double actual = actualDemand[i];

        const double error = predicted - actual;
        const double baselineError = baseline - actual;

        out << (m_pendingPredictionTime - m_controlInterval) << ','
            << actualTime << ','
            << m_deviceIds[i] << ','
            << m_categories[i] << ','
            << m_priorities[i] << ','
            << predicted << ','
            << baseline << ','
            << actual << ','
            << std::abs(error) << ','
            << error * error << ','
            << std::abs(baselineError) << ','
            << baselineError * baselineError << '\n';
    }

    m_hasPendingPrediction = false;
    return static_cast<bool>(out);
}

void TrafficController::RunController() {
    const double now = Simulator::Now().GetSeconds();
    std::vector<double> actualDemand, optimizationDemand, predicted;
    actualDemand.reserve(m_applications.size());
    for (const auto& app : m_applications) actualDemand.push_back(app->GetDemand());
    optimizationDemand = actualDemand;

    bool mlActive = false;
    if (m_useMl) {
        AppendPredictionEvaluation(now, actualDemand);
        AppendHistory(now, actualDemand);
        predicted = actualDemand;
        mlActive = RunPrediction(predicted);

        if (mlActive) {
            optimizationDemand = predicted;
            m_pendingPredictions = predicted;
            m_pendingBaseline = actualDemand;
            m_pendingPredictionTime = now + m_controlInterval;
            m_hasPendingPrediction = true;

            std::cout << "[ML DEBUG] Predicted demand: ";
            for (size_t i = 0; i < predicted.size(); ++i) {
                std::cout << m_deviceIds[i] << "="
                        << predicted[i] << " Mbps";
                if (i + 1 < predicted.size()) std::cout << ", ";
            }
            std::cout << "\n";
        }
        else if (!m_warnedMlFallback) {
            std::cout << "[ML] Model/prediction unavailable; using current measured demand until a valid model is available.\n";
            m_warnedMlFallback = true;
        }
    }

    std::vector<double> allocation;
    if (m_useEho && m_optimizer) {
        allocation = m_optimizer->Optimize(
            optimizationDemand, m_priorities, m_deadlinesMs, m_bottleneckCapacity);
    } else {
        allocation = ReservedAllocation(optimizationDemand);
    }

    std::vector<double> appliedRates;
    appliedRates.reserve(m_applications.size());

    for (size_t i = 0; i < m_applications.size(); ++i) {
        const double appliedRate =
            std::min(actualDemand[i], allocation[i]);

        m_applications[i]->SetAllocatedRate(appliedRate);
        appliedRates.push_back(appliedRate);
    }

    // Record the demand used by the optimizer and the resulting allocation.
    {
        std::ofstream out(m_allocationEvaluationPath, std::ios::app);

        if (out) {
            out << std::fixed << std::setprecision(6);

            for (size_t i = 0; i < m_applications.size(); ++i) {
                out << now << ','
                    << m_deviceIds[i] << ','
                    << m_categories[i] << ','
                    << m_priorities[i] << ','
                    << m_deadlinesMs[i] << ','
                    << actualDemand[i] << ','
                    << optimizationDemand[i] << ','
                    << allocation[i] << ','
                    << appliedRates[i] << ','
                    << (mlActive ? 1 : 0) << '\n';
            }
        } else {
            std::cerr << "[Allocation Evaluation] Could not open "
                      << m_allocationEvaluationPath << "\n";
        }
    }

    std::cout << std::fixed << std::setprecision(3)
              << "[t=" << now << "s] "
              << (m_useEho ? "EHPSO" : "Priority allocator")
              << (mlActive ? "+RandomForest" : (m_useMl ? "+ML-fallback" : "")) << " allocation: ";
    for (size_t i = 0; i < allocation.size(); ++i) {
        std::cout << m_deviceIds[i] << "(" << m_categories[i] << ",p=" << m_priorities[i]
                  << ",deadline=" << m_deadlinesMs[i] << "ms)="
                  << std::min(actualDemand[i], allocation[i]) << " Mbps";
        if (i + 1 < allocation.size()) std::cout << ", ";
    }
    std::cout << "\n";
    if (m_monitor) m_monitor->PrintCurrentState(now);
    const double next = now + m_controlInterval;
    if (next < m_simulationTime)
        m_controllerEvent = Simulator::Schedule(Seconds(m_controlInterval), &TrafficController::RunController, this);
}
