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

#ifndef AIRD_MONITOR_H_
#define AIRD_MONITOR_H_

#include <vector>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>

namespace aird {

class root_logger;
class monitor_impl;
class event_handler;
class status_provider;

class monitor
{
public:
   struct settings
   {
      struct brightness
      {
         double exponent;
         double delta;
         double delta_slow;
      };

      struct power_mode
      {
         unsigned idle_timeout;
         unsigned display_backlight_idle_level;
         unsigned keyboard_backlight_idle_level;

         unsigned fan_hot_delay;
         unsigned fan_cold_delay;
         unsigned fan_speed_min;
         unsigned fan_speed_max;
         unsigned fan_speed_delta;
         double fan_temp_min;
         double fan_temp_delta;

         unsigned cpu_hot_delay;
         unsigned cpu_cold_delay;
         double cpu_temp_hot;
         double cpu_temp_cold;
         unsigned cpu_throttle_delay;
         unsigned cpu_unthrottle_delay;
         unsigned cpu_max_speed;
      };

      void add_options(boost::program_options::options_description& od);

      std::string hwmon_base_path;
      std::string intel_backlight_path;
      std::string battery_path;
      std::string ac_path;
      std::string cpu_base_path;
      brightness display_backlight;
      brightness keyboard_backlight;
      unsigned check_interval;
      unsigned power_interval;
      unsigned power_measurements;
      power_mode on_ac;
      power_mode on_battery;

      double powersave_min_energy_percent;
      unsigned powersave_cpu_max_speed;

      unsigned min_safe_display_backlight;
   };

   monitor(boost::asio::io_service& ios, root_logger& root, const settings& set);
   void start();
   void stop();
   void ensure_safe_defaults();

   boost::shared_ptr<event_handler> get_event_handler();
   boost::shared_ptr<status_provider> get_status_provider();

private:
   boost::shared_ptr<monitor_impl> m_impl;
};

}

#endif
