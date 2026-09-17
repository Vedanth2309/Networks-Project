#include "ehpso.h"
#include "network-scenario.h"
#include "traffic-application.h"
#include "traffic-controller.h"
#include "traffic-generator.h"
#include "traffic-monitor.h"
#include "traffic-receiver.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"

#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

using namespace ns3;

static void UpdateTrafficDemand(Ptr<TrafficApplication> application, double demandMbps) {
    application->SetDemand(demandMbps);
}

int main(int argc, char* argv[]) {
    uint32_t numberOfSources = 6;
    double bottleneckRateMbps = 5.0;
    double simulationTime = 20.0;
    double profileInterval = 1.0;
    double controlInterval = 1.0;
    uint32_t packetSize = 1024;
    uint32_t seed = 12345;
    bool useEho = true;

    CommandLine cmd(__FILE__);

    cmd.AddValue(
        "sources",
        "Number of IoT sources",
        numberOfSources);

    cmd.AddValue(
        "bottleneck",
        "Bottleneck capacity in Mbps",
        bottleneckRateMbps);

    cmd.AddValue(
        "time",
        "Simulation time in seconds",
        simulationTime);

    cmd.AddValue(
        "seed",
        "Random seed",
        seed);

    cmd.AddValue(
        "eho",
        "Enable EHPSO (1) or baseline (0)",
        useEho);

    cmd.Parse(argc, argv);

    std::cout
        << "\n========================================\n"
        << " EHPSO DYNAMIC TRAFFIC CONTROL\n"
        << "========================================\n";

    std::cout
        << "IoT sources     : "
        << numberOfSources
        << "\n";

    std::cout
        << "Bottleneck      : "
        << bottleneckRateMbps
        << " Mbps\n";

    std::cout
        << "Simulation time : "
        << simulationTime
        << " seconds\n";

    std::cout
        << "Profile interval: "
        << profileInterval
        << " seconds\n";

    std::cout
        << "Control interval: "
        << controlInterval
        << " seconds\n";

    std::cout
        << "Seed            : "
        << seed
        << "\n";

    std::cout
        << "Mode            : "
        << (useEho
                ? "WITH EHPSO"
                : "WITHOUT EHPSO")
        << "\n";

    TrafficGenerator generator(
        numberOfSources,
        packetSize,
        0.5,
        2.0,
        simulationTime,
        profileInterval,
        seed);

    std::vector<TrafficGenerator::TrafficSource> traffic = generator.GenerateTraffic();

    generator.PrintTraffic(traffic);

    NetworkScenario network(numberOfSources, bottleneckRateMbps, "10ms");

    network.CreateNetwork();
    network.InstallInternet();
    network.AssignAddresses();

    NodeContainer sourceNodes = network.GetSourceNodes();
    Ptr<Node> server = network.GetServerNode();
    Ipv4Address serverAddress = network.GetServerAddress();

    const uint16_t port = 9000;
    Ptr<TrafficReceiver> receiver = CreateObject<TrafficReceiver>();

    receiver->Setup(port);
    server->AddApplication(receiver);

    receiver->SetStartTime(Seconds(0.0));

    receiver->SetStopTime(Seconds(simulationTime));

    std::vector<Ptr<TrafficApplication>> applications;

    for (uint32_t i = 0; i < numberOfSources; ++i) {
        Ptr<TrafficApplication> application = CreateObject<TrafficApplication>();

        double initialDemand = traffic[i].profile.front().demandMbps;

        application->Setup(initialDemand, traffic[i].packetSize, serverAddress, port);

        sourceNodes.Get(i)->AddApplication(application);

        application->SetStartTime(Seconds(traffic[i].startTime));

        application->SetStopTime(Seconds(traffic[i].stopTime));

        applications.push_back(application);

        for (const auto& point : traffic[i].profile) {
            if (point.time <=
                traffic[i].startTime) {
                continue;
            }

            if (point.time >= traffic[i].stopTime) {
                continue;
            }

            Simulator::Schedule(
                Seconds(point.time),
                &UpdateTrafficDemand,
                application,
                point.demandMbps);
        }
    }

    TrafficMonitor monitor(applications, PeekPointer(receiver), bottleneckRateMbps);

    std::unique_ptr<EHPSO> optimizer;

    std::unique_ptr<TrafficController> controller;

    if (useEho) {
        std::cout
            << "\n========================================\n"
            << " EHPSO ENABLED\n"
            << "========================================\n";

        optimizer = std::make_unique<EHPSO>(
                30,
                3,
                20,
                0.5,
                0.1,

                20,
                20,
                0.7,
                1.5,
                1.5,
                seed);

        controller = std::make_unique<
                TrafficController>(
                applications,
                &monitor,
                optimizer.get(),
                bottleneckRateMbps,
                controlInterval,
                profileInterval,
                simulationTime);

        controller->Start();
    }
    else {
        std::cout
            << "\n========================================\n"
            << " BASELINE - WITHOUT EHPSO\n"
            << "========================================\n";
    }

    std::cout
        << "\n========================================\n"
        << " STARTING SIMULATION\n"
        << "========================================\n";

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    std::cout
        << "\n========================================\n"
        << " FINAL RESULTS - "
        << (useEho
                ? "WITH EHPSO"
                : "WITHOUT EHPSO")
        << "\n"
        << "========================================\n";

    monitor.PrintFinalResults(simulationTime);

    Simulator::Destroy();

    return 0;
}