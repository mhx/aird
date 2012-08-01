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
#include <fstream>
#include <csignal>
#include <cstring>
#include <cerrno>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>

#include "event_source.h"
#include "log.h"
#include "monitor.h"
#include "server.h"
#include "settings.h"

namespace aird {

class daemon
{
public:
   daemon(const settings& set, const std::string& pidfile)
      : m_root(set.root_level)
      , m_log(m_root, "daemon")
      , m_running(0)
      , m_set(set)
      , m_pidfile(pidfile)
   {
   }

   int run(const std::string& name, bool debug)
   {
      if (debug)
      {
         m_root.add_appender(boost::shared_ptr<appender>(new console_appender(m_set.console_level)));
      }
      else
      {
         daemonise();
         m_root.add_appender(boost::shared_ptr<appender>(new syslog_appender(name, m_set.syslog_level)));
      }

      try
      {
         m_ev.reset(new event_source(m_ios, m_root, m_set.ev));
         m_mon.reset(new monitor(m_ios, m_root, m_set.mon));
         m_srv.reset(new server(m_ios, m_root, m_set.srv));

         m_ev->start(m_mon->get_event_handler());
         m_srv->start(m_mon->get_status_provider());
         m_mon->start();

         m_running = 1;
         set_quit_handler(boost::bind(&daemon::stop, this));

         LINFO(m_log, "entering event loop");

         for (;;)
         {
            try
            {
               m_ios.run();
               break;
            }
            catch (const std::exception& e)
            {
               LERROR(m_log, e.what());
            }
         }

         m_mon->ensure_safe_defaults();

         LINFO(m_log, "finished successfully");

         return 0;
      }
      catch (const std::exception& e)
      {
         LFATAL(m_log, e.what());
      }
      catch (...)
      {
         LFATAL(m_log, "unknown exception caught");
      }

      return 1;
   }

   void stop()
   {
      LINFO(m_log, "stop called");

      if (m_running)
      {
         m_running = 0;
         m_ios.post(boost::bind(&daemon::stop_handler, this));
      }
   }

private:
   void stop_handler()
   {
      m_ev->stop();
      m_srv->stop();
      m_mon->stop();
   }

   static void set_quit_handler(boost::function<void ()> handler)
   {
      quit_handler = handler;

      struct sigaction sa_;

      ::memset(&sa_, 0, sizeof(sa_));
      sa_.sa_handler = daemon::sighdl;

      ::sigemptyset(&sa_.sa_mask);
      sa_.sa_flags = SIG_BLOCK;

      ::sigaddset(&sa_.sa_mask, SIGINT);
      ::sigaddset(&sa_.sa_mask, SIGTERM);
      ::sigaddset(&sa_.sa_mask, SIGQUIT);

      ::sigaction(SIGINT, &sa_, NULL);
      ::sigaction(SIGTERM, &sa_, NULL);
      ::sigaction(SIGQUIT, &sa_, NULL);
   }

   static void sighdl(int)
   {
      if (quit_handler)
      {
         quit_handler();
      }
   }

   void daemonise()
   {
      pid_t pid = ::fork();

      if (pid < 0)
      {
         throw std::runtime_error("fork() failed: " + std::string(strerror(errno)));
      }

      if (pid > 0)
      {
         std::ofstream ofs(m_pidfile.c_str());

         if (ofs)
         {
            ofs << pid << '\n';
            ofs.close();
         }
         else
         {
            std::cerr << "failed to write pid file to: " << m_pidfile << std::endl;
         }

         ::exit(EXIT_SUCCESS);
      }

      ::umask(0);

      pid_t sid = setsid();

      if (sid < 0)
      {
         throw std::runtime_error("setsid() failed: " + std::string(strerror(errno)));
      }

      if (::chdir("/") < 0)
      {
         throw std::runtime_error("chdir(\"/\") failed: " + std::string(strerror(errno)));
      }

      ::close(STDIN_FILENO);
      ::close(STDOUT_FILENO);
      ::close(STDERR_FILENO);
   }

   boost::asio::io_service m_ios;
   boost::shared_ptr<event_source> m_ev;
   boost::shared_ptr<monitor> m_mon;
   boost::shared_ptr<server> m_srv;
   root_logger m_root;
   logger m_log;
   sig_atomic_t m_running;
   const settings m_set;
   const std::string m_pidfile;

   static boost::function<void ()> quit_handler;
};

boost::function<void ()> daemon::quit_handler;

}

int main(int argc, const char **argv)
{
   try
   {
      namespace po = boost::program_options;
      std::string config, pidfile;
      bool debug = false;

      boost::filesystem::path command(argv[0]);

      po::options_description od("options");

      od.add_options()
         ("config,c", po::value<std::string>(&config)->default_value("/etc/aird.cfg"), "configuration file")
         ("pidfile", po::value<std::string>(&pidfile)->default_value("/var/run/aird.pid"), "pid file location")
         ("debug,d", po::value<bool>(&debug)->zero_tokens(), "run in foreground")
         ;

      try
      {
         po::variables_map vm;
         po::store(po::parse_command_line(argc, argv, od), vm);
         po::notify(vm);
      }
      catch (const std::exception& e)
      {
         std::cerr << "error: " << e.what() << "\n\n" << od << "\n";
         return 1;
      }

      aird::settings set(config);
      aird::daemon daemon(set, pidfile);

      return daemon.run(command.filename().native(), debug);
   }
   catch (const std::exception& e)
   {
      std::cerr << "fatal: " << e.what() << std::endl;
   }
   catch (...)
   {
      std::cerr << "fatal: unknown exception caught" << std::endl;
   }

   return 1;
}
