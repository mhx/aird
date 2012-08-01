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

#include <linux/input.h>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "event_handler.h"
#include "mouse_device.h"

namespace aird {

mouse_device::mouse_device(boost::asio::io_service& ios, root_logger& root, int fd, const std::string& name)
   : m_dev(ios, fd)
   , m_log(root, "mouse_device(" + name + ")")
   , m_stopped(true)
{
}

void mouse_device::start(boost::shared_ptr<event_handler> handler)
{
   LINFO(m_log, "starting");
   m_stopped = false;
   m_handler = handler;
   read_next_event();
}

void mouse_device::stop()
{
   if (!m_stopped)
   {
      m_stopped = true;
      m_dev.close();
   }
}

void mouse_device::read_next_event()
{
   m_dev.async_read_some(boost::asio::buffer(m_buffer, sizeof(m_buffer)),
      boost::bind(&input_device::handle_input_event, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void mouse_device::handle_input_event(const boost::system::error_code& e, size_t bytes_read)
{
   if (e)
   {
      if (m_stopped && e == boost::asio::error::operation_aborted)
      {
         LINFO(m_log, "stopped");
      }
      else
      {
         LERROR(m_log, "async read failed: " << e.message());
      }

      return;
   }

   LTRACE(m_log, bytes_read << " bytes read");

   m_handler->handle_event(event_code::MOUSE_ACTIVITY);

   read_next_event();
}

}
