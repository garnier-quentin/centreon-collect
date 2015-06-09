/*
** Copyright 2011-2013,2015 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QTimer>
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/exceptions/shutdown.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/multiplexing/subscriber.hh"
#include "com/centreon/broker/processing/failover.hh"
#include "test/processing/feeder/common.hh"
#include "test/processing/failover/setable_endpoint.hh"

using namespace com::centreon::broker;

/**
 *  Check that simple event feeding works properly.
 *
 *  @param[in] argc Arguments count.
 *  @param[in] argv Arguments values.
 *
 *  @return 0 on success.
 */
int main(int argc, char* argv[]) {
  // Qt core application.
  QCoreApplication app(argc, argv);

  // Initialization.
  config::applier::init();
  multiplexing::engine::instance().start();

  // Log messages.
  if (argc > 1)
    log_on_stderr();

  // Endpoint.
  misc::shared_ptr<setable_endpoint> se(new setable_endpoint);
  se->set_succeed(true);

  // Subscriber.
  misc::shared_ptr<multiplexing::subscriber>
    s(new multiplexing::subscriber("processing_failover_feed", ""));

  // Failover object.
  processing::failover f(
                         se.staticCast<io::endpoint>(),
                         s,
                         "processing_failover_feed_1",
                         "");

  // Launch thread.
  f.start();

  // Wait some time.
  QTimer::singleShot(5000, &app, SLOT(quit()));
  app.exec();

  // Quit feeder thread.
  f.exit();

  // Wait for thread termination.
  f.wait();

  // Check output content.
  int retval(se->streams().isEmpty());
  if (!retval) {
    misc::shared_ptr<setable_stream> ss(*se->streams().begin());
    unsigned int count(ss->get_count());
    unsigned int i(0);
    misc::shared_ptr<io::data> event;
    s->get_muxer().read(event, 0);
    while (!event.isNull()) {
      if (event->type() != io::events::data_type<io::events::internal, 1>::value)
        retval |= 1;
      else {
        misc::shared_ptr<io::raw> raw(event.staticCast<io::raw>());
        unsigned int val;
        memcpy(&val, raw->QByteArray::data(), sizeof(val));
        retval |= (val != ++i);
      }
      try {
        s->get_muxer().read(event, 0);
      }
      catch (io::exceptions::shutdown const& e) {
	event.clear();
      }
    }
    retval |= (i != count);
  }

  // Cleanup.
  config::applier::deinit();

  // Return check result.
  return (retval);
}
