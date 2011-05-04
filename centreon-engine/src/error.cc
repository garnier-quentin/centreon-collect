/*
** Copyright 2011      Merethis
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

#include <stdio.h>
#include <string.h>
#include "error.hh"

#undef error

using namespace com::centreon::engine;

/**
 *  Insert data in buffer using snprintf.
 *
 *  @param[in] t      Object to stringify.
 *  @param[in] format Format used by snprintf to stringify argument.
 *                    This format must be terminated by a %n.
 */
template <typename T>
void error::_insert_with_snprintf(T t, char const* format) {
  int wc;
  if (snprintf(_buffer + _current,
               sizeof(_buffer) / sizeof(*_buffer) - _current,
               format,
               t,
               &wc) > 0)
    _current += wc;
}

/**
 *  Constructor.
 */
#ifdef NDEBUG
error::error() throw () : _current(0), _fatal(true) {}
#else
error::error(char const* file, char const* function, int line) throw()
  : _current(0), _fatal(true) {
  *this << "[" << file << ":" << line << "(" << function << ")] ";
}
#endif // !NDEBUG

/**
 *  Copy constructor.
 *
 *  @param[in] e Object to copy.
 */
error::error(error const& e) throw ()
  : std::exception(e),
    _current(e._current),
    _fatal(e._fatal) {
  memcpy(_buffer, e._buffer, _current * sizeof(*_buffer));
}

/**
 *  Destructor.
 */
error::~error() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] e Object to copy.
 *
 *  @return This object.
 */
error& error::operator=(error const& e) throw () {
  std::exception::operator=(e);
  _current = e._current;
  memcpy(_buffer, e._buffer, _current * sizeof(*_buffer));
  _fatal = e._fatal;
  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] c Char to add to error message.
 *
 *  @return This object.
 */
error& error::operator<<(char c) throw () {
  char buffer[2];
  buffer[0] = c;
  buffer[1] = '\0';
  return (operator<<(buffer));
}

/**
 *  Insertion operator.
 *
 *  @param[in] str String to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(char const* str) throw () {
  // Detect NULL string.
  if (!str)
    str = "(null)";

  // Compute maximum number of bytes to append.
  unsigned int to_copy = strlen(str);
  unsigned int rem = sizeof(_buffer) / sizeof(*_buffer) - _current - 1;
  if (rem < to_copy)
    to_copy = rem;

  // Data copy.
  memcpy(_buffer + _current, str, to_copy * sizeof(*_buffer));
  _current += to_copy;

  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] i Integer to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(int i) throw () {
  _insert_with_snprintf(i, "%d%n");
  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] u Unsigned integer to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(unsigned int u) throw () {
  _insert_with_snprintf(u, "%u%n");
  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] u Long integer to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(long l) throw () {
  _insert_with_snprintf(l, "%l%n");
  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] u Lon lon integer to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(long long ll) throw () {
  _insert_with_snprintf(ll, "%ll%n");
  return (*this);
}

/**
 *  Insertion operator.
 *
 *  @param[in] u Unsigned long long integer to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(unsigned long long ull) throw () {
  _insert_with_snprintf(ull, "%llu%n");
  return (*this);
}

/**
 *  Get the fatal flag.
 *
 *  @return true if the error was generated by an unrecoverable error.
 */
bool error::is_fatal() const throw () {
  return (_fatal);
}

/**
 *  @brief Set the fatal flag.
 *
 *  By default the fatal flag is set to true.
 *
 *  @param[in] fatal true if the error was generated by an unrecoverable
 *                   error.
 */
void error::set_fatal(bool fatal) throw () {
  _fatal = fatal;
  return ;
}

/**
 *  Insertion operator.
 *
 *  @param[in] str String to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(std::string const& str) throw () {
  return (operator<<(str.c_str()));
}

/**
 *  Insertion operator.
 *
 *  @param[in] str String to concatenate to error message.
 *
 *  @return This object.
 */
error& error::operator<<(QString const& str) throw () {
  return (operator<<(str.toStdString().c_str()));
}

/**
 *  Get the error message.
 *
 *  @return Error message.
 */
char const* error::what() const throw () {
  _buffer[_current] = '\0';
  return (_buffer);
}
