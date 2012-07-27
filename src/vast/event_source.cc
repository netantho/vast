#include "vast/event_source.h"

#include <ze/event.h>
#include <ze/util/parse_helpers.h>
#include "vast/exception.h"
#include "vast/logger.h"

namespace vast {

event_source::event_source(cppa::actor_ptr ingestor, cppa::actor_ptr tracker)
  : tracker_(tracker)
{
  LOG(verbose, ingest) << "spawning event source @" << id();

  using namespace cppa;
  chaining(false);
  init_state = (
      on(atom("extract"), arg_match) >> [=](size_t n)
      {
        if (finished_)
        {
          send(ingestor, atom("source"), atom("done"));
          return;
        }

        if (next_id_ == last_id_)
        {
          ask_for_new_ids(n);
          return;
        }

        std::vector<ze::event> events;
        events.reserve(n);
        size_t i = 0;
        while (i < n)
        {
          if (finished_)
            break;

          try
          {
            auto e = extract();
            assert(next_id_ != last_id_);
            e.id(next_id_++);
            events.push_back(std::move(e));
            ++i;
          }
          catch (error::parse const& e)
          {
            LOG(error, ingest) << e.what();
          }
        }

        if (! events.empty())
        {
          total_events_ += events.size();

          LOG(verbose, ingest)
            << "event source @" << id()
            << " sends " << events.size()
//            << " events to segmentizer @" << segmentizer_->id()
            << " (cumulative events: " << total_events_ << ')';

          // TODO: Spawn segmentizer.
          //send(segmentizer_, std::move(events));
        }

        send(ingestor, atom("source"), finished_ ? atom("done") : atom("ack"));
      },
      on(atom("shutdown")) >> [=]
      {
        quit();
        LOG(verbose, ingest) << "event source @" << id() << " terminated";
      });
}

void event_source::ask_for_new_ids(size_t n)
{
  DBG(ingest)
    << "event source @" << id()
    << " asks tracker @"  << tracker_->id()
    << " for " << n << " new ids";

  using namespace cppa;
  auto future = sync_send(tracker_, atom("request"), n);
  handle_response(future)(
      on(atom("id"), arg_match) >> [=](uint64_t lower, uint64_t upper)
      {
        DBG(ingest)
          << "event source @" << id() << " received id range from tracker: "
          << '[' << lower << ',' << upper << ')';

        next_id_ = lower;
        last_id_ = upper;

        send(self, atom("extract"), static_cast<size_t>(upper - lower));
      },
      after(std::chrono::seconds(5)) >> [=]
      {
        LOG(error, ingest)
          << "event source @" << id()
          << " did not receive new id range from tracker"
          << " after 5 seconds";
      });
}

} // namespace vast
