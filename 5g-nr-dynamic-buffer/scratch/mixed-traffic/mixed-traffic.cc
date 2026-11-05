// SPDX-License-Identifier: GPL-2.0-only
// mixed-traffic.cc: 5G NR mixed traffic over one wired bottleneck.
// --q: pfifo | codel | pie | cobalt | fqcodel | fqpie   (paper baseline, ~1 GB buffer)
//      fifo (static limit --n) | dyn (AIMD limit) | fsd (Dyn + fair-share drop)
// hosts(7) --100M/5ms--> router ==10M/40ms (qdisc here)==> PGW -> gNB -> 7 UEs.
// Flow i: host i -> UE i. 0 UDP burst | 1 TCP bulk | 2 FTP | 3 HTTP | 4 VoIP | 5 Gaming | 6 Video
// (DASH omitted). Output: <outDir>/<tag>/s<seed>/{goodput,qdelay,drops,limit}.dat + summary.tsv
#include "fsd-queue-disc.h"

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>

using namespace ns3;

static const uint32_t N = 7;                 // host/UE pairs
std::vector<std::vector<double>> rxBytes;    // rxBytes[flow][second]
std::vector<double> sojSum, sojCnt, dropBin; // per second: sojourn sum (s), count, drops
Ptr<QueueDisc> gQd;                          // the bottleneck qdisc
Ptr<FsdQueueDisc> gFsd;                      // same qdisc when q=fsd, else null
uint32_t lastDrops = 0;

// Dyn controller (Algorithm 1): every 100 ms, AIMD on the FIFO limit from the max sojourn
const double TARGET = 0.025;                      // s
const uint32_t STEP = 5, LMIN = 20, LMAX = 200;   // packets
uint32_t gH = 3;                                  // hold periods before halving (--H)
uint32_t gLim = 83, gBad = 0;                     // limit (start = BDP), consecutive bad periods
double gMaxSoj = 0;                               // max sojourn in the current period (s)
std::vector<std::pair<double, uint32_t>> gLimLog; // (time, limit)

static uint32_t
Bin()
{
    return (uint32_t)Simulator::Now().GetSeconds();
}

void
OnRx(uint32_t flow, Ptr<const Packet> p, const Address&)
{
    if (Bin() < rxBytes[flow].size())
    {
        rxBytes[flow][Bin()] += p->GetSize();
    }
}

void
OnSojourn(Time t)
{
    gMaxSoj = std::max(gMaxSoj, t.GetSeconds());
    if (Bin() < sojSum.size())
    {
        sojSum[Bin()] += t.GetSeconds();
        sojCnt[Bin()] += 1;
    }
}

void
Tick() // once per second: drops since last tick -> bin of the second that just ended
{
    uint32_t cur = gQd->GetStats().nTotalDroppedPackets;
    uint32_t b = Bin();
    if (b >= 1 && b <= dropBin.size())
    {
        dropBin[b - 1] = cur - lastDrops;
    }
    lastDrops = cur;
    Simulator::Schedule(Seconds(1), &Tick);
}

void
Ctl() // Dyn: halve after gH bad periods, grow by STEP when calm, bounded [LMIN, LMAX]
{
    gBad = gMaxSoj > TARGET ? gBad + 1 : 0;
    if (gBad >= gH)
    {
        gLim = std::max(LMIN, gLim / 2);
        gBad = 0;
    }
    else if (gMaxSoj < TARGET / 2)
    {
        gLim = std::min(LMAX, gLim + STEP);
    }
    gMaxSoj = 0;
    if (gFsd)
    {
        gFsd->SetLimit(gLim); // soft limit, no size constraint
    }
    else
    {
        // ns-3 forbids size < occupancy, so clamp; ponytail: excess drains with up to 100 ms lag
        gQd->SetMaxSize(QueueSize(QueueSizeUnit::PACKETS, std::max(gLim, gQd->GetNPackets())));
    }
    gLimLog.emplace_back(Simulator::Now().GetSeconds(), gLim);
    Simulator::Schedule(MilliSeconds(100), &Ctl);
}

