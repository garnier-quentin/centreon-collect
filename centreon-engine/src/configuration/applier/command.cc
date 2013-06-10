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

#include "com/centreon/engine/configuration/applier/command.hh"
#include "com/centreon/engine/configuration/applier/difference.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

/**
 *  Default constructor.
 */
applier::command::command() {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
applier::command::command(applier::command const& right) {
  (void)right;
}

/**
 *  Destructor.
 */
applier::command::~command() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
applier::command& applier::command::operator=(
                                      applier::command const& right) {
  (void)right;
  return (*this);
}

/**
 *  Add new command.
 *
 *  @param[in] obj The new command to add into the monitoring engine.
 */
void applier::command::add_object(command_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Creating new command '" << obj->command_name() << "'.";

  // Create command.
  shared_ptr<command_struct> c(new command_struct);
  memset(c.get(), 0, sizeof(*c));
  c->name = my_strdup(obj->command_name().c_str());
  c->command_line = my_strdup(obj->command_line().c_str());

  // Register command.
  c->next = command_list;
  applier::state::instance().commands()[obj->command_name()] = c;
  command_list = c.get();

  return ;
}

/**
 *  Modified command.
 *
 *  @param[in] obj The new command to modify into the monitoring engine.
 */
void applier::command::modify_object(command_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Modifying command '" << obj->command_name() << "'.";

  // Modify command.
  shared_ptr<command_struct>&
    c(applier::state::instance().commands()[obj->command_name()]);
  modify_if_different(c->command_line, obj->command_line().c_str());

  return ;
}

/**
 *  Remove old command.
 *
 *  @param[in] obj The new command to remove from the monitoring engine.
 */
void applier::command::remove_object(command_ptr obj) {
  // Logging.
  logger(logging::dbg_config, logging::more)
    << "Removing command '" << obj->command_name() << "'.";

  // Unregister command.
  for (command_struct** cs(&command_list); *cs; cs = &(*cs)->next)
    if (!strcmp((*cs)->name, obj->command_name().c_str())) {
      (*cs) = (*cs)->next;
      break ;
    }
  applier::state::instance().commands().erase(obj->command_name());

  return ;
}

/**
 *  @brief Resolve a command object.
 *
 *  This method does nothing, as command objects do not require
 *  resolution.
 *
 *  @param[in] obj Object to resolve.
 */
void applier::command::resolve_object(command_ptr obj) {
  (void)obj;
  return ;
}
