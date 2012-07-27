#include "vast/configuration.h"

#include "config.h"

#include <iostream>
#include <string>
#include "vast/exception.h"
#include "vast/fs/path.h"
#include "vast/fs/fstream.h"
#include "vast/fs/operations.h"
#include "vast/logger.h"

namespace vast {

configuration::configuration()
  : visible_("")
  , all_("")
{
  po::options_description general("general options");
  general.add_options()
    ("config,c", po::value<fs::path>(), "configuration file")
    ("directory,d", po::value<fs::path>()->default_value("vast"),
     "VAST directory")
    ("help,h", "display this help")
    ("schema,s", po::value<std::string>(), "event schema file")
    ("query,q", po::value<std::string>(), "query expression")
    ("console-verbosity,v",
     po::value<int>()->default_value(logger::info),
     "console logging verbosity")
    ("advanced,z", "show advanced options")
    ;

  po::options_description advanced("advanced options");
  advanced.add_options()
    ("logfile-verbosity,V",
     po::value<int>()->default_value(logger::verbose),
     "log file verbosity")
    ("profile,P", po::value<unsigned>(), "getrusage profiling (seconds)")
#ifdef USE_PERFTOOLS_CPU_PROFILER
    ("perftools-cpu", "enable Google perftools CPU profiling")
#endif
#ifdef USE_PERFTOOLS_HEAP_PROFILER
    ("perftools-heap", "enable Google perftools heap profiling")
#endif
    ("broccoli-messages", "enable broccoli debug messages")
    ("broccoli-calltrace", "enable broccoli function call tracing")
    ;

  po::options_description actor("actor options");
  actor.add_options()
    ("ingestor-actor,I", "spawn the ingestor locally")
    ("archive-actor,A", "spawn the archive locally")
    ("index-actor,X", "spawn the index locally")
    ("search-actor,S", "spawn the search locally")
    ;

  po::options_description schema("schema options");
  schema.add_options()
    ("print-schema", "print the parsed event schema")
    ;

  po::options_description ingestor("ingestor options");
  ingestor.add_options()
    ("ingestor.host", po::value<std::string>()->default_value("127.0.0.1"),
     "IP address of the ingestor")
    ("ingestor.port", po::value<unsigned>()->default_value(42000),
     "port of the ingestor")
    ("ingestor.events", po::value<std::vector<std::string>>()->multitoken(),
     "explicit list of events to ingest")
    ("ingestor.file-type", po::value<std::string>()->default_value("bro1"),
     "file type of the file(s) to ingest")
    ("ingestor.file-names", po::value<std::vector<std::string>>()->multitoken(),
     "file(s) to ingest")
    ;

  po::options_description archive("archive options");
  archive.add_options()
    ("archive.host", po::value<std::string>()->default_value("127.0.0.1"),
     "IP address of the archive")
    ("archive.port", po::value<unsigned>()->default_value(42002),
     "port of the archive")
    ("archive.max-events-per-chunk", po::value<size_t>()->default_value(1000),
     "maximum number of events per chunk")
    ("archive.max-segment-size", po::value<size_t>()->default_value(1000),
     "maximum segment size in KB")
    ("archive.max-segments", po::value<size_t>()->default_value(500),
     "maximum number of segments to keep in memory")
    ;

  po::options_description index("index options");
  index.add_options()
    ("index.host", po::value<std::string>()->default_value("127.0.0.1"),
     "IP address of the index")
    ("index.port", po::value<unsigned>()->default_value(42003),
     "port of the index")
    ;

  po::options_description search("search options");
  search.add_options()
    ("search.host", po::value<std::string>()->default_value("127.0.0.1"),
     "IP address of the search")
    ("search.port", po::value<unsigned>()->default_value(42001),
     "port of the search")
    ;

  po::options_description client("client options");
  client.add_options()
    ("client.paginate,p", po::value<unsigned>()->default_value(10),
     "number of query results per page")
    ;

  all_.add(general).add(advanced).add(actor).add(schema)
    .add(ingestor).add(archive).add(index).add(search).add(client);

  visible_.add(general).add(actor);
}

void configuration::load(std::string const& filename)
{
  if (fs::exists(filename))
  {
    fs::ifstream ifs(filename);
    po::store(po::parse_config_file(ifs, all_), config_);
  }

  init();
}

void configuration::load(int argc, char *argv[])
{
  po::store(parse_command_line(argc, argv, all_), config_);

  if (check("config"))
  {
    fs::path const& cfg = get<fs::path>("conf");
    std::ifstream ifs(cfg.string().c_str());
    po::store(po::parse_config_file(ifs, all_), config_);
  }

  init();
}

bool configuration::check(char const* option) const
{
  return config_.count(option);
}

void configuration::print(std::ostream& out, bool advanced) const
{
  out << " _   _____   __________\n"
    "| | / / _ | / __/_  __/\n"
    "| |/ / __ |_\\ \\  / /\n"
    "|___/_/ |_/___/ /_/  " << VAST_VERSION << '\n'
    << (advanced ? all_ : visible_)
    << std::endl;
}

void configuration::init()
{
  po::notify(config_);

  depends("index-actor", "archive-actor");
  depends("print-schema", "schema");
  depends("ingestor.file-names", "ingestor.file-type");

  auto v = get<int>("console-verbosity");
  if (v < 0 || v > 6)
    throw error::config("verbosity not in [0,6]", "console-verbosity");

  v = get<int>("logfile-verbosity");
  if (v < 0 || v > 6)
    throw error::config("verbosity not in [0,6]", "log-verbosity");

  if (check("profile") && get<unsigned>("profile") == 0)
    throw error::config("profiling interval must be non-zero", "profile");

  if (get<unsigned>("client.paginate") == 0)
    throw error::config("pagination must be non-zero", "client.paginate");
}

void configuration::conflicts(const char* opt1, const char* opt2) const
{
  if (check(opt1) && ! config_[opt1].defaulted()
      && check(opt2) && ! config_[opt2].defaulted())
    throw error::config("conflicting options", opt1, opt2);
}

void configuration::depends(const char* for_what, const char* required) const
{
  if (check(for_what) && ! config_[for_what].defaulted() &&
      (! check(required) || config_[required].defaulted()))
    throw error::config("missing option dependency", for_what, required);
}

} // namespace vast
