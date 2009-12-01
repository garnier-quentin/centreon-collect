/*
**  Copyright 2009 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#include "concurrency/lock.h"
#include "events/event.h"

using namespace Events;

/**************************************
*                                     *
*          Protected Methods          *
*                                     *
**************************************/

/**
 *  \brief Event copy constructor.
 *
 *  Copy data from the given object to the current instance.
 *
 *  \param[in] event Object to copy from.
 */
Event::Event(const Event& event)
{
  this->instance = event.instance;
  this->readers_ = 0;
}

/**
 *  \brief Overload of the assignment operator.
 *
 *  Copy the Nagios instance name of the given object to the current instance.
 *
 *  \param[in] event Object to copy from.
 *
 *  \return *this
 */
Event& Event::operator=(const Event& event)
{
  this->instance = event.instance;
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Event default constructor.
 */
Event::Event() : readers_(0) {}

/**
 *  Event destructor.
 */
Event::~Event() {}

/**
 *  \brief Add a reader to the event.
 *
 *  Specify that somebody is reading the Event. It shall not be destructed
 *  until the reader specify that he's done with the event.
 *
 *  For now, AddReader() only counts the number of times it has been called.
 *
 *  \see RemoveReader
 *
 *  \param[in] s Ignored.
 */
void Event::AddReader(Multiplexing::Subscriber* s)
{
  Concurrency::Lock lock(this->mutex_);

  (void)s;
  ++this->readers_;
  return ;
}

/**
 *  \brief Remove a reader from the event.
 *
 *  Remove an event reader. The current implementation only counts the number
 *  of calls made to AddReader and RemoveReader and if the numbers equal, the
 *  object self-destructs.
 *
 *  \see AddReader
 *
 *  \param[in] s Ignored.
 */
void Event::RemoveReader(const Multiplexing::Subscriber* s)
{
  bool destroy;

  (void)s;
  this->mutex_.Lock();
  if (--this->readers_ <= 0)
    destroy = true;
  else
    destroy = false;
  this->mutex_.Unlock();
  if (destroy)
    delete (this);
  return ;
}
