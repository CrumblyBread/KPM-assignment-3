#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/config-store.h"
#include "ns3/buildings-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"


using namespace ns3;

int main(int argc, char *argv[])
{
    // Time for teh simulation
    Time simTime = Seconds(10); // Simulation time

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);


    // Number of eNodeBs and UEs
    uint32_t numEnb = 2;
    uint32_t numUe = argc == 4 ? std::stoi(argv[1]): 10;
    double distance = argc == 4 ? std::stoi(argv[2]): 500;


    // Set default values for the LTE simulation
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(30.0));
    Config::SetDefault("ns3::LteUePhy::TxPower", DoubleValue(23.0));
    Config::SetDefault("ns3::LteUePowerControl::ClosedLoop", BooleanValue(true));

    // Create all nodes
    NodeContainer enbNodes, ueNodes, remoteHost;
    enbNodes.Create(numEnb);
    ueNodes.Create(numUe);
    remoteHost.Create(1); // Create one remote host

    // Install Mobility Model for eNodeBs (static)
    MobilityHelper mobilityEnb;
    Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numEnb; ++i)
    {
        positionAllocEnb->Add(Vector(i * distance, 0.0, 0.0)); // Distribute eNBs along the x-axis
    }
    mobilityEnb.SetPositionAllocator(positionAllocEnb);
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // Install Mobility Model for UEs (random walking model)
    MobilityHelper mobilityUe;
    Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUe; ++i)
    {
        positionAllocUe->Add(Vector(0.0, i * distance, 0.0)); // Distribute UEs along the y-axis
    }
    mobilityUe.SetPositionAllocator(positionAllocUe);
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-5000, 5000, -5000, 5000)));
    mobilityUe.Install(ueNodes);

     // Install Mobility Model for the remote host
    MobilityHelper mobilityRemoteHost;
    mobilityRemoteHost.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityRemoteHost.Install(remoteHost);

    // Install te LTE Devices on nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Configure the propagation loss model
    Ptr<HybridBuildingsPropagationLossModel> lossModel = CreateObject<HybridBuildingsPropagationLossModel>();
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::HybridBuildingsPropagationLossModel"));

    // Add a building to the scenario
    Ptr<Building> building = CreateObject<Building>();
    building->SetBoundaries(Box(0.0, 50.0, 0.0, 50.0, 0.0, 20.0)); // Define building dimensions
    building->SetBuildingType(Building::Residential);
    building->SetExtWallsType(Building::ConcreteWithWindows);

    // Install UEs and eNodeBs in the building context
    BuildingsHelper::Install(ueNodes);
    BuildingsHelper::Install(enbNodes);
    
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);
    NetDeviceContainer remoteHostDevices = lteHelper->InstallUeDevice(remoteHost);

    // Create internet stack and assign IP addresses
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);
    internet.Install(remoteHost);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpInterfaces = ipv4.Assign(enbDevs);

    ipv4.SetBase("2.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ueIpInterfaces = ipv4.Assign(ueDevs);

    ipv4.SetBase("3.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer remoteHostInterfaces = ipv4.Assign(remoteHostDevices);

    uint16_t echoPort = 9;
    //Install USP server on port 9
    UdpEchoServerHelper echoServer(echoPort);

    // Install the Server on an eNodeB
    ApplicationContainer serverApps = echoServer.Install(remoteHost);
    serverApps.Start(Seconds(1.0)); // Start the server at 1 second
    serverApps.Stop(simTime);       // Stop the server at simulation end

    UdpEchoClientHelper echoClient(remoteHostInterfaces.GetAddress(0), echoPort); // Server IP and port
    echoClient.SetAttribute("MaxPackets", UintegerValue(5000));             // Large enough to last the simulation
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(argc == 4 ? std::stoi(argv[2]): 500)));      // Packet interval: 200 ms
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));             // Packet size in bytes

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numUe; ++i)
    {
        clientApps.Add(echoClient.Install(ueNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0)); // Start the clients at 2 seconds
    clientApps.Stop(simTime);       // Stop the clients at simulation end

    NS_LOG_UNCOND("UE: " << numUe);
    NS_LOG_UNCOND("distance: " << distance);

    // Installing applications 
    NS_LOG_UNCOND("Simulation start.");
    Simulator::Stop(simTime);
    Simulator::Run();
    NS_LOG_UNCOND("Simulation stop.");

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    double packetsSent = 0.0;
    double packetsReceived = 0.0;

    if (stats.empty()) {
        NS_LOG_UNCOND("No flows were detected during the simulation.");
    } else {
        for (auto const& flow : stats) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
            NS_LOG_UNCOND("Flow ID: " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
            NS_LOG_UNCOND("  Tx Packets: " << flow.second.txPackets);
            NS_LOG_UNCOND("  Rx Packets: " << flow.second.rxPackets);
            NS_LOG_UNCOND("  Throughput: " << flow.second.rxBytes * 8.0 / simTime.GetSeconds() / 1e6 << " Mbps");
            NS_LOG_UNCOND("  Delay: " << flow.second.delaySum.GetSeconds() / flow.second.rxPackets << " seconds");

            totalThroughput += flow.second.rxBytes * 8.0 / simTime.GetSeconds() / 1e6;
            totalDelay += flow.second.delaySum.GetSeconds();
            packetsSent += flow.second.txPackets;
            packetsReceived += flow.second.rxPackets;
        }
    }

    double averageThroughput = totalThroughput;
    double averageDelay = (packetsReceived > 0) ? totalDelay / packetsReceived : 0.0;
    double pdr = (packetsSent > 0) ? (packetsReceived / packetsSent) * 100.0 : 0.0;

    NS_LOG_UNCOND("=== Simulation Results ===");
    NS_LOG_UNCOND("Total Throughput: " << averageThroughput << " Mbps");
    NS_LOG_UNCOND("Average Latency: " << averageDelay * 1000 << " ms");
    NS_LOG_UNCOND("Packet Delivery Ratio: " << pdr << " %");

    Simulator::Destroy();
    return 0;
}
