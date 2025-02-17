/**
 * Copyright 2022-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <google/protobuf/util/time_util.h>
#include <sys/types.h>
#include <future>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include <absl/strings/str_join.h>

#include <spdlog/common.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <rapidjson/document.h>

#include "com/centreon/common/process_stat.hh"
#include "com/centreon/common/time.hh"
#include "com/centreon/engine/host.hh"

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/commands.hh"
#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_finder.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/engine_impl.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/hostdependency.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/servicedependency.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "com/centreon/engine/statistics.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/version.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::downtimes;

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::engine {

std::ostream& operator<<(std::ostream& str, const HostIdentifier& host_id) {
  switch (host_id.identifier_case()) {
    case HostIdentifier::kName:
      str << "host name=" << host_id.name();
      break;
    case HostIdentifier::kId:
      str << "host id=" << host_id.id();
      break;
    default:
      str << " host nor id nor name";
  }
  return str;
}

std::ostream& operator<<(std::ostream& str, const ServiceIdentifier& serv_id) {
  switch (serv_id.identifier_case()) {
    case ServiceIdentifier::kNames:
      str << "host name=" << serv_id.names().host_name()
          << " serv name=" << serv_id.names().service_name();
      break;
    case ServiceIdentifier::kIds:
      str << "host id=" << serv_id.ids().host_id()
          << " serv id=" << serv_id.ids().service_id();
      break;
    default:
      str << " serv nor id nor name";
  }
  return str;
}

}  // namespace com::centreon::engine

namespace fmt {
template <>
struct formatter<HostIdentifier> : ostream_formatter {};
template <>
struct formatter<ServiceIdentifier> : ostream_formatter {};

}  // namespace fmt

/**
 * @brief Return the Engine's version.
 *
 * @param context gRPC context
 * @param  unused
 * @param response A Version object to fill
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetVersion(
    grpc::ServerContext* /*context*/,
    const ::google::protobuf::Empty* /*request*/,
    Version* response) {
  response->set_major(CENTREON_ENGINE_VERSION_MAJOR);
  response->set_minor(CENTREON_ENGINE_VERSION_MINOR);
  response->set_patch(CENTREON_ENGINE_VERSION_PATCH);
  return grpc::Status::OK;
}

grpc::Status engine_impl::GetStats(grpc::ServerContext* context
                                   [[maybe_unused]],
                                   const GenericString* request
                                   [[maybe_unused]],
                                   Stats* response) {
  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::get_stats, &command_manager::instance(),
                request->str_arg(), response));
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::StatusCode::UNKNOWN, "Unknown error");
}

grpc::Status engine_impl::ProcessServiceCheckResult(grpc::ServerContext* context
                                                    [[maybe_unused]],
                                                    const Check* request,
                                                    CommandSuccess* response
                                                    [[maybe_unused]]) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string const& svc_desc = request->svc_desc();
  if (svc_desc.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "svc_desc must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_service_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, svc_desc, request->code(), request->output()));
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessHostCheckResult(grpc::ServerContext* context
                                                 [[maybe_unused]],
                                                 const Check* request,
                                                 CommandSuccess* response
                                                 [[maybe_unused]]) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_host_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, request->code(), request->output()));
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

/**
 * @brief When a new file arrives on the centreon server, this command is used
 * to notify engine to update its anomaly detection services with those new
 * thresholds. The update is done by the main loop thread because of concurrent
 * accesses.
 *
 * @param
 * @param request The Protobuf message containing the full name of the
 *                thresholds file.
 * @param
 *
 * @return A grpc::Status::OK if the file is well read, an error otherwise.
 */
grpc::Status engine_impl::NewThresholdsFile(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const ThresholdsFile* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  const std::string& filename = request->filename();
  auto fn = std::packaged_task<int(void)>(
      std::bind(&anomalydetection::update_thresholds, filename));
  command_manager::instance().enqueue(std::move(fn));
  return grpc::Status::OK;
}

/**
 * @brief Return host informations.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response The filled fields
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetHost(grpc::ServerContext* context [[maybe_unused]],
                                  const HostIdentifier* request
                                  [[maybe_unused]],
                                  EngineHost* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, host = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::host> selectedhost;
        std::tie(selectedhost, err) = get_host(*request);
        if (!err.empty()) {
          return 1;
        }

        host->set_name(selectedhost->name());
        host->set_alias(selectedhost->get_alias());
        host->set_address(selectedhost->get_address());
        host->set_check_period(selectedhost->check_period());
        host->set_current_state(
            static_cast<EngineHost::State>(selectedhost->get_current_state()));
        host->set_id(selectedhost->host_id());
        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return contact informations.
 *
 * @param context gRPC context
 * @param request Contact's identifier
 * @param response The filled fields
 *
 * @return Status::OK
 **/
