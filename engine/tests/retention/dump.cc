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

#include "com/centreon/engine/retention/dump.hh"
#include <gtest/gtest.h>

#include "com/centreon/engine/exceptions/error.hh"
#include "helper.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::retention;

class RetentionDumpTest : public ::testing::Test {
 public:
  void SetUp() { init_config_state(); }

  void TearDown() { deinit_config_state(); }
};

TEST_F(RetentionDumpTest, DumpComment) {
  comment cmt(comment::host, comment::flapping, 12, 0, time(nullptr),
              "Test comment", "test comment", false, comment::internal, false,
              0);
  std::ostringstream oss;
  dump::comments(oss);
  std::string str(oss.str());
  ASSERT_EQ(str, "");
}

TEST_F(RetentionDumpTest, DumpContact) {
  std::ostringstream oss;
  dump::contacts(oss);
  std::string str(oss.str());
  ASSERT_EQ(str, "");
}

TEST_F(RetentionDumpTest, DumpDowntime) {
  std::ostringstream oss;
  dump::downtimes(oss);
  std::string str(oss.str());
  ASSERT_EQ(str, "");
}

TEST_F(RetentionDumpTest, DumpHeader) {
  std::ostringstream oss;
  dump::header(oss);
  std::string str(oss.str());
  ASSERT_EQ(str,
            "##############################################\n"
            "#    CENTREON ENGINE STATE RETENTION FILE    #\n"
            "#                                            #\n"
            "# THIS FILE IS AUTOMATICALLY GENERATED BY    #\n"
            "# CENTREON ENGINE. DO NOT MODIFY THIS FILE ! #\n"
            "##############################################\n");
}

TEST_F(RetentionDumpTest, DumpHost) {
  std::ostringstream oss;
  dump::hosts(oss);
  std::string str(oss.str());
  ASSERT_EQ(
      str,
      "host "
      "{\nhost_name=test_host1\nhost_id=12\nacknowledgement_type=0\nactive_"
      "checks_enabled=1\ncheck_command=\ncheck_execution_time=0.000\ncheck_"
      "flapping_recovery_notification=0\ncheck_latency=0.000\ncheck_options="
      "0\ncheck_period=\ncheck_type=0\ncurrent_attempt=1\ncurrent_event_id="
      "0\ncurrent_notification_id=0\ncurrent_notification_number=0\ncurrent_"
      "problem_id=0\ncurrent_state=0\nevent_handler=\nevent_handler_enabled="
      "1\nflap_detection_enabled=1\nhas_been_checked=0\nis_flapping=0\nlast_"
      "acknowledgement=0\nlast_check=0\nlast_event_id=0\nlast_hard_state="
      "0\nlast_hard_state_change=0\nlast_notification=0\nlast_problem_id="
      "0\nlast_state=0\nlast_state_change=0\nlast_time_down=0\nlast_time_"
      "unreachable=0\nlast_time_up=0\nlong_plugin_output=\nmax_attempts="
      "3\nmodified_attributes=0\nnext_check=0\nnormal_check_interval=5."
      "000\nnotification_period=\nnotifications_enabled=1\nnotified_on_down="
      "0\nnotified_on_unreachable=0\nobsess_over_host=1\npassive_checks_"
      "enabled=1\npercent_state_change=0.00\nperformance_data=\nplugin_output="
      "\nproblem_has_been_acknowledged=0\nprocess_performance_data=1\nretry_"
      "check_interval=5.00\nstate_type=1\nrecovery_been_sent=0\nstate_history="
      "0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0\n}\n");
}

TEST_F(RetentionDumpTest, DumpInfo) {
  std::ostringstream oss;
  dump::info(oss);
  std::string str(oss.str());
  ASSERT_EQ(str, "info {\ncreated=18446744073709551615\n}\n");
}

TEST_F(RetentionDumpTest, DumpProgram) {
  std::ostringstream oss;
  dump::program(oss);
  std::string str(oss.str());
  ASSERT_EQ(
      str,
      "program "
      "{\nactive_host_checks_enabled=1\nactive_service_checks_enabled=1\ncheck_"
      "host_freshness=0\ncheck_service_freshness=1\nenable_event_handlers="
      "1\nenable_flap_detection=0\nenable_notifications=1\nglobal_host_event_"
      "handler=\nglobal_service_event_handler=\nmodified_host_attributes="
      "0\nmodified_service_attributes=0\nnext_comment_id=0\nnext_event_id="
      "1\nnext_notification_id=1\nnext_problem_id=0\nobsess_over_hosts="
      "0\nobsess_over_services=0\npassive_host_checks_enabled=1\npassive_"
      "service_checks_enabled=1\nprocess_performance_data=0\n}\n");
}

TEST_F(RetentionDumpTest, DumpService) {
  std::ostringstream oss;
  dump::services(oss);
  std::string str(oss.str());
  ASSERT_EQ(str, "");
}
