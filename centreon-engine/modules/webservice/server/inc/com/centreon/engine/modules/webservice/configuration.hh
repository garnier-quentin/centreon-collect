/*
** Copyright 2011-2012 Merethis
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

#ifndef CCE_MOD_WS_CONFIGURATION_HH
#  define CCE_MOD_WS_CONFIGURATION_HH

#  include <map>
#  include <QXmlStreamReader>
#  include <string>
#  include "com/centreon/engine/modules/webservice/namespace.hh"

CCE_MOD_WS_BEGIN()

/**
 *  @class configuration configuration.hh
 *  @brief Base configuration class.
 *
 *  Simple configuration class to parse
 *  configuration file.
 */
class                configuration {
public:
                     configuration(std::string const& filename = "");
                     ~configuration() throw();
  int                get_accept_timeout() const throw();
  std::string const& get_filename() const throw();
  std::string const& get_host() const throw();
  int                get_port() const throw();
  int                get_recv_timeout() const throw();
  int                get_send_timeout() const throw();
  std::string const& get_ssl_cacert() const throw();
  std::string const& get_ssl_dh() const throw();
  bool               get_ssl_enable() const throw();
  std::string const& get_ssl_keyfile() const throw();
  std::string const& get_ssl_password() const throw();
  unsigned int       get_thread_count() const throw();
  void               parse();
  void               set_filename(std::string const& filename);

private:
                     configuration(configuration const& right);
  configuration&     operator=(configuration const& right);
  void               _set_accept_timeout();
  void               _set_host();
  void               _set_port();
  void               _set_recv_timeout();
  void               _set_send_timeout();
  void               _set_ssl_cacert();
  void               _set_ssl_dh();
  void               _set_ssl_enable();
  void               _set_ssl_keyfile();
  void               _set_ssl_password();
  void               _set_thread_count();

  int                _accept_timeout;
  std::string        _filename;
  std::string        _host;
  std::map<std::string, void (configuration::*)()>
                     _keytab;
  QXmlStreamReader   _reader;
  std::string        _path;
  int                _port;
  int                _recv_timeout;
  int                _send_timeout;
  std::string        _ssl_cacert;
  std::string        _ssl_dh;
  bool               _ssl_enable;
  std::string        _ssl_keyfile;
  std::string        _ssl_password;
  unsigned int       _thread_count;
};

CCE_MOD_WS_END()

#endif // !CCE_MOD_WS_CONFIGURATION_HH
