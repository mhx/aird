/* vim:set ts=3 sw=3 sts=3 et: */

/***********************************************************************

Copyright (c) 2012 Marcus Holland-Moritz

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***********************************************************************/

#include <fstream>

#include "settings.h"

namespace aird {

settings::settings(const std::string& config_file)
{
   using namespace boost::program_options;

   std::ifstream ifs(config_file.c_str());

   if (!ifs)
   {
      throw std::runtime_error("cannot open config file " + config_file);
   }

   std::string root, console, syslog;

   options_description options;

   ev.add_options(options);
   mon.add_options(options);
   srv.add_options(options);

   options.add_options()
      ("logging.root_level", value<std::string>(&root)->default_value("info"))
      ("logging.console_level", value<std::string>(&console)->default_value("info"))
      ("logging.syslog_level", value<std::string>(&syslog)->default_value("info"))
      ;

   try
   {
      variables_map vm;
      store(parse_config_file(ifs, options), vm);
      notify(vm);
      root_level = log_level::str2level(root);
      console_level = log_level::str2level(console);
      syslog_level = log_level::str2level(syslog);
   }
   catch (const std::exception& e)
   {
      throw std::runtime_error(std::string("error parsing config file <") + e.what() + ">");
   }
}

}
