# Common documentation {#mainpage}

## Table of content

* [Pool](#Pool)
* [Grpc](#Grpc)
* [Process](#Process)
* [Engine configuration](#Engineconfiguration)


## Pool

After a fork, only caller thread is activated in child process, so we mustn't join others. That's why thread container is dynamically allocated and not freed in case of fork.


## Grpc
The goal of the two classes provided, grpc_server_base and grpc_client_base is to create server or channel in order to use it with grpc generated services such as exchange in broker grpc module. 
* `grpc_server_base` creates a ::grpc::server object. You can register service with third constructor parameter builder_option.
* `grpc_client_base` creates a ::grpc::channel that can be used to create a stub.

In order to use it you have to override and use protected methods and attributes:
  * server:
```c++
class my_grpc_server: public public com::centreon::common::grpc::grpc_server_base {
  std::shared_ptr<service_impl> _service;
public:
  my_grpc_server(const grpc_config::pointer& conf);
};

my_grpc_server::my_grpc_server(const grpc_config::pointer& conf)
    : com::centreon::common::grpc::grpc_server_base(conf, log_v2::grpc()) {
  _init([this](::grpc::ServerBuilder& builder) {
    _service = std::make_shared<service_impl>(
        std::static_pointer_cast<grpc_config>(get_conf()));
    builder.RegisterService(_service.get());
  });
}
```
  * client:
```c++
class my_grpc_client : public com::centreon::common::grpc::grpc_client_base {
  std::unique_ptr<com::centreon::my_service::Stub> _stub;

 public:
  my_grpc_client(const grpc_config::pointer& conf);
};

my_grpc_client::my_grpc_client(const grpc_config::pointer& conf)
    : com::centreon::common::grpc::grpc_client_base(conf, log_v2::grpc()) {
  _stub = std::move(com::centreon::my_service::NewStub(_channel));
}


```

## Process

The goal of this class is to provide an base class to execute asynchronously process according to asio library.
It relies on boost v2 process library.
All is asynchronous, child process end of life is notified to on_process_end method. It's the same for stdin write and stdout/err read.

You have 4 constructors that allow user to pass executable arguments in four different ways. On of them accept a string command line with exe and arguments

In order to use this, you have to inherit from this class

An example of usage:
```c++
class process_wait : public process {
  std::condition_variable _cond;
  std::string _stdout;
  std::string _stderr;

 public:
  void on_stdout_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      _stdout += std::string_view(_stdout_read_buffer, nb_read);
    }
    process::on_stdout_read(err, nb_read);
  }

  void on_stderr_read(const boost::system::error_code& err,
                      size_t nb_read) override {
    if (!err) {
      _stderr += std::string_view(_stderr_read_buffer, nb_read);
    }
    process::on_stderr_read(err, nb_read);
  }

  void on_process_end(const boost::system::error_code& err,
                      int raw_exit_status) override {
    process::on_process_end(err, raw_exit_status);
    _cond.notify_one();
  }

  template <typename string_type>
  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& exe_path,
               const std::initializer_list<string_type>& args)
      : process(io_context, logger, exe_path, args) {}

  process_wait(const std::shared_ptr<boost::asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::string_view& cmd_line)
      : process(io_context, logger, cmd_line) {}

  const std::string& get_stdout() const { return _stdout; }
  const std::string& get_stderr() const { return _stderr; }

  void wait() {
    std::mutex dummy;
    std::unique_lock l(dummy);
    _cond.wait(l);
  }
};

```

### Asio bug work around
There is an issue in io_context::notify_fork. Internally, ctx.notify_fork calls epoll_reactor::notify_fork which locks registered_descriptors_mutex_. An issue occurs when registered_descriptors_mutex_ is locked by another thread at fork timepoint. 
In such a case, child process starts with registered_descriptors_mutex_ already locked and both child and parent process will hang.

## Engine configuration

Here is the new Engine configuration library. It is a full rewrite of the legacy Engine configuration library that you can find now in the `engine_legacy_conf` directory. This new one library uses protobuf messages to store the configuration. We have tried to keep the objects structures as Protobuf was able to do.

A new parser has been implemented. It works differently from the previous one as the goal is to traduce the legacy cfg format to protobuf. The parser is in the `engine_conf/parser.cc` file, functions are almost the same as in the legacy parser but they use a lot the Protobuf reflection.

A cfg file defines objects, each one has fields that are given by key/value.

Configuration objects have default values whereas Protobuf messages have fixed default values, 0 for numbers, empty strings for strings, empty array for arrays, etc.

To allow custom default values in our messages, each one has a helper class associated to it. For example, the Contact message has a contact_helper class associated. To define them it is simple:
```
// To define a contact with the structure of Protobuf Contact
configuration::Contact ctc;
// Now we initialize the helper
configuration::contact_helper ctc_hlp(&ctc);
```
Once the helper is defined, all the default values in the message are initialized.
The helper provides also a hook method, this is needed because in the cfg files some fields can have several key names, and also some enum types can be defined from strings, or pair of strings or anything else. So to allow weird things with object as cfg files allow, we have the hook function.
The `hook()` function takes the key and the value as arguments and returns a boolean True if the hook has been correctly applied.

Three steps are done while a cfg file is parsed. For each key,
* we firstly try to read it from the `hook`.
* on failure, we try the `set` method that only uses the Protobuf reflection.
* on failure, we try to read the key as a custom variable, and here there is another way to parse it (the idea is very similar to the legacy parser).
