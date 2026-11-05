// SPDX-License-Identifier: GPL-2.0-only
#include "fsd-queue-disc.h"

#include "ns3/drop-tail-queue.h" // full Queue<QueueDiscItem> type for GetInternalQueue()

#include <algorithm>

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(FsdQueueDisc);

TypeId
FsdQueueDisc::GetTypeId()
{
    static TypeId tid = TypeId("ns3::FsdQueueDisc")
                            .SetParent<FifoQueueDisc>()
                            .SetGroupName("TrafficControl")
                            .AddConstructor<FsdQueueDisc>();
    return tid;
}

bool
FsdQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    uint32_t h = item->Hash();
    auto it = m_flows.find(h);
    uint32_t mine = it == m_flows.end() ? 0 : it->second; // packets this flow has queued
    uint32_t n = GetInternalQueue(0)->GetNPackets();
    if (n >= m_limit) // over the soft limit: admit only flows below their fair share
    {
        bool overShare;
        if (m_longest)
        {
            uint32_t top = 0; // longest other flow
            for (const auto& [flow, pkts] : m_flows)
            {
                if (flow != h)
                {
                    top = std::max(top, pkts);
                }
            }
            overShare = mine >= top;
        }
        else
        {
            uint32_t active = m_flows.size() + (mine == 0); // include the arriving flow
            overShare = mine >= m_limit / active;
        }
        if (overShare || n >= 2 * m_limit)
        {
            DropBeforeEnqueue(item, "Fair-share drop");
            return false;
        }
    }
    if (!GetInternalQueue(0)->Enqueue(item))
    {
        return false;
    }
    m_flows[h]++;
    return true;
}

Ptr<QueueDiscItem>
FsdQueueDisc::DoDequeue()
{
    Ptr<QueueDiscItem> item = GetInternalQueue(0)->Dequeue();
    if (item)
    {
        auto it = m_flows.find(item->Hash());
        if (it != m_flows.end() && --it->second == 0)
        {
            m_flows.erase(it);
        }
    }
    return item;
}

} // namespace ns3
