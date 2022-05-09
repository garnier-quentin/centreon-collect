/*
** Copyright 2011-2013, 2020-2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <rrd.h>

#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "bbdo/storage/rebuild.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/rrd/factory.hh"
#include "com/centreon/broker/rrd/internal.hh"

using namespace com::centreon::broker;

// Load count.
static uint32_t instances(0);

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
char const* broker_module_version = CENTREON_BROKER_VERSION;

/**
 * @brief Return an array with modules needed for this one to work.
 *
 * @return An array of const char*
 */
const char* const* broker_module_parents() {
  constexpr static const char* retval[]{"10-neb.so", nullptr};
  return retval;
}

/**
 *  Module deinitialization routine.
 */
void broker_module_deinit() {
  // Decrement instance number.
  if (!--instances)
    // Deregister RRD layer.
    io::protocols::instance().unreg("RRD");
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  // Increment instance number.
  if (!instances++) {
    // RRD module.
    log_v2::rrd()->info("RRD: module for Centreon Broker {}",
                        CENTREON_BROKER_VERSION);

    // Print RRDtool version.
    char const* rrdversion(rrd_strversion());
    log_v2::rrd()->info("RRD: using rrdtool {}",
                        (rrdversion ? rrdversion : "(unknown)"));

    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::storage, storage::de_metric), "metric",
                       &storage::metric::operations, storage::metric::entries,
                       "rt_metrics");
      e.register_event(make_type(io::storage, storage::de_rebuild), "rebuild",
                       &storage::rebuild::operations,
                       storage::rebuild::entries);
      e.register_event(make_type(io::storage, storage::de_remove_graph),
                       "remove_graph", &storage::remove_graph::operations,
                       storage::remove_graph::entries);
      e.register_event(make_type(io::storage, storage::de_status), "status",
                       &storage::status::operations, storage::status::entries);
      e.register_event(make_type(io::storage, storage::de_index_mapping),
                       "index_mapping", &storage::index_mapping::operations,
                       storage::index_mapping::entries);
      e.register_event(make_type(io::storage, storage::de_metric_mapping),
                       "metric_mapping", &storage::metric_mapping::operations,
                       storage::metric_mapping::entries);

      /* Let's register the message to start rebuilds, send rebuilds and
       * terminate rebuilds. This is pb_rebuild_message. */
      e.register_event(make_type(io::storage, storage::de_rebuild_message),
                       "rebuild_message",
                       &storage::pb_rebuild_message::operations);

      /* Let's register the message to ask rrd for remove metrics. This is
       * pb_remove_graph_message. */
      e.register_event(make_type(io::storage, storage::de_remove_graph_message),
                       "remove_graphs_message",
                       &storage::pb_remove_graph_message::operations);
    }

    // Register RRD layer.
    io::protocols::instance().reg("RRD", std::make_shared<rrd::factory>(), 1,
                                  7);
  }
}
}