grpc::Status engine_impl::GetContact(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ContactIdentifier* request,
                                     EngineContact* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, contact = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::contact> selectedcontact;
        /* get the contact by his name */
        auto itcontactname = contact::contacts.find(request->name());
        if (itcontactname != contact::contacts.end())
          selectedcontact = itcontactname->second;
        else {
          err = fmt::format("could not find contact '{}'", request->name());
          return 1;
        }
        /* recovering contact's information */
        contact->set_name(selectedcontact->get_name());
        contact->set_alias(selectedcontact->get_alias());
        contact->set_email(selectedcontact->get_email());
        return 0;
      });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return service informations.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 *        hostid & serviceid)
 * @param response The filled fields
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetService(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ServiceIdentifier* request,
                                     EngineService* response) {
  std::string err;
  auto fn = std::packaged_task<int(void)>(
      [&err, request, service = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::service> selectedservice;
        std::tie(selectedservice, err) = get_serv(*request);
        if (!err.empty()) {
          return 1;
        }

        /* recovering service's information */
        service->set_host_id(selectedservice->host_id());
        service->set_service_id(selectedservice->service_id());
        service->set_host_name(selectedservice->get_hostname());
        service->set_description(selectedservice->description());
        service->set_check_period(selectedservice->check_period());
        service->set_current_state(static_cast<EngineService::State>(
            selectedservice->get_current_state()));
        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT, err);
}

/**
 * @brief Return the total number of hosts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetHostsCount(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty* request
                                        [[maybe_unused]],
                                        GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return host::hosts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contacts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */

grpc::Status engine_impl::GetContactsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contact::contacts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of services.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServicesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return service::services.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 *@return Status::OK
 */
grpc::Status engine_impl::GetServiceGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return servicegroup::servicegroups.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contact groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetContactGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contactgroup::contactgroups.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostGroupsCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostgroup::hostgroups.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServiceDependenciesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>([]() -> int32_t {
    return servicedependency::servicedependencies.size();
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostDependenciesCount(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostdependency::hostdependencies.size(); });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Add a comment into a host.
 *
 * @param context gRPC context
 * @param request EngineComment. EngineComment requires differents
 * fields to add a host comment. theses fields are :
 *    host name
 *    user name
 *    the comment
 *    persistent value
 *    entry time value
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::AddHostComment(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const EngineComment* request,
                                         CommandSuccess* response
                                         [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::host, comment::user, temp_host->host_id(), 0,
        request->entry_time(), request->user(), request->comment_data(),
        request->persistent(), comment::external, false, (time_t)0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Add a comment into a service.
 *
 * @param context gRPC context
 * @param request EngineComment. EngineComment requires differents
 * fields to add a service comment. theses fields are :
 *    host name
 *    service description
 *    user name
 *    the comment
 *    persistent value
 *    entry time value
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::AddServiceComment(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const EngineComment* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::shared_ptr<engine::service> temp_service;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->svc_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service ('{}', '{}')",
                        request->host_name(), request->svc_desc());
      return 1;
    }
    auto it2 = host::hosts.find(request->host_name());
    if (it2 != host::hosts.end())
      temp_host = it2->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::service, comment::user, temp_host->host_id(),
        temp_service->service_id(), request->entry_time(), request->user(),
        request->comment_data(), request->persistent(), comment::external,
        false, (time_t)0);
    if (!cmt) {
      err =
          fmt::format("could not insert comment '{}'", request->comment_data());
      return 1;
    }
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Remove a host or service comment from the status log.
 *
 * @param context gRPC context
 * @param request GenericValue. Id of the comment
 * @param response Command Success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteComment(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const GenericValue* request,
                                        CommandSuccess* response
                                        [[maybe_unused]]) {
  uint32_t comment_id = request->value();
  std::string err;
  if (comment_id == 0)
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "comment_id must not be set to 0");

  auto fn = std::packaged_task<int32_t(void)>([&err, &comment_id]() -> int32_t {
    if (comment::delete_comment(comment_id))
      return 0;
    else {
      err = fmt::format("could not delete comment with id {}", comment_id);
      return 1;
    }
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes all comments from a host.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllHostComments(grpc::ServerContext* context
                                                [[maybe_unused]],
                                                const HostIdentifier* request,
                                                CommandSuccess* response
                                                [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::tie(temp_host, err) = get_host(*request);
    if (!err.empty()) {
      return 1;
    }
    comment::delete_host_comments(temp_host->host_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes all comments from a service.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllServiceComments(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    std::tie(temp_service, err) = get_serv(*request);
    if (!err.empty()) {
      return 1;
    }

    comment::delete_service_comments(temp_service->host_id(),
                                     temp_service->service_id());
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes a host acknowledgement.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or hostid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::RemoveHostAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const HostIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::tie(temp_host, err) = get_host(*request);
    if (!err.empty()) {
      return 1;
    }

    /* set the acknowledgement flag */
    temp_host->set_acknowledgement(AckType::NONE);
    /* update the status log with the host info */
    temp_host->update_status();
    /* remove any non-persistant comments associated with the ack */
    comment::delete_host_acknowledgement_comments(temp_host.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Removes a service acknowledgement.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::RemoveServiceAcknowledgement(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    std::tie(temp_service, err) = get_serv(*request);
    if (!err.empty()) {
      return 1;
    }

    /* set the acknowledgement flag */
    temp_service->set_acknowledgement(AckType::NONE);
    /* update the status log with the service info */
    temp_service->update_status();
    /* remove any non-persistant comments associated with the ack */
    comment::delete_service_acknowledgement_comments(temp_service.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::AcknowledgementHostProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const EngineAcknowledgement* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* cannot acknowledge a non-existent problem */
    if (temp_host->get_current_state() == host::state_up) {
      err = fmt::format("state of host '{}' is up", request->host_name());
      return 1;
    }
    /* set the acknowledgement flag */
    if (request->type() == EngineAcknowledgement_Type_STICKY)
      temp_host->set_acknowledgement(AckType::STICKY);
    else
      temp_host->set_acknowledgement(AckType::NORMAL);
    /* schedule acknowledgement expiration */
    time_t current_time(time(nullptr));
    temp_host->set_last_acknowledgement(current_time);
    temp_host->schedule_acknowledgement_expiration();
    /* send data to event broker */
    broker_acknowledgement_data(
        NEBTYPE_ACKNOWLEDGEMENT_ADD, acknowledgement_resource_type::HOST,
        static_cast<void*>(temp_host.get()), request->ack_author().c_str(),
        request->ack_data().c_str(), request->type(), request->notify(),
        request->persistent());
    /* send out an acknowledgement notification */
    if (request->notify())
      temp_host->notify(notifier::reason_acknowledgement, request->ack_author(),
                        request->ack_data(),
                        notifier::notification_option_none);
    /* update the status log with the host info */
    temp_host->update_status();
    /* add a comment for the acknowledgement */
    auto com = std::make_shared<comment>(
        comment::host, comment::acknowledgment, temp_host->host_id(), 0,
        current_time, request->ack_author(), request->ack_data(),
        request->persistent(), comment::internal, false, (time_t)0);
    comment::comments.insert({com->get_comment_id(), com});

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::AcknowledgementServiceProblem(
    grpc::ServerContext* context [[maybe_unused]],
    const EngineAcknowledgement* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    /* cannot acknowledge a non-existent problem */
    if (temp_service->get_current_state() == service::state_ok) {
      err = fmt::format("state of service '{}', '{}' is up",
                        request->host_name(), request->service_desc());
      return 1;
    }
    /* set the acknowledgement flag */
    if (request->type() == EngineAcknowledgement_Type_STICKY)
      temp_service->set_acknowledgement(AckType::STICKY);
    else
      temp_service->set_acknowledgement(AckType::NORMAL);
    /* schedule acknowledgement expiration */
    time_t current_time = time(nullptr);
    temp_service->set_last_acknowledgement(current_time);
    temp_service->schedule_acknowledgement_expiration();
    /* send data to event broker */
    broker_acknowledgement_data(
        NEBTYPE_ACKNOWLEDGEMENT_ADD, acknowledgement_resource_type::SERVICE,
        static_cast<void*>(temp_service.get()), request->ack_author().c_str(),
        request->ack_data().c_str(), request->type(), request->notify(),
        request->persistent());
    /* send out an acknowledgement notification */
    if (request->notify())
      temp_service->notify(notifier::reason_acknowledgement,
                           request->ack_author(), request->ack_data(),
                           notifier::notification_option_none);
    /* update the status log with the service info */
    temp_service->update_status();

    /* add a comment for the acknowledgement */
    auto com = std::make_shared<comment>(
        comment::service, comment::acknowledgment, temp_service->host_id(),
        temp_service->service_id(), current_time, request->ack_author(),
        request->ack_data(), request->persistent(), comment::internal, false,
        (time_t)0);
    comment::comments.insert({com->get_comment_id(), com});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules downtime for a specific host.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields for a host are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id = 0;
    unsigned long duration;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());
    /* scheduling downtime */
    int res = downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    if (res == ERROR) {
      err = fmt::format("could not schedule downtime of host '{}'",
                        request->host_name());
      return 1;
    } else {
      return 0;
    }
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific service.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields for a service are :
 *  host name
 *  service description
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->service_desc().empty() ||
      request->author().empty() || request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling downtime */
    int res = downtime_manager::instance().schedule_downtime(
        downtime::service_downtime, temp_service->host_id(),
        temp_service->service_id(), request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    if (res == ERROR) {
      err = fmt::format("could not schedule downtime of service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    } else
      return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services from a specific host.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    for (service_map_unsafe::iterator it(temp_host->services.begin()),
         end(temp_host->services.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, temp_host->host_id(),
          it->second->service_id(), request->entry_time(),
          request->author().c_str(), request->comment_data().c_str(),
          request->start(), request->end(), request->fixed(),
          request->triggered_by(), duration, &downtime_id);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all hosts from a specific host group name.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostGroupHostsDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id = 0;
    unsigned long duration;
    hostgroup* hg{nullptr};
    /* get the host group */
    hostgroup_map::const_iterator it(
        hostgroup::hostgroups.find(request->host_group_name()));
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    hg = it->second.get();
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through host group members(hosts) */
    for (host_map_unsafe::iterator it(hg->members.begin()),
         end(hg->members.end());
         it != end; ++it)
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, it->second->host_id(), 0,
          request->entry_time(), request->author().c_str(),
          request->comment_data().c_str(), request->start(), request->end(),
          request->fixed(), request->triggered_by(), duration, &downtime_id);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services belonging
 * to the hosts of the host group name.
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostGroupServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id(0);
    unsigned long duration;
    hostgroup* hg{nullptr};
    /* get the hostgroup */
    hostgroup_map::const_iterator it(
        hostgroup::hostgroups.find(request->host_group_name()));
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    hg = it->second.get();
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through host group members(hosts) */
    for (host_map_unsafe::iterator it(hg->members.begin()),
         end(hg->members.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      /* iterate through services of the current host */
      for (service_map_unsafe::iterator it2(it->second->services.begin()),
           end2(it->second->services.end());
           it2 != end2; ++it2) {
        if (!it2->second)
          continue;
        /* scheduling downtime */
        downtime_manager::instance().schedule_downtime(
            downtime::service_downtime, it2->second->host_id(),
            it2->second->service_id(), request->entry_time(),
            request->author().c_str(), request->comment_data().c_str(),
            request->start(), request->end(), request->fixed(),
            request->triggered_by(), duration, &downtime_id);
      }
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all host from a service group name
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  service group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceGroupHostsDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->service_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    host* temp_host{nullptr};
    host* last_host{nullptr};
    uint64_t downtime_id(0);
    unsigned long duration;
    servicegroup_map::const_iterator sg_it;
    /* verify that the servicegroup is valid */
    sg_it = servicegroup::servicegroups.find(request->service_group_name());
    if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
      err = fmt::format("could not find servicegroupname '{}'",
                        request->service_group_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
         end(sg_it->second->members.end());
         it != end; ++it) {
      /* get the host to schedule */
      host_map::const_iterator found(host::hosts.find(it->first.first));
      if (found == host::hosts.end() || !found->second)
        continue;
      temp_host = found->second.get();
      if (last_host == temp_host)
        continue;
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::host_downtime, it->second->host_id(), 0,
          request->entry_time(), request->author().c_str(),
          request->comment_data().c_str(), request->start(), request->end(),
          request->fixed(), request->triggered_by(), duration, &downtime_id);
      last_host = temp_host;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for all services from a service group name
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  service group name
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceGroupServicesDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->service_group_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    uint64_t downtime_id(0);
    unsigned long duration;
    servicegroup_map::const_iterator sg_it;
    /* verify that the servicegroup is valid */
    sg_it = servicegroup::servicegroups.find(request->service_group_name());
    if (sg_it == servicegroup::servicegroups.end() || !sg_it->second) {
      err = fmt::format("could not find servicegroupname '{}'",
                        request->service_group_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* iterate through the services of service group */
    for (service_map_unsafe::iterator it(sg_it->second->members.begin()),
         end(sg_it->second->members.end());
         it != end; ++it)
      /* scheduling downtime */
      downtime_manager::instance().schedule_downtime(
          downtime::service_downtime, it->second->host_id(),
          it->second->service_id(), request->entry_time(),
          request->author().c_str(), request->comment_data().c_str(),
          request->start(), request->end(), request->fixed(),
          request->triggered_by(), duration, &downtime_id);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific host and his childrens
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleAndPropagateHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the main host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling the parent host */
    downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);

    /* schedule (non-triggered) downtime for all child hosts */
    command_manager::schedule_and_propagate_downtime(
        temp_host.get(), request->entry_time(), request->author().c_str(),
        request->comment_data().c_str(), request->start(), request->end(),
        request->fixed(), 0, duration);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief Schedules downtime for a specific host and his childrens with a
 * triggered id
 *
 * @param context gRPC context
 * @param request ScheduleDowntime's identifier. This type requires fields
 * to complete a downtime. Theses fields are :
 *  host name
 *  start time value
 *  end time value
 *  fixed value
 *  triggered by value
 *  duration value
 *  author name
 *  the comment
 *  entry time value
 * @param response Command answer
 *
 * @return Status::OK
 */

grpc::Status engine_impl::ScheduleAndPropagateTriggeredHostDowntime(
    grpc::ServerContext* context [[maybe_unused]],
    const ScheduleDowntimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty() || request->author().empty() ||
      request->comment_data().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "all fieds must be defined");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    uint64_t downtime_id(0);
    unsigned long duration;
    /* get the main host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (request->fixed())
      duration = static_cast<unsigned long>(request->end() - request->start());
    else
      duration = static_cast<unsigned long>(request->duration());

    /* scheduling the parent host */
    downtime_manager::instance().schedule_downtime(
        downtime::host_downtime, temp_host->host_id(), 0, request->entry_time(),
        request->author().c_str(), request->comment_data().c_str(),
        request->start(), request->end(), request->fixed(),
        request->triggered_by(), duration, &downtime_id);
    /* scheduling his childs */
    command_manager::schedule_and_propagate_downtime(
        temp_host.get(), request->entry_time(), request->author().c_str(),
        request->comment_data().c_str(), request->start(), request->end(),
        request->fixed(), downtime_id, duration);

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else {
    response->set_value(1);
    return grpc::Status::OK;
  }
}

/**
 * @brief  Deletes scheduled downtime
 *
 * @param context gRPC context
 * @param request GenericValue. GenericValue is a downtime id
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntime(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const GenericValue* request,
                                         CommandSuccess* response
                                         [[maybe_unused]]) {
  uint32_t downtime_id = request->value();
  std::string err;
  auto fn =
      std::packaged_task<int32_t(void)>([&err, &downtime_id]() -> int32_t {
        /* deletes scheduled  downtime */
        if (downtime_manager::instance().unschedule_downtime(downtime_id) ==
            ERROR) {
          err = fmt::format("could not delete downtime {}", downtime_id);
          return 1;
        } else
          return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delete scheduled host  downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a start_time,
 * an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteHostDowntimeFull(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeCriterias* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::list<std::shared_ptr<downtimes::downtime>> dtlist;
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      auto dt = it->second;
      uint64_t host_id = engine::get_host_id(request->host_name());
      if (!request->host_name().empty() && host_id != dt->host_id())
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      dtlist.push_back(dt);
    }

    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(d->get_downtime_id());

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delete scheduled service downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a service description
 * a start_time, an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteServiceDowntimeFull(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeCriterias* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::list<service_downtime*> dtlist;
    /* iterate through all current downtime(s) */
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      service_downtime* dt = static_cast<service_downtime*>(it->second.get());
      /* we are checking if request criteria match with the downtime criteria */
      auto p =
          engine::get_host_and_service_names(dt->host_id(), dt->service_id());
      if (!request->host_name().empty() && p.first != request->host_name())
        continue;
      if (!request->service_desc().empty() &&
          p.second != request->service_desc())
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      /* if all criterias match then we found a downtime to delete */
      dtlist.push_back(dt);
    }

    /* deleting downtime(s) */
    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(d->get_downtime_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on hostname and
 * optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByHostName(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeHostIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  /*hostname must be defined to delete the downtime but not others arguments*/
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &host_name,
                                               request]() -> int32_t {
    std::pair<bool, time_t> start_time;
    std::string service_desc;
    std::string comment_data;
    if (!(request->service_desc().empty()))
      service_desc = request->service_desc();
    if (!(request->comment_data().empty()))
      comment_data = request->comment_data();
    if (!(request->has_start()))
      start_time = {false, 0};
    else
      start_time = {true, request->start().value()};

    uint32_t deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                host_name, service_desc, start_time, comment_data);
    if (deleted == 0) {
      err = fmt::format("could not delete downtime with hostname '{}'",
                        request->host_name());
      return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on hostgroupname
 * and optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByHostGroupName(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeHostGroupIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string const& host_group_name = request->host_group_name();
  if (host_group_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_group_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &host_group_name,
                                               request]() -> int32_t {
    std::pair<bool, time_t> start_time;
    std::string host_name;
    std::string service_desc;
    std::string comment_data;
    uint32_t deleted = 0;

    auto it = hostgroup::hostgroups.find(host_group_name);
    if (it == hostgroup::hostgroups.end() || !it->second) {
      err = fmt::format("could not find host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    if (!(request->host_name().empty()))
      host_name = request->host_name();
    if (!(request->service_desc().empty()))
      service_desc = request->service_desc();
    if (!(request->comment_data().empty()))
      comment_data = request->comment_data();
    if (!(request->has_start()))
      start_time = {false, 0};
    else
      start_time = {true, request->start().value()};

    for (host_map_unsafe::iterator it_h(it->second->members.begin()),
         end_h(it->second->members.end());
         it_h != end_h; ++it_h) {
      if (!it_h->second)
        continue;
      if (!(host_name.empty()) && it_h->first != host_name)
        continue;
      deleted =
          downtime_manager::instance()
              .delete_downtime_by_hostname_service_description_start_time_comment(
                  host_name, service_desc, start_time, comment_data);
    }

    if (deleted == 0) {
      err = fmt::format("could not delete downtime with host group name '{}'",
                        request->host_group_name());
      return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on start time,
 * the comment and optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's
 * comment
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByStartTimeComment(
    grpc::ServerContext* context [[maybe_unused]],
    const DowntimeStartTimeIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  time_t start_time;
  /*hostname must be defined to delete the downtime but not others arguments*/
  if (!(request->has_start()))
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "start_time must not be empty");
  else
    start_time = request->start().value();

  std::string const& comment_data = request->comment_data();
  if (comment_data.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "comment_data must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &comment_data,
                                               &start_time]() -> int32_t {
    uint32_t deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                "", "", {true, start_time}, comment_data);
    if (0 == deleted) {
      err = fmt::format("could not delete comment with comment_data '{}'",
                        comment_data);
      return 1;
    }
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules a host check at particular time
 *
 * @param context gRPC context
 * @param request HostCheckIdentifier. HostCheckIdentifier is a host name
 * and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostCheck(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const HostCheckIdentifier* request,
                                            CommandSuccess* response
                                            [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    if (!request->force())
      temp_host->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
    else
      temp_host->schedule_check(request->delay_time(),
                                CHECK_OPTION_FORCE_EXECUTION);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules all services check from a host at particular time
 *
 * @param context gRPC context
 * @param request HostCheckIdentifier. HostCheckIdentifier is a host name
 * and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleHostServiceCheck(
    grpc::ServerContext* context [[maybe_unused]],
    const HostCheckIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    /* get the host */
    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    /* iterate through services of the current host */
    for (service_map_unsafe::iterator it(temp_host->services.begin()),
         end(temp_host->services.end());
         it != end; ++it) {
      if (!it->second)
        continue;
      if (!request->force())
        it->second->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
      else
        it->second->schedule_check(request->delay_time(),
                                   CHECK_OPTION_FORCE_EXECUTION);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Schedules a service check at particular time
 *
 * @param context gRPC context
 * @param request ServiceCheckIdentifier. HostCheckIdentifier is a host name,
 *  a service description and a delay time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ScheduleServiceCheck(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceCheckIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  if (request->service_desc().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "service description must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    /* get the service */
    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    if (!request->force())
      temp_service->schedule_check(request->delay_time(), CHECK_OPTION_NONE);
    else
      temp_service->schedule_check(request->delay_time(),
                                   CHECK_OPTION_FORCE_EXECUTION);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief  Schedules a program shutdown or restart
 *
 * @param context gRPC context
 * @param request EngineSignalProcess. HostCheckIdentifier is a process name
 * and a scheduled time.
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::SignalProcess(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const EngineSignalProcess* request,
                                        CommandSuccess* response
                                        [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::unique_ptr<timed_event> evt;
    if (EngineSignalProcess::Process_Name(request->process()) == "SHUTDOWN") {
      /* add a scheduled program shutdown or restart to the event list */
      evt = std::make_unique<timed_event>(timed_event::EVENT_PROGRAM_SHUTDOWN,
                                          request->scheduled_time(), false, 0,
                                          nullptr, false, nullptr, nullptr, 0);
    } else if (EngineSignalProcess::Process_Name(request->process()) ==
               "RESTART") {
      evt = std::make_unique<timed_event>(timed_event::EVENT_PROGRAM_RESTART,
                                          request->scheduled_time(), false, 0,
                                          nullptr, false, nullptr, nullptr, 0);
    } else {
      err = "no signal informed, you should inform a restart or a shutdown";
      return 1;
    }

    events::loop::instance().schedule(std::move(evt), true);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delays a host notification for given number of minutes
 *
 * @param context gRPC context
 * @param request HostDelayIdentifier. HostDelayIdentifier is a host name or a
 * host id and a delay time
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DelayHostNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const HostDelayIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    switch (request->identifier_case()) {
      case HostDelayIdentifier::kName: {
        auto it = host::hosts.find(request->name());
        if (it != host::hosts.end())
          temp_host = it->second;
        if (temp_host == nullptr) {
          err = fmt::format("could not find host '{}'", request->name());
          return 1;
        }
      } break;
      case HostDelayIdentifier::kId: {
        auto it = host::hosts_by_id.find(request->id());
        if (it != host::hosts_by_id.end())
          temp_host = it->second;
        if (temp_host == nullptr) {
          err = fmt::format("could not find host {}", request->id());
          return 1;
        }
      } break;
      default: {
        err = "could not find identifier, you should inform a real host";
        return 1;
        break;
      }
    }

    temp_host->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Delays a service notification for given number of minutes
 *
 * @param context gRPC context
 * @param request ServiceDelayIdentifier. ServiceDelayIdentifier is
 * a {host name and service name} or a {host id, service id} and delay time
 * @param Command response
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DelayServiceNotification(
    grpc::ServerContext* context [[maybe_unused]],
    const ServiceDelayIdentifier* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;

    switch (request->identifier_case()) {
      case ServiceDelayIdentifier::kNames: {
        NameIdentifier names = request->names();
        auto it =
            service::services.find({names.host_name(), names.service_name()});
        if (it != service::services.end())
          temp_service = it->second;
        if (temp_service == nullptr) {
          err = fmt::format("could not find service ('{}', '{}')",
                            names.host_name(), names.service_name());
          return 1;
        }
      } break;
      case ServiceDelayIdentifier::kIds: {
        IdIdentifier ids = request->ids();
        auto it =
            service::services_by_id.find({ids.host_id(), ids.service_id()});
        if (it != service::services_by_id.end())
          temp_service = it->second;
        if (temp_service == nullptr) {
          err = fmt::format("could not find service ({}, {})", ids.host_id(),
                            ids.service_id());
          return 1;
        }
      } break;
      default: {
        err = "could not find identifier, you should inform a real service";
        return 1;
        break;
      }
    }

    temp_service->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectIntVar(grpc::ServerContext* context
                                                 [[maybe_unused]],
                                                 const ChangeObjectInt* request,
                                                 CommandSuccess* response
                                                 [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    unsigned long attr = MODATTR_NONE;

    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    switch (request->mode()) {
      case ChangeObjectInt_Mode_NORMAL_CHECK_INTERVAL: {
        /* save the old check interval */
        double old_dval = temp_host->check_interval();

        /* modify the check interval */
        temp_host->set_check_interval(request->dval());
        attr = MODATTR_NORMAL_CHECK_INTERVAL;
        temp_host->set_modified_attributes(
            temp_host->get_modified_attributes() | attr);

        /* schedule a host check if previous interval was 0 (checks were not
         * regularly scheduled) */
        if (old_dval == 0 && temp_host->active_checks_enabled()) {
          time_t preferred_time(0);
          time_t next_valid_time(0);
          /* set the host check flag */
          temp_host->set_should_be_scheduled(true);

          /* schedule a check for right now (or as soon as possible) */
          time(&preferred_time);
          if (!check_time_against_period(preferred_time,
                                         temp_host->check_period_ptr)) {
            get_next_valid_time(preferred_time, &next_valid_time,
                                temp_host->check_period_ptr);
            temp_host->set_next_check(next_valid_time);
          } else
            temp_host->set_next_check(preferred_time);

          /* schedule a check if we should */
          if (temp_host->get_should_be_scheduled())
            temp_host->schedule_check(temp_host->get_next_check(),
                                      CHECK_OPTION_NONE);
        }
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);

        /* We need check result to handle next check */
        temp_host->update_status();
      } break;

      case ChangeObjectInt_Mode_RETRY_CHECK_INTERVAL:
        temp_host->set_retry_interval(request->dval());
        attr = MODATTR_RETRY_CHECK_INTERVAL;
        temp_host->set_modified_attributes(
            temp_host->get_modified_attributes() | attr);
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;

      case ChangeObjectInt_Mode_MAX_ATTEMPTS:
        temp_host->set_max_attempts(request->intval());
        attr = MODATTR_MAX_CHECK_ATTEMPTS;
        temp_host->set_modified_attributes(
            temp_host->get_modified_attributes() | attr);

        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);

        /* adjust current attempt number if in a hard state */
        if (temp_host->get_state_type() == notifier::hard &&
            temp_host->get_current_state() != host::state_up &&
            temp_host->get_current_attempt() > 1) {
          temp_host->set_current_attempt(temp_host->max_check_attempts());
          /* We need check result to handle next check */
          temp_host->update_status();
        }
        break;

      case ChangeObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_host->set_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;

      default:
        err = "no mode informed for method ChangeHostObjectIntVar";
        return 1;
    }

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectIntVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectInt* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    unsigned long attr = MODATTR_NONE;

    auto it =
        service::services.find({request->host_name(), request->service_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr) {
      err = fmt::format("could not find service '{}', '{}'",
                        request->host_name(), request->service_desc());
      return 1;
    }
    switch (request->mode()) {
      case ChangeObjectInt_Mode_NORMAL_CHECK_INTERVAL: {
        /* save the old check interval */
        double old_dval = temp_service->check_interval();

        /* modify the check interval */
        temp_service->set_check_interval(request->dval());
        attr = MODATTR_NORMAL_CHECK_INTERVAL;

        /* schedule a service check if previous interval was 0 (checks were not
         * regularly scheduled) */
        if (old_dval == 0 && temp_service->active_checks_enabled() &&
            temp_service->check_interval() != 0) {
          time_t preferred_time(0);
          time_t next_valid_time(0);
          /* set the service check flag */
          temp_service->set_should_be_scheduled(true);

          /* schedule a check for right now (or as soon as possible) */
          time(&preferred_time);
          if (!check_time_against_period(preferred_time,
                                         temp_service->check_period_ptr)) {
            get_next_valid_time(preferred_time, &next_valid_time,
                                temp_service->check_period_ptr);
            temp_service->set_next_check(next_valid_time);
          } else
            temp_service->set_next_check(preferred_time);

          /* schedule a check if we should */
          if (temp_service->get_should_be_scheduled())
            temp_service->schedule_check(temp_service->get_next_check(),
                                         CHECK_OPTION_NONE);
        }
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, NEBATTR_NONE,
                                     temp_service.get(), attr);

        /* We need check result to handle next check */
        temp_service->update_status();
      } break;
      case ChangeObjectInt_Mode_RETRY_CHECK_INTERVAL:
        temp_service->set_retry_interval(request->dval());
        attr = MODATTR_RETRY_CHECK_INTERVAL;
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);
        /* send data to event broker */
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, NEBATTR_NONE,
                                     temp_service.get(), attr);
        break;

      case ChangeObjectInt_Mode_MAX_ATTEMPTS:
        temp_service->set_max_attempts(request->intval());
        attr = MODATTR_MAX_CHECK_ATTEMPTS;
        temp_service->set_modified_attributes(
            temp_service->get_modified_attributes() | attr);

        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, NEBATTR_NONE,
                                     temp_service.get(), attr);

        /* adjust current attempt number if in a hard state */
        if (temp_service->get_state_type() == notifier::hard &&
            temp_service->get_current_state() != service::state_ok &&
            temp_service->get_current_attempt() > 1) {
          temp_service->set_current_attempt(temp_service->max_check_attempts());
          /* We need check result to handle next check */
          temp_service->update_status();
        }
        break;

      case ChangeObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_service->set_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE,
                                     NEBFLAG_NONE, NEBATTR_NONE,
                                     temp_service.get(), attr);
        break;
      default:
        err = "no mode informed for method ChangeServiceObjectIntVar";
        return 1;
    }

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectIntVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeContactObjectInt* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<com::centreon::engine::contact> temp_contact;
    unsigned long attr = MODATTR_NONE;
    unsigned long hattr = MODATTR_NONE;
    unsigned long sattr = MODATTR_NONE;

    auto itcontactname = contact::contacts.find(request->contact_name());
    if (itcontactname != contact::contacts.end())
      temp_contact = itcontactname->second;
    else {
      err = fmt::format("could not find contact '{}'", request->contact_name());
      return 1;
    }

    switch (request->mode()) {
      case ChangeContactObjectInt_Mode_MODATTR:
        attr = request->intval();
        temp_contact->set_modified_attributes(attr);
        break;
      case ChangeContactObjectInt_Mode_MODHATTR:
        hattr = request->intval();
        temp_contact->set_modified_host_attributes(hattr);
        break;
      case ChangeContactObjectInt_Mode_MODSATTR:
        sattr = request->intval();
        temp_contact->set_modified_service_attributes(sattr);
        break;
      default:
        err = "no mode informed for method ChangeContactObjectIntVar";
        return 1;
    }

    /* send data to event broker */
    broker_adaptive_contact_data(
        NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE,
        temp_contact.get(), CMD_NONE, attr,
        temp_contact->get_modified_attributes(), hattr,
        temp_contact->get_modified_host_attributes(), sattr,
        temp_contact->get_modified_service_attributes(), nullptr);

    /* update the status log with the contact info */
    temp_contact->update_status_info(false);
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    response->set_value(result.get());

  return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    timeperiod* temp_timeperiod{nullptr};
    command_map::iterator cmd_found;
    unsigned long attr{MODATTR_NONE};

    /* For these cases, we verify that the host is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto it = host::hosts.find(request->host_name());
      if (it != host::hosts.end())
        temp_host = it->second;
      if (temp_host == nullptr) {
        err = fmt::format("could not find host '{}'", request->host_name());
        return 1;
      }
    }
    /* make sure the timeperiod is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto found = timeperiod::timeperiods.find(request->charval());
      if (found != timeperiod::timeperiods.end())
        temp_timeperiod = found->second.get();
      if (temp_timeperiod == nullptr) {
        err = fmt::format("could not find timeperiod with value '{}'",
                          request->charval());
        return 1;
      }
    }
    /* make sure the command exists */
    else {
      cmd_found = commands::command::commands.find(request->charval());
      if (cmd_found == commands::command::commands.end() ||
          !cmd_found->second) {
        err =
            fmt::format("no command found with value '{}'", request->charval());
        return 1;
      }
    }

    /* update the variable */
    switch (request->mode()) {
      case ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER:
#ifdef LEGACY_CONF
        config->global_host_event_handler(request->charval());
#else
        pb_config.set_global_host_event_handler(request->charval());
#endif
        global_host_event_handler_ptr = cmd_found->second.get();
        attr = MODATTR_EVENT_HANDLER_COMMAND;
        /* set the modified host attribute */
        modified_host_process_attributes |= attr;

        /* send data to event broker */
        broker_adaptive_program_data(
            NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE,
            CMD_NONE, attr, modified_host_process_attributes, MODATTR_NONE,
            modified_service_process_attributes, nullptr);
        /* update program status */
        update_program_status(false);
        break;
      case ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER:
        temp_host->set_event_handler(request->charval());
        temp_host->set_event_handler_ptr(cmd_found->second.get());
        attr = MODATTR_EVENT_HANDLER_COMMAND;
        /* set the modified host attribute */
        temp_host->add_modified_attributes(attr);
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND:
        temp_host->set_check_command(request->charval());
        temp_host->set_check_command_ptr(cmd_found->second);
        attr = MODATTR_CHECK_COMMAND;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD:
        temp_host->set_check_period(request->charval());
        temp_host->check_period_ptr = temp_timeperiod;
        attr = MODATTR_CHECK_TIMEPERIOD;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;
      case ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD:
        temp_host->set_notification_period(request->charval());
        temp_host->set_notification_period_ptr(temp_timeperiod);
        attr = MODATTR_NOTIFICATION_TIMEPERIOD;
        /* send data to event broker */
        broker_adaptive_host_data(NEBTYPE_ADAPTIVEHOST_UPDATE, NEBFLAG_NONE,
                                  NEBATTR_NONE, temp_host.get(), attr);
        break;
      default:
        err = "no mode informed for method ChangeHostObjectCharVar";
        return 1;
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;
    timeperiod* temp_timeperiod{nullptr};
    command_map::iterator cmd_found;
    unsigned long attr{MODATTR_NONE};

    /* For these cases, we verify that the host is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND ||
        request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      /* verify that the service is valid */
      auto it = service::services.find(
          {request->host_name(), request->service_desc()});
      if (it != service::services.end())
        temp_service = it->second;
      if (temp_service == nullptr) {
        err = fmt::format("could not find service ('{}', '{}')",
                          request->host_name(), request->service_desc());
        return 1;
      }
    }
    /* make sure the timeperiod is valid */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD ||
        request->mode() ==
            ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      auto found = timeperiod::timeperiods.find(request->charval());
      if (found != timeperiod::timeperiods.end())
        temp_timeperiod = found->second.get();
      if (temp_timeperiod == nullptr) {
        err = fmt::format("could not find timeperiod with value '{}'",
                          request->charval());
        return 1;
      }
    }
    /* make sure the command exists */
    else {
      cmd_found = commands::command::commands.find(request->charval());
      if (cmd_found == commands::command::commands.end() ||
          !cmd_found->second) {
        err =
            fmt::format("no command found with value '{}'", request->charval());
        return 1;
      }
    }

    /* update the variable */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER) {
#ifdef LEGACY_CONF
      config->global_service_event_handler(request->charval());
#else
      pb_config.set_global_service_event_handler(request->charval());
#endif
      global_service_event_handler_ptr = cmd_found->second.get();
      attr = MODATTR_EVENT_HANDLER_COMMAND;
    } else if (request->mode() == ChangeObjectChar_Mode_CHANGE_EVENT_HANDLER) {
      temp_service->set_event_handler(request->charval());
      temp_service->set_event_handler_ptr(cmd_found->second.get());
      attr = MODATTR_EVENT_HANDLER_COMMAND;
    } else if (request->mode() == ChangeObjectChar_Mode_CHANGE_CHECK_COMMAND) {
      temp_service->set_check_command(request->charval());
      temp_service->set_check_command_ptr(cmd_found->second);
      attr = MODATTR_CHECK_COMMAND;
    } else if (request->mode() ==
               ChangeObjectChar_Mode_CHANGE_CHECK_TIMEPERIOD) {
      temp_service->set_check_period(request->charval());
      temp_service->check_period_ptr = temp_timeperiod;
      attr = MODATTR_CHECK_TIMEPERIOD;
    } else if (request->mode() ==
               ChangeObjectChar_Mode_CHANGE_NOTIFICATION_TIMEPERIOD) {
      temp_service->set_notification_period(request->charval());
      temp_service->set_notification_period_ptr(temp_timeperiod);
      attr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else {
      err = "no mode informed for method ChangeServiceObjectCharVar";
      return 1;
    }

    /* send data to event broker and update status file */
    if (request->mode() == ChangeObjectChar_Mode_CHANGE_GLOBAL_EVENT_HANDLER) {
      /* set the modified service attribute */
      modified_service_process_attributes |= attr;

      /* send data to event broker */
      broker_adaptive_program_data(
          NEBTYPE_ADAPTIVEPROGRAM_UPDATE, NEBFLAG_NONE, NEBATTR_NONE, CMD_NONE,
          MODATTR_NONE, modified_host_process_attributes, attr,
          modified_service_process_attributes, nullptr);

      /* update program status */
      update_program_status(false);
    } else {
      /* set the modified service attribute */
      temp_service->add_modified_attributes(attr);

      /* send data to event broker */
      broker_adaptive_service_data(NEBTYPE_ADAPTIVESERVICE_UPDATE, NEBFLAG_NONE,
                                   NEBATTR_NONE, temp_service.get(), attr);
    }
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectCharVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeContactObjectChar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->contact().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "contact must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request](void) -> int32_t {
    std::shared_ptr<engine::contact> temp_contact;
    timeperiod* temp_timeperiod{nullptr};
    unsigned long hattr{MODATTR_NONE};
    unsigned long sattr{MODATTR_NONE};

    auto it = contact::contacts.find(request->contact());
    if (it != contact::contacts.end())
      temp_contact = it->second;
    if (temp_contact == nullptr) {
      err = fmt::format("could not find contact '{}'", request->contact());
      return 1;
    }

    auto found = timeperiod::timeperiods.find(request->charval());
    if (found != timeperiod::timeperiods.end())
      temp_timeperiod = found->second.get();
    if (temp_timeperiod == nullptr) {
      err = fmt::format("could not find timeperiod with value '{}'",
                        request->charval());
      return 1;
    }
    if (ChangeContactObjectChar::Mode_Name(request->mode()) ==
        "CHANGE_HOST_NOTIFICATION_TIMEPERIOD") {
      temp_contact->set_host_notification_period(request->charval());
      temp_contact->set_host_notification_period_ptr(temp_timeperiod);
      hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else if (ChangeContactObjectChar::Mode_Name(request->mode()) ==
               "CHANGE_CONTACT_SVC_NOTIFICATION_TIMEPERIOD") {
      temp_contact->set_service_notification_period(request->charval());
      temp_contact->set_service_notification_period_ptr(temp_timeperiod);
      hattr = MODATTR_NOTIFICATION_TIMEPERIOD;
    } else {
      err = "no mode informed for method ChangeContactObjectCharVar";
      return 1;
    }

    /* set the modified attributes */
    temp_contact->set_modified_host_attributes(
        temp_contact->get_modified_host_attributes() | hattr);
    temp_contact->set_modified_service_attributes(
        temp_contact->get_modified_service_attributes() | sattr);

    /* send data to event broker */
    broker_adaptive_contact_data(
        NEBTYPE_ADAPTIVECONTACT_UPDATE, NEBFLAG_NONE, NEBATTR_NONE,
        temp_contact.get(), CMD_NONE, MODATTR_NONE,
        temp_contact->get_modified_attributes(), hattr,
        temp_contact->get_modified_host_attributes(), sattr,
        temp_contact->get_modified_service_attributes(), nullptr);

    /* update the status log with the contact info */
    temp_contact->update_status_info(false);

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeHostObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    // std::shared_ptr<engine::host> temp_host;
    host* temp_host{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    host_map::const_iterator it_h(host::hosts.find(request->host_name()));
    if (it_h != host::hosts.end())
      temp_host = it_h->second.get();
    if (temp_host == nullptr) {
      err = fmt::format("could not find host '{}'", request->host_name());
      return 1;
    }
    map_customvar::iterator it(temp_host->custom_variables.find(varname));
    if (it == temp_host->custom_variables.end())
      temp_host->custom_variables[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    /* set the modified attributes and update the status of the object */
    temp_host->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeServiceObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->host_name().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");
  if (request->service_desc().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "service description must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, &request]() -> int32_t {
    service* temp_service{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    service_map::const_iterator it_s(service::services.find(
        {request->host_name(), request->service_desc()}));
    if (it_s != service::services.end())
      temp_service = it_s->second.get();
    if (temp_service == nullptr) {
      err = fmt::format("could not find service ('{}', '{}')",
                        request->host_name(), request->service_desc());
      return 1;
    }
    map_customvar::iterator it(temp_service->custom_variables.find(varname));
    if (it == temp_service->custom_variables.end())
      temp_service->custom_variables[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    temp_service->add_modified_attributes(MODATTR_CUSTOM_VARIABLE);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

grpc::Status engine_impl::ChangeContactObjectCustomVar(
    grpc::ServerContext* context [[maybe_unused]],
    const ChangeObjectCustomVar* request,
    CommandSuccess* response [[maybe_unused]]) {
  if (request->contact().empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "contact must not be empty");

  std::string err;
  auto fn = std::packaged_task<int32_t(void)>([&err, request]() -> int32_t {
    contact* temp_contact{nullptr};
    std::string varname(request->varname());

    std::transform(varname.begin(), varname.end(), varname.begin(), ::toupper);
    contact_map::iterator cnct_it = contact::contacts.find(request->contact());
    if (cnct_it != contact::contacts.end())
      temp_contact = cnct_it->second.get();
    if (temp_contact == nullptr) {
      err = fmt::format("could not find contact '{}'", request->contact());
      return 1;
    }
    map_customvar::iterator it(
        temp_contact->get_custom_variables().find(varname));
    if (it == temp_contact->get_custom_variables().end())
      temp_contact->get_custom_variables()[varname] =
          customvariable(request->varvalue());
    else
      it->second.update(request->varvalue());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err);
  else
    return grpc::Status::OK;
}

/**
 * @brief Shutdown Program.
 *
 * @param context gRPC context
 * @param unused
 * @param response Command success
 *
 * @return Status::OK
 */
grpc::Status engine_impl::ShutdownProgram(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::google::protobuf::Empty* response [[maybe_unused]]) {
  auto fn = std::packaged_task<int32_t(void)>([]() -> int32_t {
    exit(0);
    return 0;
  });

  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

#define HOST_METHOD_BEGIN                                                    \
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,       \
                      *request);                                             \
  auto host_info = get_host(*request);                                       \
  if (!host_info.second.empty()) {                                           \
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown host {}", \
                        __FUNCTION__, *request, host_info.second);           \
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,                \
                        host_info.second);                                   \
  }

#define SERV_METHOD_BEGIN                                                    \
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,       \
                      *request);                                             \
  auto serv_info = get_serv(*request);                                       \
  if (!serv_info.second.empty()) {                                           \
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown serv {}", \
                        __FUNCTION__, *request, serv_info.second);           \
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,                \
                        serv_info.second);                                   \
  }

::grpc::Status engine_impl::EnableHostAndChildNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::HostIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  commands::processing::wrapper_enable_host_and_child_notifications(
      host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableHostAndChildNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::HostIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  commands::processing::wrapper_disable_host_and_child_notifications(
      host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableHostNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::HostIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  disable_host_notifications(host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableHostNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::HostIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  HOST_METHOD_BEGIN
  enable_host_notifications(host_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty*,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  disable_all_notifications();
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty*,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  enable_all_notifications();
  return grpc::Status::OK;
}

::grpc::Status engine_impl::DisableServiceNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ServiceIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SERV_METHOD_BEGIN
  disable_service_notifications(serv_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::EnableServiceNotifications(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ServiceIdentifier* request,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SERV_METHOD_BEGIN
  enable_service_notifications(serv_info.first.get());
  return grpc::Status::OK;
}

::grpc::Status engine_impl::ChangeAnomalyDetectionSensitivity(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::com::centreon::engine::ChangeServiceNumber* serv_and_value,
    ::com::centreon::engine::CommandSuccess* response [[maybe_unused]]) {
  SPDLOG_LOGGER_DEBUG(external_command_logger, "{}({})", __FUNCTION__,
                      serv_and_value->serv());
  auto serv_info = get_serv(serv_and_value->serv());
  if (!serv_info.second.empty()) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : unknown serv {}",
                        __FUNCTION__, serv_and_value->serv(), serv_info.second);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, serv_info.second);
  }

  if (serv_info.first->get_service_type() != service_type::ANOMALY_DETECTION) {
    SPDLOG_LOGGER_ERROR(external_command_logger,
                        "{}({}) : {} is not an anomalydetection", __FUNCTION__,
                        serv_and_value->serv(), serv_info.second);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, serv_info.second);
  }

  std::shared_ptr<anomalydetection> ano =
      std::static_pointer_cast<anomalydetection>(serv_info.first);

  if (serv_and_value->has_dval()) {
    ano->set_sensitivity(serv_and_value->dval());
    return grpc::Status::OK;
  }

  if (serv_and_value->has_intval()) {
    ano->set_sensitivity(serv_and_value->intval());
    return grpc::Status::OK;
  }
  SPDLOG_LOGGER_ERROR(external_command_logger, "{}({}) : no value provided",
                      __FUNCTION__, serv_and_value->serv());
  return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                      "no value provided");
}

/**
 * @brief find host ever by name or id
 *
 * @param host_info
 * @return std::shared_ptr<com::centreon::engine::host>
 */
std::pair<std::shared_ptr<com::centreon::engine::host>, std::string>
engine_impl::get_host(
    const ::com::centreon::engine::HostIdentifier& host_info) {
  std::string err;
  switch (host_info.identifier_case()) {
    case HostIdentifier::kName: {
      /* get the host */
      auto ithostname = host::hosts.find(host_info.name());
      if (ithostname != host::hosts.end())
        return std::make_pair(ithostname->second, err);
      else {
        err = fmt::format("could not find host '{}'", host_info.name());
        return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                              err);
      }
    } break;
    case HostIdentifier::kId: {
      /* get the host */
      auto ithostid = host::hosts_by_id.find(host_info.id());
      if (ithostid != host::hosts_by_id.end())
        return std::make_pair(ithostid->second, err);
      else {
        err = fmt::format("could not find host {}", host_info.id());
        return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                              err);
      }
    } break;
    default: {
      err = fmt::format("could not find identifier, you should inform a host");
      return std::make_pair(std::shared_ptr<com::centreon::engine::host>(),
                            err);
    }
  }
}

std::pair<std::shared_ptr<com::centreon::engine::service>,
          std::string /*error*/>
engine_impl::get_serv(
    const ::com::centreon::engine::ServiceIdentifier& serv_info) {
  std::string err;

  /* checking identifier sesrname (by names or by ids) */
  switch (serv_info.identifier_case()) {
    case ServiceIdentifier::kNames: {
      const NameIdentifier& names = serv_info.names();
      /* get the service */
      auto itservicenames = service::services.find(
          std::make_pair(names.host_name(), names.service_name()));
      if (itservicenames != service::services.end())
        return std::make_pair(itservicenames->second, err);
      else {
        err = fmt::format("could not find service ('{}', '{}')",
                          names.host_name(), names.service_name());
        return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                              err);
      }
    } break;
    case ServiceIdentifier::kIds: {
      const IdIdentifier& ids = serv_info.ids();
      /* get the service */
      auto itserviceids = service::services_by_id.find(
          std::make_pair(ids.host_id(), ids.service_id()));
      if (itserviceids != service::services_by_id.end())
        return std::make_pair(itserviceids->second, err);
      else {
        err = fmt::format("could not find service ({}, {})", ids.host_id(),
                          ids.service_id());
        return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                              err);
      }
    } break;
    default: {
      err = "could not find identifier, you should inform a service";
      return std::make_pair(std::shared_ptr<com::centreon::engine::service>(),
                            err);
    }
  }
}

/**
 * @brief get log levels and infos
 *
 * @param context
 * @param request
 * @param response
 * @return ::grpc::Status
 */
::grpc::Status engine_impl::GetLogInfo(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::com::centreon::engine::LogInfo* response) {
  std::vector<std::shared_ptr<spdlog::logger>> loggers;

  spdlog::apply_all([&loggers](const std::shared_ptr<spdlog::logger>& logger) {
    loggers.push_back(logger);
  });

  response->set_log_file(log_v2::instance().filename());
  response->set_log_flush_period(log_v2::instance().flush_interval().count());
  auto levels = response->mutable_level();
  for (const auto& logger : loggers) {
    auto level = spdlog::level::to_string_view(logger->level());
    (*levels)[logger->name()] = std::string(level.data(), level.size());
  }
  return grpc::Status::OK;
}

grpc::Status engine_impl::SetLogLevel(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const LogLevel* request,
                                      ::google::protobuf::Empty*) {
  const std::string& logger_name{request->logger()};
  std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
  if (!logger) {
    std::string err_detail =
        fmt::format("The '{}' logger does not exist", logger_name);
    SPDLOG_LOGGER_ERROR(external_command_logger, err_detail);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err_detail);
  } else {
    logger->set_level(spdlog::level::level_enum(request->level()));
    return grpc::Status::OK;
  }
}

grpc::Status engine_impl::SetLogFlushPeriod(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const LogFlushPeriod* request,
                                            ::google::protobuf::Empty*) {
  // first get all log_v2 objects
  log_v2::instance().set_flush_interval(request->period());
  return grpc::Status::OK;
}

/**
 * @brief get stats of the process (cpu, memory...)
 *
 * @param context
 * @param request
 * @param response
 * @return grpc::Status
 */
grpc::Status engine_impl::GetProcessStats(
    grpc::ServerContext* context [[maybe_unused]],
    const google::protobuf::Empty*,
    com::centreon::common::pb_process_stat* response) {
  try {
    com::centreon::common::process_stat stat(getpid());
    stat.to_protobuff(*response);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(external_command_logger, "fail to get process info: {}",
                        boost::diagnostic_information(e));

    return grpc::Status(grpc::StatusCode::INTERNAL,
                        boost::diagnostic_information(e));
  }
  return grpc::Status::OK;
}

/**
 * @brief send a bench event across brokers network
 *
 * @param context
 * @param request
 * @param response
 * @return grpc::Status
 */
grpc::Status engine_impl::SendBench(
    grpc::ServerContext* context [[maybe_unused]],
    const com::centreon::engine::BenchParam* request,
    google::protobuf::Empty* response [[maybe_unused]]) {
  std::chrono::system_clock::time_point client_ts =
      std::chrono::system_clock::time_point::min();

  if (request->ts().seconds() > 0) {
    client_ts = common::google_ts_to_time_point(request->ts());
  }

  broker_bench(request->id(), client_ts);
  return grpc::Status::OK;
}
