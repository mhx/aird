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

#ifndef AIRD_LOG_H_
#define AIRD_LOG_H_

#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

namespace aird {

struct log_level
{
   enum type
   {
      TRACE,
      DEBUG,
      INFO,
      WARN,
      ERROR,
      FATAL
   };

   static type str2level(const std::string& str);
   static const char *level2str(type level);
};

class appender
{
public:
   virtual void log(log_level::type level, const std::string& msg) const = 0;
   virtual ~appender();
};

class root_logger
{
public:
   root_logger(log_level::type level);

   log_level::type level() const
   {
      return m_level;
   }

   void log(log_level::type level, const std::string& msg) const;

   void add_appender(boost::shared_ptr<appender> app);

private:
   std::vector< boost::shared_ptr<appender> > m_app;
   log_level::type m_level;
};

class logger
{
public:
   logger(root_logger& lg, const std::string& name)
      : m_root(lg)
      , m_name(name)
   {
   }

   const std::string& name() const
   {
      return m_name;
   }

   log_level::type level() const
   {
      return m_root.level();
   }

   void log(log_level::type level, const std::string& msg) const
   {
      m_root.log(level, msg);
   }

   root_logger& root() const
   {
      return m_root;
   }

   bool operator() (log_level::type level) const
   {
      return level >= m_root.level();
   }

   void operator() (log_level::type level, const std::string& msg) const
   {
      log(level, '[' + m_name + "] " + msg);
   }

private:
   root_logger& m_root;
   const std::string m_name;
};

class syslog_appender : public appender
{
public:
   syslog_appender(const std::string& ident, log_level::type thresh);
   ~syslog_appender();
   virtual void log(log_level::type level, const std::string& msg) const;

private:
   log_level::type m_thresh;
};

class console_appender : public appender
{
public:
   console_appender(log_level::type thresh);
   virtual void log(log_level::type level, const std::string& msg) const;

private:
   log_level::type m_thresh;
};

}

#define LOG_HELPER__(logger, lvl, msg) \
        do { \
           if (::aird::log_level::lvl >= logger.level()) \
           { \
              std::ostringstream oss; \
              oss << '[' << logger.name() << "] " << msg; \
              logger.log(::aird::log_level::lvl, oss.str()); \
           } \
        } while (0)

#define LTRACE(logger, msg) LOG_HELPER__(logger, TRACE, msg)
#define LDEBUG(logger, msg) LOG_HELPER__(logger, DEBUG, msg)
#define LINFO(logger, msg)  LOG_HELPER__(logger, INFO, msg)
#define LWARN(logger, msg)  LOG_HELPER__(logger, WARN, msg)
#define LERROR(logger, msg) LOG_HELPER__(logger, ERROR, msg)
#define LFATAL(logger, msg) LOG_HELPER__(logger, FATAL, msg)

#endif
