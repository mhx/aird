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

#ifndef AIRD_MOUSE_DEVICE_H_
#define AIRD_MOUSE_DEVICE_H_

#include <string>

#include <boost/asio.hpp>

#include "input_device.h"
#include "log.h"

namespace aird {

class mouse_device : public input_device
{
public:
   mouse_device(boost::asio::io_service& ios, root_logger& root, int fd, const std::string& name);
   virtual void start(boost::shared_ptr<event_handler> handler);
   virtual void stop();
   virtual void handle_input_event(const boost::system::error_code& error, size_t bytes_read);

private:
   void read_next_event();

   boost::asio::posix::basic_stream_descriptor<> m_dev;
   char m_buffer[32];
   boost::shared_ptr<event_handler> m_handler;
   logger m_log;
   bool m_stopped;
};

}

#endif
