#ifndef VAST_SOURCE_PCAP_H
#define VAST_SOURCE_PCAP_H

#include <chrono>
#include <pcap.h>
#include <unordered_map>
#include <random>
#include "vast/schema.h"
#include "vast/source/synchronous.h"
#include "vast/util/hash_combine.h"
#include "vast/util/operators.h"

namespace vast {
namespace detail {

struct connection : util::equality_comparable<connection>
{
  address src;
  address dst;
  port sport;
  port dport;

  friend bool operator==(connection const& lhs, connection const& rhs)
  {
    return lhs.src == rhs.src && lhs.dst == rhs.dst
        && lhs.sport == rhs.sport && lhs.dport == rhs.dport;
  }
};

} // namespace detail
} // namespace vast

namespace std {

template <>
struct hash<vast::detail::connection>
{
  size_t operator()(vast::detail::connection const& c) const
  {
    auto src0 = *reinterpret_cast<uint64_t const*>(&c.src.data()[0]);
    auto src1 = *reinterpret_cast<uint64_t const*>(&c.src.data()[8]);
    auto dst0 = *reinterpret_cast<uint64_t const*>(&c.dst.data()[0]);
    auto dst1 = *reinterpret_cast<uint64_t const*>(&c.dst.data()[8]);
    auto sprt = c.sport.number();
    auto dprt = c.dport.number();
    auto proto = static_cast<uint8_t>(c.sport.type());

    return vast::util::hash_combine(src0, src1, dst0, dst1, sprt, dprt, proto);
  }
};

} // namespace std

namespace vast {
namespace source {

/// A source that reads PCAP packets from an interface or a file.
class pcap : public synchronous<pcap>
{
public:
  /// Constructs a PCAP source.
  /// @param sch The schema containing the packet event.
  /// @param name The name of the interface or trace file.
  /// @param cutoff The number of bytes to keep per flow.
  /// @param max_flows The maximum number of flows to keep state for.
  /// @param max_age The number of seconds to wait since the last seen packet
  ///                before evicting the corresponding flow.
  /// @param expire_interval The number of seconds between successive expire
  ///                        passes over the flow table.
  pcap(schema sch,
       std::string name,
       uint64_t cutoff = -1,
       size_t max_flows = 100000,
       size_t max_age = 60,
       size_t expire_interval = 10);

  ~pcap();

  result<event> extract();
  bool done() const;
  std::string describe() const final;

private:
  struct connection_state
  {
    uint64_t bytes = 0;
    uint64_t last;
  };

  schema schema_;
  std::string name_;
  bool done_ = false;
  type packet_type_;
  pcap_t* pcap_ = nullptr;
  pcap_pkthdr* packet_header_ = nullptr;
  std::unordered_map<detail::connection, connection_state> flows_;
  uint64_t cutoff_;
  size_t max_flows_;
  std::mt19937 generator_;
  uint64_t max_age_;
  uint64_t expire_interval_;
  uint64_t last_expire_ = 0;
};

} // namespace source
} // namespace vast

#endif
