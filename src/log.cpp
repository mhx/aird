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

#include <iostream>
#include <stdexcept>
#include <syslog.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "log.h"

namespace aird {

log_level::type log_level::str2level(const std::string& str)
{
   if (str == "trace") return TRACE;
   if (str == "debug") return DEBUG;
   if (str == "info") return INFO;
   if (str == "warn") return WARN;
   if (str == "error") return ERROR;
   if (str == "fatal") return FATAL;
   throw std::runtime_error("invalid logging level: " + str);
}

const char *log_level::level2str(type level)
{
   switch (level)
   {
      case TRACE: return "trace";
      case DEBUG: return "debug";
      case INFO:  return "info";
      case WARN:  return "warn";
      case ERROR: return "error";
      case FATAL: return "fatal";
      default: return "***";
   }
}

appender::~appender()
{
}

root_logger::root_logger(log_level::type level)
   : m_level(level)
{
}

void root_logger::log(log_level::type level, const std::string& msg) const
{
   if (level >= m_level)
   {
      for (std::vector< boost::shared_ptr<appender> >::const_iterator it = m_app.begin(); it != m_app.end(); ++it)
      {
         (*it)->log(level, msg);
      }
   }
}

void root_logger::add_appender(boost::shared_ptr<appender> app)
{
   m_app.push_back(app);
}

console_appender::console_appender(log_level::type thresh)
   : m_thresh(thresh)
{
}

void console_appender::log(log_level::type level, const std::string& msg) const
{
   if (level >= m_thresh)
   {
      boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
      std::cerr << to_simple_string(now) << " <" << log_level::level2str(level) << "> " << msg << std::endl;
   }
}

syslog_appender::syslog_appender(const std::string& ident, log_level::type thresh)
   : m_thresh(thresh)
{
   ::openlog(ident.c_str(), 0, LOG_DAEMON);
}

syslog_appender::~syslog_appender()
{
   ::closelog();
}

void syslog_appender::log(log_level::type level, const std::string& msg) const
{
   if (level >= m_thresh)
   {
      int priority;

      switch (level)
      {
         case log_level::INFO:  priority = LOG_INFO; break;
         case log_level::WARN:  priority = LOG_WARNING; break;
         case log_level::ERROR: priority = LOG_ERR; break;
         case log_level::FATAL: priority = LOG_CRIT; break;
         default:               priority = LOG_DEBUG; break;
      }

      ::syslog(priority, "%s", msg.c_str());
   }
}

}
