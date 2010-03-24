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

#include "events/acknowledgement.h"

using namespace Events;

/**************************************
*                                     *
*          Private Methods            *
*                                     *
**************************************/

/**
 *  \brief Copy internal data of the given object to the current instance.
 *
 *  This internal method is used to copy data defined inside the
 *  Acknowledgement class from an object to the current instance. This means
 *  that no superclass data are copied. This method is used in Acknowledgement
 *  copy constructor and in the assignment operator overload.
 *
 *  \param[in] ack Object to copy from.
 *
 *  \see Acknowledgement(const Acknowledgement&)
 *  \see operator=(const Acknowledgement&)
 */
void Acknowledgement::InternalCopy(const Acknowledgement& ack)
{
  this->acknowledgement_type = ack.acknowledgement_type;
  this->author               = ack.author;
  this->comment              = ack.comment;
  this->entry_time           = ack.entry_time;
  this->host_name            = ack.host_name;
  this->instance_name        = ack.instance_name;
  this->is_sticky            = ack.is_sticky;
  this->notify_contacts      = ack.notify_contacts;
  this->persistent_comment   = ack.persistent_comment;
  this->service_description  = ack.service_description;
  this->state                = ack.state;
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  \brief Acknowledgement default constructor.
 *
 *  Acknowledgement default constructor. Set all members to their default value
 *  (0, NULL or equivalent).
 */
Acknowledgement::Acknowledgement()
  : acknowledgement_type(0),
    entry_time(0),
    is_sticky(false),
    notify_contacts(false),
    persistent_comment(false),
    state(0) {}

/**
 *  \brief Acknowledgement copy constructor.
 *
 *  Copy data from the Acknowledgement object to the current instance.
 *
 *  \param[in] ack Object to copy from.
 */
Acknowledgement::Acknowledgement(const Acknowledgement& ack) : Event(ack)
{
  this->InternalCopy(ack);
}

/**
 *  \brief Acknowledgement destructor.
 */
Acknowledgement::~Acknowledgement() {}

/**
 *  Overload of the assignment operator.
 *
 *  \param[in] ack Object to copy from.
 *
 *  \return *this
 */
Acknowledgement& Acknowledgement::operator=(const Acknowledgement& ack)
{
  this->Event::operator=(ack);
  this->InternalCopy(ack);
  return (*this);
}

/**
 *  \brief Get the type of the event.
 *
 *  Return the type of this event (Event::ACKNOWLEDGEMENT). This can be useful
 *  for runtime type determination.
 *
 *  \return Event::ACKNOWLEDGEMENT
 */
int Acknowledgement::GetType() const
{
  return (Event::ACKNOWLEDGEMENT);
}