int
main(int argc, char* argv[])
{
    // ---- CLI ----
    std::string q = "codel"; // bottleneck qdisc
    uint32_t seed = 1;       // RNG run number
    double simTime = 60;     // s
    std::string outDir = "results";
    uint32_t fifoN = 83;  // q=fifo: static limit (packets)
    bool longest = false; // q=fsd: longest-flow rule (ablation)
    std::string udpRate = "10Mbps"; // UDP burst rate (assumption D8)
    CommandLine cmd(__FILE__);
    cmd.AddValue("q", "pfifo|codel|pie|cobalt|fqcodel|fqpie|fifo|dyn|fsd", q);
    cmd.AddValue("seed", "RNG run number", seed);
    cmd.AddValue("simTime", "simulation time (s)", simTime);
    cmd.AddValue("outDir", "output directory", outDir);
    cmd.AddValue("n", "q=fifo: limit in packets", fifoN);
    cmd.AddValue("H", "q=dyn|fsd: hold periods before halving", gH);
    cmd.AddValue("longest", "q=fsd: drop only from the longest flow", longest);
    cmd.AddValue("udpRate", "UDP burst rate (D8)", udpRate);
    cmd.Parse(argc, argv);
    // names the output dir and the summary row
    std::string tag =
        q == "fifo" ? q + std::to_string(fifoN) : (q == "fsd" && longest ? "fsdl" : q);

    RngSeedManager::SetSeed(1);
    RngSeedManager::SetRun(seed);
    // RLC never drops: the only buffer that matters is the bottleneck qdisc
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    // ---- scenario: 1 gNB, N UEs near it (as cttc-nr-demo) ----
    GridScenarioHelper grid;
    grid.SetRows(1);
    grid.SetColumns(1);
    grid.SetHorizontalBsDistance(10.0);
    grid.SetVerticalBsDistance(10.0);
    grid.SetBsHeight(10);
    grid.SetUtHeight(1.5);
    grid.SetSectorization(GridScenarioHelper::SINGLE);
    grid.SetBsNumber(1);
    grid.SetUtNumber(N);
    grid.SetScenarioHeight(3);
    grid.SetScenarioLength(3);
    grid.CreateScenario();

    // ---- radio: 28 GHz, 50 MHz, numerology 4, 35 dBm, UMi, direct-path beamforming ----
    Ptr<NrPointToPointEpcHelper> nrEpc = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> bf = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nr = CreateObject<NrHelper>();
    nr->SetBeamformingHelper(bf);
    nr->SetEpcHelper(nrEpc);

    CcBwpCreator ccBwp;
    CcBwpCreator::SimpleOperationBandConf bandConf(28e9, 50e6, 1);
    OperationBandInfo band = ccBwp.CreateOperationBandContiguousCc(bandConf);
    Ptr<NrChannelHelper> ch = CreateObject<NrChannelHelper>();
    ch->ConfigureFactories("UMi", "Default", "ThreeGpp");
    ch->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    ch->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    ch->AssignChannelsToBands({band});
    BandwidthPartInfoPtrVector bwps = CcBwpCreator::GetAllBwps({band});

    bf->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpc->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    nr->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nr->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nr->SetUeAntennaAttribute("AntennaElement",
                              PointerValue(CreateObject<IsotropicAntennaModel>()));
    nr->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nr->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nr->SetGnbAntennaAttribute("AntennaElement",
                               PointerValue(CreateObject<IsotropicAntennaModel>()));

    NetDeviceContainer gnbDev = nr->InstallGnbDevice(grid.GetBaseStations(), bwps);
    NetDeviceContainer ueDev = nr->InstallUeDevice(grid.GetUserTerminals(), bwps);
    nr->AssignStreams({.scenario = &grid, .gnbDevs = gnbDev, .ueDevs = ueDev});
    NrHelper::GetGnbPhy(gnbDev.Get(0), 0)->SetAttribute("Numerology", UintegerValue(4));
    NrHelper::GetGnbPhy(gnbDev.Get(0), 0)->SetAttribute("TxPower", DoubleValue(35)); // dBm

    // ---- wired side: hosts -- router -- PGW ----
    Ptr<Node> pgw = nrEpc->GetPgwNode();
    NodeContainer hosts;
    hosts.Create(N);
    Ptr<Node> router = CreateObject<Node>();
    InternetStackHelper internet;
    internet.Install(hosts);
    internet.Install(router);
    internet.Install(grid.GetUserTerminals());

    PointToPointHelper edge; // host <-> router
    edge.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    edge.SetChannelAttribute("Delay", StringValue("5ms"));
    PointToPointHelper core; // router <-> PGW = the bottleneck
    core.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    core.SetChannelAttribute("Delay", StringValue("40ms"));
    // device queue 1 packet: the backlog lives in the qdisc
    core.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("1p"));

    Ipv4AddressHelper ip;
    Ipv4StaticRoutingHelper rt;
    std::vector<Ipv4Address> hostAddr(N); // address of each host (HTTP server needs it)

    // host i -> router; each host reaches the UE subnet 7.0.0.0/8 via the router
    for (uint32_t i = 0; i < N; ++i)
    {
        NetDeviceContainer d = edge.Install(hosts.Get(i), router);
        std::ostringstream base;
        base << "10.1." << i << ".0";
        ip.SetBase(base.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer ifc = ip.Assign(d);
        hostAddr[i] = ifc.GetAddress(0);
        rt.GetStaticRouting(hosts.Get(i)->GetObject<Ipv4>())
            ->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                                Ipv4Mask("255.0.0.0"),
                                ifc.GetAddress(1),
                                ifc.Get(0).second);
    }

    // router -> PGW. Install the qdisc BEFORE Assign, else ns-3 adds a default one.
    std::map<std::string, std::string> types = {{"pfifo", "ns3::PfifoFastQueueDisc"},
                                                {"codel", "ns3::CoDelQueueDisc"},
                                                {"pie", "ns3::PieQueueDisc"},
                                                {"cobalt", "ns3::CobaltQueueDisc"},
                                                {"fqcodel", "ns3::FqCoDelQueueDisc"},
                                                {"fqpie", "ns3::FqPieQueueDisc"},
                                                {"fifo", "ns3::FifoQueueDisc"},
                                                {"dyn", "ns3::FifoQueueDisc"},
                                                {"fsd", "ns3::FsdQueueDisc"}};
    // qdisc size: fifo = static n; dyn = LMAX (Ctl lowers it); fsd = 2*LMAX hard cap; AQMs ~1 GB
    std::string maxSize = q == "fifo"  ? std::to_string(fifoN) + "p"
                          : q == "dyn" ? std::to_string(LMAX) + "p"
                          : q == "fsd" ? std::to_string(2 * LMAX) + "p"
                                       : "666666p";
    NetDeviceContainer cd = core.Install(router, pgw);
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(types.at(q), "MaxSize", StringValue(maxSize));
    gQd = tch.Install(cd.Get(0)).Get(0);
    gFsd = DynamicCast<FsdQueueDisc>(gQd);
    if (gFsd)
    {
        gFsd->SetLongest(longest);
    }
    ip.SetBase("10.2.0.0", "255.255.255.0");
    Ipv4InterfaceContainer cifc = ip.Assign(cd);
    rt.GetStaticRouting(router->GetObject<Ipv4>())
        ->AddNetworkRouteTo(Ipv4Address("7.0.0.0"),
                            Ipv4Mask("255.0.0.0"),
                            cifc.GetAddress(1),
                            cifc.Get(0).second);
    rt.GetStaticRouting(pgw->GetObject<Ipv4>())
        ->AddNetworkRouteTo(Ipv4Address("10.1.0.0"),
                            Ipv4Mask("255.255.0.0"),
                            cifc.GetAddress(0),
                            cifc.Get(1).second);

    // ---- UE IPs, default route (TCP ACKs go back), attach ----
    Ipv4InterfaceContainer ueIf = nrEpc->AssignUeIpv4Address(NetDeviceContainer(ueDev));
    for (uint32_t i = 0; i < N; ++i)
    {
        rt.GetStaticRouting(grid.GetUserTerminals().Get(i)->GetObject<Ipv4>())
            ->SetDefaultRoute(nrEpc->GetUeDefaultGatewayAddress(), 1);
    }
    nr->AttachToClosestGnb(ueDev, gnbDev);

    // ---- measurement storage ----
    uint32_t nBins = (uint32_t)std::ceil(simTime);
    rxBytes.assign(N, std::vector<double>(nBins, 0));
    sojSum.assign(nBins, 0);
    sojCnt.assign(nBins, 0);
    dropBin.assign(nBins, 0);
    gQd->TraceConnectWithoutContext("SojournTime", MakeCallback(&OnSojourn));
    Simulator::Schedule(Seconds(1), &Tick);
    if (q == "dyn" || q == "fsd")
    {
        Simulator::Schedule(MilliSeconds(100), &Ctl); // after qdisc init, before traffic (0.4 s)
    }

    // ---- sinks: UDP/TCP PacketSink on UEs (HTTP is different: UE is the client) ----
    const char* tcpOrUdp[N] = {"udp", "tcp", "tcp", "http", "udp", "udp", "udp"};
    for (uint32_t i = 0; i < N; ++i)
    {
        if (std::string(tcpOrUdp[i]) == "http")
        {
            continue;
        }
        std::string sf =
            std::string(tcpOrUdp[i]) == "tcp" ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory";
        PacketSinkHelper sh(sf, InetSocketAddress(Ipv4Address::GetAny(), 5000 + i));
        ApplicationContainer s = sh.Install(grid.GetUserTerminals().Get(i));
        s.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback(&OnRx, i));
        s.Start(Seconds(0));
    }

    // ---- senders (all start 0.4 s, stop at simTime) ----
    double w = simTime / 6; // UDP burst: OFF w, ON w, OFF w, ON w, OFF w, ON w (paper: 50 s each)
    ApplicationContainer cl;

    OnOffHelper burst("ns3::UdpSocketFactory", InetSocketAddress(ueIf.GetAddress(0), 5000));
    burst.SetAttribute("DataRate", StringValue(udpRate));
    burst.SetAttribute("PacketSize", UintegerValue(1400));
    std::string wv = "ns3::ConstantRandomVariable[Constant=" + std::to_string(w) + "]";
    burst.SetAttribute("OnTime", StringValue(wv));
    burst.SetAttribute("OffTime", StringValue(wv));
    ApplicationContainer b = burst.Install(hosts.Get(0));
    b.Start(Seconds(0)); // OnOff begins OFF
    b.Stop(Seconds(simTime));

    BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(ueIf.GetAddress(1), 5001));
    bulk.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    cl.Add(bulk.Install(hosts.Get(1)));

    TrafficGeneratorHelper ftp("ns3::TcpSocketFactory",
                               InetSocketAddress(ueIf.GetAddress(2), 5002),
                               TrafficGeneratorNgmnFtpMulti::GetTypeId());
    ftp.SetAttribute("PacketSize", UintegerValue(1448));
    ftp.SetAttribute("MaxFileSize", UintegerValue(5000000));
    cl.Add(ftp.Install(hosts.Get(2)));

    ThreeGppHttpServerHelper httpSrv(hostAddr[3]); // server on host 3, client (browser) on UE 3
    ApplicationContainer hs = httpSrv.Install(hosts.Get(3));
    hs.Start(Seconds(0));
    ThreeGppHttpClientHelper httpCli(hostAddr[3]);
    ApplicationContainer hc = httpCli.Install(grid.GetUserTerminals().Get(3));
    hc.Get(0)->TraceConnectWithoutContext("Rx", MakeBoundCallback(&OnRx, 3u));
    hc.Start(Seconds(0.4));
    hc.Stop(Seconds(simTime));

    TrafficGeneratorHelper voip("ns3::UdpSocketFactory",
                                InetSocketAddress(ueIf.GetAddress(4), 5004),
                                TrafficGeneratorNgmnVoip::GetTypeId());
    cl.Add(voip.Install(hosts.Get(4)));

    TrafficGeneratorHelper game("ns3::UdpSocketFactory",
                                InetSocketAddress(ueIf.GetAddress(5), 5005),
                                TrafficGeneratorNgmnGaming::GetTypeId());
    cl.Add(game.Install(hosts.Get(5)));

    TrafficGeneratorHelper video("ns3::UdpSocketFactory",
                                 InetSocketAddress(ueIf.GetAddress(6), 5006),
                                 TrafficGeneratorNgmnVideo::GetTypeId());
    video.SetAttribute("NumberOfPacketsInFrame", UintegerValue(8));
    video.SetAttribute("InterframeIntervalTime", TimeValue(Seconds(0.100)));
    cl.Add(video.Install(hosts.Get(6)));

    cl.Start(Seconds(0.4));
    cl.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ---- 1 s bins: goodput.dat (t, 7 flows Mbps), qdelay.dat (t, ms), drops.dat (t, pkts) ----
    std::string dir = outDir + "/" + tag + "/s" + std::to_string(seed);
    std::filesystem::create_directories(dir);
    std::ofstream g(dir + "/goodput.dat"), d(dir + "/qdelay.dat"), x(dir + "/drops.dat");
    if (!gLimLog.empty()) // dyn / fsd only
    {
        std::ofstream lf(dir + "/limit.dat");
        for (auto& [t, lim] : gLimLog)
        {
            lf << t << ' ' << lim << '\n';
        }
    }
    for (uint32_t t = 0; t < nBins; ++t)
    {
        g << t + 1;
        for (uint32_t i = 0; i < N; ++i)
        {
            g << ' ' << rxBytes[i][t] * 8 / 1e6;
        }
        g << '\n';
        d << t + 1 << ' ' << (sojCnt[t] > 0 ? sojSum[t] / sojCnt[t] * 1000 : NAN) << '\n';
        x << t + 1 << ' ' << dropBin[t] << '\n';
    }

    // ---- summary: mean delay, avg throughput, Jain's index over per-flow totals ----
    double sumX = 0, sumX2 = 0; // per-flow Mbps: sum, sum of squares
    std::cout << "q=" << tag << " seed=" << seed << " per-flow Mbps:";
    for (uint32_t i = 0; i < N; ++i)
    {
        double mbps =
            std::accumulate(rxBytes[i].begin(), rxBytes[i].end(), 0.0) * 8 / simTime / 1e6;
        std::cout << ' ' << mbps;
        sumX += mbps;
        sumX2 += mbps * mbps;
    }
    double sumS = std::accumulate(sojSum.begin(), sojSum.end(), 0.0);
    double cntS = std::accumulate(sojCnt.begin(), sojCnt.end(), 0.0);
    double totDrops = std::accumulate(dropBin.begin(), dropBin.end(), 0.0);
    double meanDelayMs = cntS > 0 ? sumS / cntS * 1000 : 0;
    double jain = sumX2 > 0 ? sumX * sumX / (N * sumX2) : 0;
    std::cout << "\n  mean qdelay=" << meanDelayMs << " ms  total thr=" << sumX
              << " Mbps  Jain=" << jain << "  drops=" << totDrops << std::endl;
    std::ofstream s(outDir + "/summary.tsv", std::ios::app);
    s << tag << '\t' << seed << '\t' << simTime << '\t' << meanDelayMs << '\t' << sumX << '\t'
      << jain << '\t' << totDrops << '\n';

    Simulator::Destroy();
    return 0;
}
