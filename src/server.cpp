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

#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>

#include "log.h"
#include "server.h"

namespace aird {

namespace {

class connection : public boost::enable_shared_from_this<connection>
{
public:
   connection(boost::asio::io_service& ios, root_logger& root)
      : m_socket(ios)
      , m_log(root, "connection")
   {
   }

   boost::asio::ip::tcp::socket& socket()
   {
      return m_socket;
   }

   void start(boost::shared_ptr<status_provider> provider)
   {
      std::ostream os(&m_buffer);

      try
      {
         provider->status(os);
      }
      catch (const std::exception& e)
      {
         os.seekp(0);
         os << "error while getting status: " << e.what() << '\n';
      }

      boost::asio::async_write(m_socket, m_buffer,
          boost::bind(&connection::handle_write, shared_from_this(),
            boost::asio::placeholders::error));
   }

private:
   void handle_write(const boost::system::error_code& error)
   {
      if (error)
      {
         LERROR(m_log, "error during write: " << error.message());
      }

      m_socket.close();
   }

   boost::asio::ip::tcp::socket m_socket;
   boost::asio::streambuf m_buffer;
   logger m_log;
};

}

class server_impl
{
public:
   server_impl(boost::asio::io_service& ios, root_logger& root, const server::settings& set);
   void start(boost::shared_ptr<status_provider> provider);
   void stop();

private:
   void start_accept();
   void handle_accept(boost::shared_ptr<connection> new_conn, const boost::system::error_code& error);

   boost::asio::io_service& m_ios;
   boost::asio::ip::tcp::acceptor m_acceptor;
   boost::asio::deadline_timer m_accept_timer;
   boost::shared_ptr<status_provider> m_provider;
   bool m_stopped;
   logger m_log;
};

status_provider::~status_provider()
{
}

server_impl::server_impl(boost::asio::io_service& ios, root_logger& root, const server::settings& set)
   : m_ios(ios)
   , m_acceptor(ios, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), set.port))
   , m_accept_timer(ios)
   , m_stopped(true)
   , m_log(root, "server")
{
   m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
}

void server_impl::start(boost::shared_ptr<status_provider> provider)
{
   m_stopped = false;
   m_provider = provider;
   start_accept();
}

void server_impl::stop()
{
   if (!m_stopped)
   {
      m_stopped = true;
      m_accept_timer.cancel();
      m_acceptor.close();
   }
}

void server_impl::start_accept()
{
   if (!m_stopped)
   {
      boost::shared_ptr<connection> new_conn(new connection(m_ios, m_log.root()));
      m_acceptor.async_accept(new_conn->socket(),
          boost::bind(&server_impl::handle_accept, this, new_conn,
            boost::asio::placeholders::error));
   }
}

void server_impl::handle_accept(boost::shared_ptr<connection> new_conn, const boost::system::error_code& e)
{
   if (e)
   {
      if (m_stopped && e == boost::asio::error::operation_aborted)
      {
         LINFO(m_log, "stopped");
      }
      else
      {
         LERROR(m_log, "accept failed: " << e.message());
         m_accept_timer.expires_from_now(boost::posix_time::milliseconds(1000));
         m_accept_timer.async_wait(boost::bind(&server_impl::start_accept, this));
      }
   }
   else
   {
      new_conn->start(m_provider);
      start_accept();
   }
}

void server::settings::add_options(boost::program_options::options_description& od)
{
   using namespace boost::program_options;

   od.add_options()
      ("server.port", value<uint16_t>(&port)->default_value(21577))
      ;
}

server::server(boost::asio::io_service& ios, root_logger& root, const settings& set)
   : m_impl(new server_impl(ios, root, set))
{
}

void server::start(boost::shared_ptr<status_provider> provider)
{
   m_impl->start(provider);
}

void server::stop()
{
   m_impl->stop();
}

}
