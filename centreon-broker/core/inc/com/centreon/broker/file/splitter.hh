/*
** Copyright 2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_FILE_SPLITTER_HH
#  define CCB_FILE_SPLITTER_HH

#  include <memory>
#  include <string>
#  include "com/centreon/broker/file/fs_browser.hh"
#  include "com/centreon/broker/file/fs_file.hh"
#  include "com/centreon/broker/misc/shared_ptr.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace       file {
  /**
   *  @class splitter splitter.hh "com/centreon/broker/file/splitter.hh"
   *  @brief Manage multi-file splitting.
   *
   *  Handle logical file splitting across multiple real files to
   *  provide easier file management.
   */
  class         splitter : public fs_file {
   public:
                splitter(
                  std::string const& path,
                  fs_file::open_mode mode,
                  fs_file_factory* file_factory,
                  fs_browser* fs,
                  long max_file_size = 100000000,
                  bool auto_delete = false);
                ~splitter();
    long        read(void* buffer, long max_size);
    void        seek(
                  long offset,
                  fs_file::seek_whence whence = fs_file::seek_start);
    long        tell();
    long        write(void const* buffer, long size);

   private:
                splitter(splitter const& other);
    splitter&   operator=(splitter const& other);
    std::string _file_path(int id) const;
    void        _open_next_read();
    void        _open_next_write();

    bool        _auto_delete;
    std::string _base_path;
    std::auto_ptr<fs_file_factory>
                _file_factory;
    std::auto_ptr<fs_browser>
                _fs;
    long        _max_file_size;
    misc::shared_ptr<fs_file>
                _rfile;
    int         _rid;
    long        _roffset;
    misc::shared_ptr<fs_file>
                _wfile;
    int         _wid;
    long        _woffset;
  };

  /**
   *  @class splitter_factory splitter.hh "com/centreon/broker/file/splitter.hh"
   *  @brief Create new file splitter.
   *
   *  Factory to create new file splitter.
   */
  class       splitter_factory : public fs_file_factory {
   public:
    fs_file*  new_fs_file(
                std::string const& path,
                fs_file::open_mode mode);
    splitter* new_cfile_splitter(
                std::string const& path,
                fs_file::open_mode mode,
                long max_file_size = 100000000,
                bool auto_delete = false);
  };
}

CCB_END()

#endif // !CCB_FILE_SPLITTER_HH
