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

#include <memory>
#include "initial.h"
#include "events/host.h"
#include "events/service.h"
#include "multiplexing/publisher.h"
#include "nagios/objects.h"

// Extern global sender.
extern Multiplexing::Publisher gl_publisher;

// Internal Nagios host list.
extern "C"
{
  extern host* host_list;
  extern service* service_list;
}

/**
 *  Send to the global publisher the list of host within Nagios.
 */
void SendHostList()
{
  for (host* h = host_list; h != NULL; h = h->next)
    {
      std::auto_ptr<Events::Host> my_host(new Events::Host);

      // my_host->acknowledgement_type = XXX;
      my_host->action_url = h->action_url;
      my_host->active_checks_enabled = h->checks_enabled;
      my_host->address = h->address;
      my_host->alias = h->alias;
      my_host->check_freshness = h->check_freshness;
      my_host->check_command = h->host_check_command;
      my_host->check_interval = h->check_interval;
      my_host->check_period = h->check_period;
      // my_host->check_type = XXX;
      // my_host->current_check_attempt = XXX;
      // my_host->current_notification_number = XXX;
      // my_host->current_state = XXX;
      my_host->display_name = h->display_name;
      my_host->event_handler = h->event_handler;
      my_host->event_handler_enabled = h->event_handler_enabled;
      // my_host->execution_time = XXX;
      my_host->failure_prediction_enabled = h->failure_prediction_enabled;
      my_host->first_notification_delay = h->first_notification_delay;
      my_host->flap_detection_enabled = h->flap_detection_enabled;
      my_host->flap_detection_on_down = h->flap_detection_on_down;
      my_host->flap_detection_on_unreachable
        = h->flap_detection_on_unreachable;
      my_host->flap_detection_on_up = h->flap_detection_on_up;
      my_host->freshness_threshold = h->freshness_threshold;
      // my_host->has_been_checked = XXX;
      my_host->have_2d_coords = h->have_2d_coords;
      my_host->high_flap_threshold = h->high_flap_threshold;
      my_host->host = h->name;
      // my_host->host_id = XXX;
      my_host->icon_image = h->icon_image;
      my_host->icon_image_alt = h->icon_image_alt;
      // my_host->is_flapping = XXX;
      // my_host->last_check = XXX;
      // my_host->last_hard_state = XXX;
      // my_host->last_hard_state_change = XXX;
      // my_host->last_notification = XXX;
      // my_host->last_state_change = XXX;
      // my_host->last_time_down = XXX;
      // my_host->last_time_unreachable = XXX;
      // my_host->last_time_up = XXX;
      // my_host->last_update = XXX;
      // my_host->latency = XXX;
      // my_host->long_output = XXX;
      my_host->low_flap_threshold = h->low_flap_threshold;
      my_host->max_check_attempts = h->max_attempts;
      // my_host->modified_attributes = XXX;
      // my_host->next_check = XXX;
      // my_host->next_notification = XXX;
      // my_host->no_more_notifications = XXX;
      my_host->notes = h->notes;
      my_host->notes_url = h->notes_url;
      my_host->notification_interval = h->notification_interval;
      my_host->notification_period = h->notification_period;
      my_host->notifications_enabled = h->notifications_enabled;
      my_host->notify_on_down = h->notify_on_down;
      my_host->notify_on_downtime = h->notify_on_downtime;
      my_host->notify_on_flapping = h->notify_on_flapping;
      my_host->notify_on_recovery = h->notify_on_recovery;
      my_host->notify_on_unreachable = h->notify_on_unreachable;
      my_host->obsess_over = h->obsess_over_host;
      // my_host->output = XXX;
      my_host->passive_checks_enabled = h->accept_passive_host_checks;
      // my_host->percent_state_change = XXX;
      // my_host->perf_data = XXX;
      // my_host->problem_has_been_acknowledged = XXX;
      my_host->process_performance_data = h->process_performance_data;
      my_host->retain_nonstatus_information = h->retain_nonstatus_information;
      my_host->retain_status_information = h->retain_status_information;
      my_host->retry_interval = h->retry_interval;
      // my_host->scheduled_downtime_depth = XXX;
      // my_host->should_be_scheduled = XXX;
      my_host->stalk_on_down = h->stalk_on_down;
      my_host->stalk_on_unreachable = h->stalk_on_unreachable;
      my_host->stalk_on_up = h->stalk_on_up;
      // my_host->state_type = XXX;
      my_host->statusmap_image = h->statusmap_image;
      my_host->vrml_image = h->vrml_image;
      my_host->x_2d = h->x_2d;
      my_host->y_2d = h->y_2d;

      my_host->AddReader();
      gl_publisher.Event(my_host.get());
      my_host.release();
    }
  return ;
}

