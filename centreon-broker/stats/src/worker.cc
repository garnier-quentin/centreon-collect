/*
** Copyright 2012 Merethis
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <iomanip>
#include <QMutexLocker>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/config/applier/modules.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/stats/worker.hh"

using namespace com::centreon::broker::stats;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
worker::worker() : _fd(-1) {}

/**
 *  Destructor.
 */
worker::~worker() throw () {}

/**
 *  Set the exit flag.
 */
void worker::exit() {
  _should_exit = true;
  return ;
}

/**
 *  Run the statistics thread.
 *
 *  @param[in] fifo_file Path to the FIFO file.
 */
void worker::run(QString const& fifo_file) {
  // Close FD.
  _close();

  // Set FIFO file.
  _fifo = fifo_file;

  // Set exit flag.
  _should_exit = false;

  // Launch thread.
  start();

  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
worker::worker(worker const& right) : QThread() {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
worker& worker::operator=(worker const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Close FIFO fd.
 */
void worker::_close() {
  if (_fd >= 0) {
    close(_fd);
    _fd = -1;
  }
  return ;
}

/**
 *  Generate statistics.
 */
void worker::_generate_stats() {
  // Modules.
  config::applier::modules&
    mod_applier(config::applier::modules::instance());
  for (config::applier::modules::iterator
         it = mod_applier.begin(),
         end = mod_applier.end();
       it != end;
       ++it) {
    _buffer.append("module ");
    _buffer.append(it.key().toStdString());
    _buffer.append("\nstate=loaded\n");
    _buffer.append("\n");
  }

  // Endpoint applier.
  config::applier::endpoint&
    endp_applier(config::applier::endpoint::instance());

  // Print input endpoints.
  {
    QMutexLocker lock(&endp_applier.input_mutex());
    for (config::applier::endpoint::iterator
           it = endp_applier.input_begin(),
           end = endp_applier.input_end();
         it != end;
         ++it) {
      _generate_stats_for_endpoint(*it, _buffer, false);
      _buffer.append("\n");
    }
  }

  // Print output endpoints.
  {
    QMutexLocker lock(&endp_applier.output_mutex());
    for (config::applier::endpoint::iterator
           it = endp_applier.output_begin(),
           end = endp_applier.output_end();
         it != end;
         ++it) {
      _generate_stats_for_endpoint(*it, _buffer, true);
      _buffer.append("\n");
    }
  }

  return ;
}

/**
 *  Generate statistics for an endpoint.
 *
 *  @param[in]  fo     Failover thread of the endpoint.
 *  @param[out] buffer Buffer in which data will be printed.
 *  @param[in]  is_out true if fo manage an output endpoint.
 */
void worker::_generate_stats_for_endpoint(
               processing::failover* fo,
               std::string& buffer,
               bool is_out) {
  // Header.
  buffer.append(is_out ? "output " : "input ");
  buffer.append(fo->_name.toStdString());
  buffer.append("\n");

  // Choose stream we will work on.
  QReadWriteLock* rwl;
  QSharedPointer<io::stream>* s;
  if (is_out) {
    rwl = &fo->_tom;
    s = &fo->_to;
  }
  else {
    rwl = &fo->_fromm;
    s = &fo->_from;
  }

  {
    // Get state.
    buffer.append("state=");
    QReadLocker rl(rwl);
    if (s->isNull()) {
      if (!fo->_last_error.isEmpty()) {
        buffer.append("disconnected");
        buffer.append(" (");
        buffer.append(fo->_last_error.toStdString());
        buffer.append(")\n");
      }
      else
        buffer.append("listening\n");
    }
    else if (!fo->_failover.isNull() && fo->_failover->isRunning())
      buffer.append("replaying\n");
    else
      buffer.append("connected\n");
  }

  {
    // Event processing stats.
    std::ostringstream oss;
    oss << "last event at=" << fo->get_last_event() << "\n"
        << "event processing speed=" << std::fixed
        << std::setprecision(2) << fo->get_event_processing_speed()
        << " events/s\n";
    buffer.append(oss.str());
  }

  // Endpoint stats.
  if (!fo->_endpoint.isNull())
    fo->_endpoint->stats(buffer);

  {
    // Last connection times.
    std::ostringstream oss;
    oss << "last connection attempt=" << fo->_last_connect_attempt
        << "\n" << "last connection success="
        << fo->_last_connect_success << "\n";
    buffer.append(oss.str());
  }

  // Failover.
  if (!fo->_failover.isNull() && fo->_failover->isRunning()) {
    buffer.append("failover\n");
    std::string subbuffer;
    _generate_stats_for_endpoint(
      fo->_failover.data(),
      subbuffer,
      is_out);
    subbuffer.insert(0, "  ");
    size_t pos(subbuffer.find('\n'));
    while ((pos != subbuffer.size() - 1)
           && (pos != std::string::npos)) {
      subbuffer.replace(pos, 1, "\n  ");
      pos = subbuffer.find('\n', pos + 3);
    }
    buffer.append(subbuffer);
  }

  return ;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void worker::_internal_copy(worker const& right) {
  (void)right;
  assert(!"statistics worker is not copyable");
  abort();
  return ;
}

/**
 *  Open FIFO.
 */
void worker::_open() {
  _fd = open(qPrintable(_fifo), O_WRONLY);
  if (_fd < 0) {
    char const* msg(strerror(errno));
    throw (exceptions::msg() << "cannot open FIFO file: " << msg);
  }
  return ;
}

/**
 *  Thread entry point.
 */
void worker::run() {
  try {
    // Initial FIFO open.
    _open();

    while (!_should_exit) {
      // FD sets.
      fd_set e;
      fd_set w;
      FD_ZERO(&e);
      FD_ZERO(&w);
      FD_SET(_fd, &e);
      FD_SET(_fd, &w);

      // Timeout.
      struct timeval timeout;
      memset(&timeout, 0, sizeof(timeout));
      timeout.tv_sec = 1;

      // Multiplexing.
      int flagged(select(_fd + 1, NULL, &w, &e, &timeout));

      // Error.
      if (flagged < 0) {
        // Unrecoverable.
        if (errno != EINTR) {
          char const* msg(strerror(errno));
          throw (exceptions::msg() << "multiplexing failure: " << msg);
        }
      }
      // FD error.
      else if (FD_ISSET(_fd, &e))
        throw (exceptions::msg() << "FIFO fd has pending error");
      // Readable.
      else if (FD_ISSET(_fd, &w)) {
        if (_buffer.empty())
          // Generate statistics.
          _generate_stats();

        // Write data.
        ssize_t wb(write(_fd, _buffer.c_str(), _buffer.size()));
        if (wb > 0)
          _buffer.erase(0, wb);

        // Buffer empty, reopen FIFO.
        if (_buffer.empty()) {
          _close();
          usleep(500);
          _open();
        }
      }
    }
  }
  catch (std::exception const& e) {
    logging::error(logging::high)
      << "stats: thread will exit due to the following error: "
      << e.what();
  }
  catch (...) {
    logging::error(logging::high)
      << "stats: thread will exit due to an unknown error";
  }
  return ;
}
