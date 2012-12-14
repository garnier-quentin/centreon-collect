/*
** Copyright 2011-2012 Merethis
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

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/mutex_posix.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 */
mutex::mutex() {
  // Return value.
  int ret;

  // Initialize mutex attributes.
  pthread_mutexattr_t mta;
  ret = pthread_mutexattr_init(&mta);
  if (ret)
    throw (basic_error() << "could not initialize mutex attributes: "
           << strerror(ret));

  // Set mutex as recursive.
  ret = pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
  if (ret)
    throw (basic_error() << "could not set mutex as recursive: "
           << strerror(ret));

  // Initialize mutex.
  ret = pthread_mutex_init(&_mtx, &mta);
  if (ret)
    throw (basic_error() << "could not initialize mutex: "
           << strerror(ret));
}

/**
 *  Destructor.
 */
mutex::~mutex() throw () {
  pthread_mutex_destroy(&_mtx);
}

/**
 *  Lock the mutex and if another thread has already locked the mutex
 *  then this call will block until the mutex has unlock by the first
 *  thread.
 */
void mutex::lock() {
  int ret(pthread_mutex_lock(&_mtx));
  if (ret)
    throw (basic_error() << "failed to lock mutex : " << strerror(ret));
  return ;
}

/**
 *  Lock the mutex if the mutex is unlock and return without any
 *  modification on the mutex if anobther thread has already locked the
 *  mutex.
 *
 *  @return true if the mutex is lock, false if the mutex was already
 *          locked.
 */
bool mutex::trylock() {
  int ret(pthread_mutex_trylock(&_mtx));
  if (ret && (ret != EBUSY))
    throw (basic_error() << "failed mutex "
           << "lock attempt: " << strerror(ret));
  return (!ret);
}

/**
 *  Unlock the mutex.
 */
void mutex::unlock() {
  int ret(pthread_mutex_unlock(&_mtx));
  if (ret)
    throw (basic_error() << "failed to unlock mutex "
           << strerror(ret));
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
 *  @param[in] right  The object to copy.
 */
mutex::mutex(mutex const& right) {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
mutex& mutex::operator=(mutex const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Calls abort().
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
void mutex::_internal_copy(mutex const& right) {
  (void)right;
  assert(!"mutex is not copyable");
  abort();
  return ;
}
