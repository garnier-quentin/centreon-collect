/*
** Copyright 2014 Merethis
**
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
*/

#ifndef CCB_BAM_BOOL_EXPRESSION_HH
#  define CCB_BAM_BOOL_EXPRESSION_HH

#  include "com/centreon/broker/bam/kpi.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace            bam {
  // Forward declaration.
  class              bool_value;

  /**
   *  @class bool_expression bool_expression.hh "com/centreon/broker/bam/bool_expression.hh"
   *  @brief Boolean expression.
   *
   *  Stores and entire boolean expression made of multiple boolean
   *  operations and evaluate them to match the kpi interface.
   */
  class              bool_expression : public kpi {
  public:
                     bool_expression();
                     bool_expression(bool_expression const& right);
                     ~bool_expression();
    bool_expression& operator=(bool_expression const& right);
    void             child_has_update(
                       misc::shared_ptr<computable>& child);
    void             impact_hard(impact_values& hard_impact);
    void             impact_soft(impact_values& soft_impact);
    void             set_expression(
                       misc::shared_ptr<bool_value> const& expression);
    void             set_impact_hard(double impact);
    void             set_impact_if(bool impact_if);
    void             set_impact_soft(double impact);

  private:
    void             _internal_copy(bool_expression const& right);

    misc::shared_ptr<bool_value>
                     _expression;
    bool             _impact_if;
    double           _impact_hard;
    double           _impact_soft;
  };
}

CCB_END()

#endif // !CCB_BAM_BOOL_EXPRESSION_HH