/**
 *  Send to the global publisher the list of services within Nagios.
 */
void SendServiceList()
{
  for (service* s = service_list; s != NULL; s = s->next)
    {
      std::auto_ptr<Events::Service> my_service(new Events::Service);

      // my_service->acknowledgement_type = XXX;
      my_service->action_url = s->action_url;
      my_service->active_checks_enabled = s->checks_enabled;
      my_service->check_command = s->service_check_command;
      my_service->check_freshness = s->check_freshness;
      my_service->check_interval = s->check_interval;
      my_service->check_period = s->check_period;
      // my_service->check_type = XXX;
      // my_service->current_check_attempt = XXX;
      // my_service->current_notification_number = XXX;
      // my_service->current_state = XXX;
      my_service->display_name = s->display_name;
      my_service->event_handler = s->event_handler;
      my_service->event_handler_enabled = s->event_handler_enabled;
      // my_service->execution_time = XXX;
      my_service->failure_prediction_enabled = s->failure_prediction_enabled;
      my_service->failure_prediction_options = s->failure_prediction_options;
      my_service->first_notification_delay = s->first_notification_delay;
      my_service->flap_detection_enabled = s->flap_detection_enabled;
      my_service->flap_detection_on_critical = s->flap_detection_on_critical;
      my_service->flap_detection_on_ok = s->flap_detection_on_ok;
      my_service->flap_detection_on_unknown = s->flap_detection_on_unknown;
      my_service->flap_detection_on_warning = s->flap_detection_on_warning;
      my_service->freshness_threshold = s->freshness_threshold;
      // my_service->has_been_checked = XXX;
      my_service->high_flap_threshold = s->high_flap_threshold;
      my_service->host = s->host_name;
      // my_service->host_id = XXX;
      my_service->icon_image = s->icon_image;
      my_service->icon_image_alt = s->icon_image_alt;
      // my_service->is_flapping = XXX;
      my_service->is_volatile = s->is_volatile;
      // my_service->last_check = XXX;
      // my_service->last_hard_state = XXX;
      // my_service->last_hard_state_change = XXX;
      // my_service->last_notification = XXX;
      // my_service->last_state_change = XXX;
      // my_service->last_time_critical = XXX;
      // my_service->last_time_ok = XXX;
      // my_service->last_time_unknown = XXX;
      // my_service->last_time_warning = XXX;
      // my__service->last_update = XXX;
      // my_service->latency = XXX;
      // my_service->long_output = XXX;
      my_service->low_flap_threshold = s->low_flap_threshold;
      my_service->max_check_attempts = s->max_attempts;
      // my_service->modified_attributes = XXX;
      // my_service->next_check = XXX;
      // my_service->next_notification = XXX;
      // my_service->no_more_notifications = XXX;
      my_service->notes = s->notes;
      my_service->notes_url = s->notes_url;
      my_service->notification_interval = s->notification_interval;
      my_service->notification_period = s->notification_period;
      my_service->notifications_enabled = s->notifications_enabled;
      my_service->notified_on_critical = s->notify_on_critical;
      my_service->notified_on_unknown = s->notify_on_unknown;
      my_service->notified_on_warning = s->notify_on_warning;
      my_service->notify_on_downtime = s->notify_on_downtime;
      my_service->notify_on_flapping = s->notify_on_flapping;
      my_service->notify_on_recovery = s->notify_on_recovery;
      my_service->obsess_over = s->obsess_over_service;
      // my_service->output = XXX;
      my_service->passive_checks_enabled = s->accept_passive_service_checks;
      // my_service->percent_state_change = XXX;
      // my_service->perf_data = XXX;
      // my_service->problem_has_been_acknowledged = XXX;
      my_service->process_performance_data = s->process_performance_data;
      my_service->retain_nonstatus_information
        = s->retain_nonstatus_information;
      my_service->retain_status_information = s->retain_status_information;
      my_service->retry_interval = s->retry_interval;
      // my_service->scheduled_downtime_depth = XXX;
      my_service->service = s->description;
      // my_service->service_id = XXX;
      // my_service->should_be_scheduled = XXX;
      my_service->stalk_on_critical = s->stalk_on_critical;
      my_service->stalk_on_ok = s->stalk_on_ok;
      my_service->stalk_on_unknown = s->stalk_on_unknown;
      my_service->stalk_on_warning = s->stalk_on_warning;
      // my_service->state_type = XXX;

      my_service->AddReader();
      gl_publisher.Event(my_service.get());
      my_service.release();
    }
  return ;
}
