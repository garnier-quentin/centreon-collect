/*
** query.cpp for CentreonBroker in ./src/db
** 
** Made by Matthieu Kermagoret <mkermagoret@merethis.com>
** 
** Copyright Merethis
** See LICENSE file for details.
** 
** Started on  06/02/09 Matthieu Kermagoret
** Last update 06/09/09 Matthieu Kermagoret
*/

#include "db/query.h"

using namespace CentreonBroker::DB;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Query copy constructor.
 */
Query::Query(const Query& query) throw ()
{
  (void)query;
}

/**
 *  Query operator= overload.
 */
Query& Query::operator=(const Query& query) throw ()
{
  (void)query;
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Query default constructor.
 */
Query::Query() throw ()
{
}

/**
 *  Query destructor.
 */
Query::~Query()
{
}
