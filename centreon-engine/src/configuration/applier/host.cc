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

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/host.hh"
#include "com/centreon/engine/configuration/applier/difference.hh"
#include "com/centreon/engine/configuration/applier/member.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"

using namespace com::centreon::engine::configuration;

static applier::host* _instance = NULL;

// XXX : update the event_list_low and event_list_high

/**
 *  Apply new configuration.
 *
 *  @param[in] config The new configuration.
 */
void applier::host::apply(configuration::state const& config) {
  _diff(::config->hosts(), config.hosts());
}

/**
 *  Get the singleton instance of host applier.
 *
 *  @return Singleton instance.
 */
applier::host& applier::host::instance() {
  return (*_instance);
}

/**
 *  Load host applier singleton.
 */
void applier::host::load() {
  if (!_instance)
    _instance = new applier::host;
}

/**
 *  Unload host applier singleton.
 */
void applier::host::unload() {
  delete _instance;
  _instance = NULL;
}

/**
 *  Default constructor.
 */
applier::host::host() {

}

/**
 *  Destructor.
 */
applier::host::~host() throw () {

}

/**
 *  Add new host.
 *
 *  @param[in] obj The new host to add into the monitoring engine.
 */
void applier::host::_add_object(host_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Creating new host '" << obj->host_name() << "'.";
}

/**
 *  Modified host.
 *
 *  @param[in] obj The new host to modify into the monitoring engine.
 */
void applier::host::_modify_object(host_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Modifying host '" << obj->host_name() << "'.";

}

/**
 *  Remove old host.
 *
 *  @param[in] obj The new host to remove from the monitoring engine.
 */
void applier::host::_remove_object(host_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Removing host '" << obj->host_name() << "'.";

  // Unregister host.
  unregister_object<host_struct, &host_struct::name>(
    &host_list,
    obj->host_name().c_str());
  applier::state::instance().hosts().erase(obj->host_name());
}
