/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
#  define CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH

#  include "com/centreon/engine/configuration/applier/base.hh"
#  include "com/centreon/engine/configuration/applier/object.hh"
#  include "com/centreon/engine/configuration/hostdependency.hh"
#  include "com/centreon/engine/namespace.hh"

CCE_BEGIN()

namespace                    configuration {
  namespace                  applier {
    class                    hostdependency
      : public base,
        public object<configuration::hostdependency> {
    public:
      void                   apply(state const& config);
      static hostdependency& instance();
      static void            load();
      static void            unload();

    private:
                             hostdependency();
                             hostdependency(hostdependency const&);
                             ~hostdependency() throw ();
      hostdependency&        operator=(hostdependency const&);
      void                   _add_object(hostdependency_ptr obj);
      void                   _modify_object(hostdependency_ptr obj);
      void                   _remove_object(hostdependency_ptr obj);
    };
  }
}

CCE_END()

#endif // !CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
