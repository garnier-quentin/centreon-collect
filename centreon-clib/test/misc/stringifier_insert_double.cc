/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <ios>
#include <sstream>
#include <string.h>
#include <float.h>
#include "com/centreon/misc/stringifier.hh"

using namespace com::centreon::misc;

/**
 *  Check the stringifier insert double.
 *
 *  @return 0 on success.
 */
int main() {
  stringifier buffer;
  buffer << static_cast<double>(DBL_MIN);
  buffer << static_cast<double>(DBL_MAX);

  std::ostringstream oss;
  oss << std::fixed << DBL_MIN << DBL_MAX;
  return (strcmp(buffer.data(), oss.str().c_str()));
}
