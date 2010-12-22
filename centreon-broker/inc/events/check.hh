/*
** Copyright 2009-2010 MERETHIS
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
**
** For more information: contact@centreon.com
*/

#ifndef EVENTS_CHECK_HH_
# define EVENTS_CHECK_HH_

# include <string>
# include "events/event.hh"

namespace       events {
  /**
   *  @class check check.hh "events/check.hh"
   *  @brief Check that has been executed.
   *
   *  Once a check has been executed (the check itself, not deduced
   *  information), this kind of event is sent.
   *
   *  @see host_check
   *  @see service_check
   */
  class         check : public event {
   private:
    void        _internal_copy(check const& c);

   public:
    std::string command_line;
    int         host_id;
                check();
                check(check const& c);
    virtual     ~check();
    check&      operator=(check const& c);
  };
}

#endif /* !EVENTS_CHECK_H_ */
