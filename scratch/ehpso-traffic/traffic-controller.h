#ifndef TRAFFIC_CONTROLLER_H
#define TRAFFIC_CONTROLLER_H
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include <string>
#include <vector>

class TrafficApplication;
class TrafficMonitor;
class EHPSO;

class TrafficController {
public:
    TrafficController(const std::vector<ns3::Ptr<TrafficApplication>>& applications,
        const std::vector<double>& priorities, const std::vector<double>& deadlinesMs,
        const std::vector<std::string>& deviceIds,
        const std::vector<std::string>& categories, TrafficMonitor* monitor, EHPSO* optimizer,
        double bottleneckCapacity, double controlInterval, double profileInterval,
        double simulationTime, bool useEho, bool useMl, const std::string& projectRoot);
    void Start();
    void ApplyInitialAllocation();
private:
    void RunController();
    bool AppendHistory(double currentTime, const std::vector<double>& actualDemand);
    bool RunPrediction(std::vector<double>& predictedDemand);
    std::vector<double> WeightedAllocation(const std::vector<double>& demand) const;
    std::vector<double> ReservedAllocation(const std::vector<double>& demand) const;
    const std::vector<ns3::Ptr<TrafficApplication>>& m_applications;
    const std::vector<double>& m_priorities;
    const std::vector<double>& m_deadlinesMs;
    const std::vector<std::string>& m_deviceIds;
    const std::vector<std::string>& m_categories;
    TrafficMonitor* m_monitor;
    EHPSO* m_optimizer;
    double m_bottleneckCapacity, m_controlInterval, m_profileInterval, m_simulationTime;
    bool m_useEho, m_useMl, m_warnedMlFallback;
    std::string m_projectRoot, m_historyPath, m_predictionPath, m_modelPath;
    std::string m_allocationEvaluationPath;
    ns3::EventId m_controllerEvent;

    bool AppendPredictionEvaluation(
        double actualTime,
        const std::vector<double>& actualDemand);

    std::string m_evaluationPath;

    std::vector<double> m_pendingPredictions;
    std::vector<double> m_pendingBaseline;

    double m_pendingPredictionTime;
    bool m_hasPendingPrediction;
};
#endif
