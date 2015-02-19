/*
** Copyright 2011-2013 Merethis
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

#ifndef CCB_COMPRESSION_FACTORY_HH
#  define CCB_COMPRESSION_FACTORY_HH

#  include "com/centreon/broker/io/factory.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace         compression {
  /**
   *  @class factory factory.hh "com/centreon/broker/compression/factory.hh"
   *  @brief Compression layer factory.
   *
   *  Build compression objects.
   */
  class           factory : public io::factory {
  public:
                  factory();
                  factory(factory const& f);
                  ~factory();
    factory&      operator=(factory const& f);
    io::factory*  clone() const;
    bool          has_endpoint(
                    config::endpoint& cfg,
                    bool is_input,
                    bool is_output) const;
    bool          has_not_endpoint(
                    config::endpoint& cfg,
                    bool is_input,
                    bool is_output) const;
    io::endpoint* new_endpoint(
                    config::endpoint& cfg,
                    bool is_input,
                    bool is_output,
                    bool& is_acceptor,
                    misc::shared_ptr<persistent_cache> cache = misc::shared_ptr<persistent_cache>()) const;
    misc::shared_ptr<io::stream>
                  new_stream(
                    misc::shared_ptr<io::stream> to,
                    bool is_acceptor,
                    QString const& proto_name);
  };
}

CCB_END()

#endif // !CCB_COMPRESSION_FACTORY_HH
