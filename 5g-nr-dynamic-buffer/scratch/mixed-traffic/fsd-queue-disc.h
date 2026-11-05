// SPDX-License-Identifier: GPL-2.0-only
// Fair-Share Drop (FSD): FifoQueueDisc + per-flow admission. MaxSize = hard cap (2 * LMAX).
// Soft limit set by the Dyn controller. Over the soft limit an arrival is dropped if its flow
// holds >= limit/activeFlows packets (share rule, default) or, with SetLongest(true), if its
// flow would become the longest (ablation). The queue never exceeds 2 * limit.
#ifndef FSD_QUEUE_DISC_H
#define FSD_QUEUE_DISC_H

#include "ns3/fifo-queue-disc.h"

#include <map>

namespace ns3
{

class FsdQueueDisc : public FifoQueueDisc
{
  public:
    static TypeId GetTypeId();

    void SetLimit(uint32_t pkts) { m_limit = pkts; }

    void SetLongest(bool b) { m_longest = b; }

  private:
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;
    Ptr<QueueDiscItem> DoDequeue() override;

    uint32_t m_limit{83};                 // soft limit (packets)
    bool m_longest{false};                // longest-flow rule instead of share rule
    std::map<uint32_t, uint32_t> m_flows; // flow hash -> packets queued
};

} // namespace ns3

#endif /* FSD_QUEUE_DISC_H */
