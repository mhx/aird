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

#ifndef AIRD_SERVER_H_
#define AIRD_SERVER_H_

#include <ostream>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

namespace aird {

class root_logger;
class server_impl;

class status_provider
{
public:
   virtual ~status_provider();
   virtual void status(std::ostream& os) const = 0;
};

class server
{
public:
   struct settings
   {
      void add_options(boost::program_options::options_description& od);

      uint16_t port;
   };

   server(boost::asio::io_service& ios, root_logger& root, const settings& set);
   void start(boost::shared_ptr<status_provider> provider);
   void stop();

private:
   boost::shared_ptr<server_impl> m_impl;
};

}

#endif
