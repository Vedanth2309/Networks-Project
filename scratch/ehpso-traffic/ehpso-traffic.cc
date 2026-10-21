#include "ehpso.h"
#include "network-scenario.h"
#include "traffic-application.h"
#include "traffic-controller.h"
#include "traffic-generator.h"
#include "traffic-monitor.h"
#include "traffic-receiver.h"
#include "hospital-config.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ns3;

static void UpdateTrafficDemand(Ptr<TrafficApplication> app, double demandMbps, bool controlled) {
    app->SetDemand(demandMbps);
    // In baseline mode, no controller throttles sources: offer the full current demand.
    if (!controlled) app->SetAllocatedRate(demandMbps);
}

int main(int argc, char* argv[]) {
    double bottleneck1RateMbps = 5.0, bottleneck2RateMbps = 3.0;
    double simulationTime = 20.0, profileInterval = 1.0, controlInterval = 1.0;
    uint32_t seed = 12345;
    bool useEho = true, useMl = false;
    std::string projectRoot = "scratch/ehpso-traffic";
    std::string deviceConfigPath = projectRoot + "/config/hospital_devices.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("devices", "Hospital device configuration CSV", deviceConfigPath);
    cmd.AddValue("projectRoot", "Project directory relative to ns-3 root", projectRoot);
    cmd.AddValue("bottleneck1", "R1-to-R2 capacity in Mbps", bottleneck1RateMbps);
    cmd.AddValue("bottleneck2", "R2-to-server capacity in Mbps", bottleneck2RateMbps);
    cmd.AddValue("time", "Simulation duration in seconds", simulationTime);
    cmd.AddValue("seed", "Random seed", seed);
    cmd.AddValue("eho", "Enable EHPSO optimizer (1) or disable it (0)", useEho);
    cmd.AddValue("ml", "Enable RandomForest prediction (1) or disable it (0)", useMl);
    cmd.Parse(argc, argv);

    std::vector<HospitalDeviceConfig> devices;
    try {
        devices = HospitalConfig::LoadDevices(deviceConfigPath);
        for (auto& d : devices) {
            if (d.startTimeSeconds >= simulationTime) throw std::runtime_error("Device starts after simulation ends: " + d.deviceId);
            d.stopTimeSeconds = std::min(d.stopTimeSeconds, simulationTime);
            if (d.stopTimeSeconds <= d.startTimeSeconds) throw std::runtime_error("Invalid active interval for device: " + d.deviceId);
        }
    } catch (const std::exception& e) {
        std::cerr << "Hospital configuration error: " << e.what() << '\n'; return 1;
    }
    if (devices.empty() || simulationTime <= 0 || profileInterval <= 0 || controlInterval <= 0 || bottleneck1RateMbps <= 0 || bottleneck2RateMbps <= 0) {
        std::cerr << "Error: devices, intervals, simulation time and capacities must be positive.\n"; return 1;
    }
    const bool controlled = useEho || useMl;
    const double effectiveCapacity = std::min(bottleneck1RateMbps, bottleneck2RateMbps);
    std::system(("mkdir -p \"" + projectRoot + "/data\" \"" + projectRoot + "/results\"").c_str());

    TrafficGenerator generator(devices, simulationTime, profileInterval, seed);
    auto traffic = generator.GenerateTraffic();
    generator.PrintTraffic(traffic);

    std::vector<double> priorities, deadlinesMs;
    std::vector<std::string> deviceIds, categories;
    for (const auto& t : traffic) {
        priorities.push_back(t.priority);
        deadlinesMs.push_back(t.maxDelayMs);
        deviceIds.push_back(t.deviceId);
        categories.push_back(t.category);
    }

    std::cout << "\n=== HOSPITAL EDGE TRAFFIC CONTROL ===\n"
              << "Devices: " << devices.size() << " | Bottleneck 1: " << bottleneck1RateMbps
              << " Mbps | Bottleneck 2: " << bottleneck2RateMbps << " Mbps | Effective capacity: "
              << effectiveCapacity << " Mbps\nSeed: " << seed << " | Duration: " << simulationTime
              << " s | Mode: " << (useEho ? "EHPSO" : "priority allocator")
              << (useMl ? "+RandomForest ML" : "") << "\n";

    NetworkScenario network(static_cast<uint32_t>(devices.size()), bottleneck1RateMbps, bottleneck2RateMbps, "10ms");
    network.CreateNetwork(); network.InstallInternet(); network.AssignAddresses();
    NodeContainer sources = network.GetSourceNodes();
    Ptr<Node> server = network.GetServerNode();
    const Ipv4Address serverAddress = network.GetServerAddress();
    const uint16_t port = 9000;
    Ptr<TrafficReceiver> receiver = CreateObject<TrafficReceiver>();
    receiver->Setup(port); server->AddApplication(receiver);
    for (uint32_t i = 0; i < traffic.size(); ++i)
    {
        receiver->RegisterDevice(i, traffic[i].deviceId);
    }
    receiver->SetStartTime(Seconds(0.0)); receiver->SetStopTime(Seconds(simulationTime));

    std::vector<Ptr<TrafficApplication>> applications;
    for (uint32_t i = 0; i < traffic.size(); ++i) {
        Ptr<TrafficApplication> app = CreateObject<TrafficApplication>();
        const double initialDemand = traffic[i].profile.front().demandMbps;
        app->Setup(
            initialDemand,
            traffic[i].packetSize,
            serverAddress,
            port,
            i,
            traffic[i].maxDelayMs);

        sources.Get(i)->AddApplication(app);
        app->SetStartTime(Seconds(traffic[i].startTime)); app->SetStopTime(Seconds(traffic[i].stopTime));
        applications.push_back(app);
        for (const auto& point : traffic[i].profile) {
            if (point.time <= traffic[i].startTime || point.time >= traffic[i].stopTime) continue;
            Simulator::Schedule(Seconds(point.time), &UpdateTrafficDemand, app, point.demandMbps, controlled);
        }
    }
    TrafficMonitor monitor(applications, PeekPointer(receiver), effectiveCapacity);
    std::unique_ptr<EHPSO> optimizer;
    std::unique_ptr<TrafficController> controller;
    if (useEho) optimizer = std::make_unique<EHPSO>(30, 3, 20, 0.5, 0.1, 20, 20, 0.7, 1.5, 1.5, seed);
    if (controlled) {
        controller = std::make_unique<TrafficController>(
            applications,
            priorities,
            deadlinesMs,
            deviceIds,
            categories,
            &monitor,
            optimizer.get(),
            effectiveCapacity,
            controlInterval,
            profileInterval,
            simulationTime,
            useEho,
            useMl,
            projectRoot);

        controller->ApplyInitialAllocation();
        controller->Start();
    } else {
        std::cout << "Baseline mode: no controller; applications offer their configured demand.\n";
    }
    std::cout << "Starting simulation...\n";
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor.PrintFinalResults(simulationTime);

    receiver->WriteDeviceMetrics(
        projectRoot + "/results/device_delivery_metrics.csv");

    Simulator::Destroy();
    return 0;
}
