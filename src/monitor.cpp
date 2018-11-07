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

#include <deque>
#include <fstream>
#include <sstream>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "event_handler.h"
#include "log.h"
#include "monitor.h"
#include "server.h"

namespace aird {

namespace {

class object
{
public:
   object(const boost::filesystem::path& path)
      : m_path(path)
   {
   }

   template <typename T>
   T get() const
   {
      std::string tmp;
      readline(tmp);
      return boost::lexical_cast<T>(tmp);
   }

   template <typename T>
   void set(const T& value) const
   {
      writeline(boost::lexical_cast<std::string>(value));
   }

   bool exists() const
   {
      return boost::filesystem::exists(m_path);
   }

   const boost::filesystem::path& path() const
   {
      return m_path;
   }

private:
   void readline(std::string& line) const
   {
      std::ifstream ifs(m_path.c_str());

      if (!ifs)
      {
         throw std::runtime_error("cannot open file: " + m_path.native());
      }

      std::getline(ifs, line);
   }

   void writeline(const std::string& line) const
   {
      std::ofstream ofs(m_path.c_str());

      if (!ofs)
      {
         throw std::runtime_error("cannot open file: " + m_path.native());
      }

      ofs << line << '\n';
   }

   boost::filesystem::path m_path;
};

class device
{
public:
   device(const std::string& basepath, const std::string& name)
   {
      std::deque<boost::filesystem::path> dirs;
      dirs.emplace_back(basepath);

      while (!dirs.empty())
      {
         auto path = dirs.front();
         dirs.pop_front();

         try
         {
            if (object(path / "name").get<std::string>() == name)
            {
               m_path = path;
               return;
            }
         }
         catch (...)
         {
         }

         try
         {
            for (boost::filesystem::directory_iterator it(path); it != boost::filesystem::directory_iterator(); ++it)
            {
               dirs.emplace_back(*it);
            }
         }
         catch (...)
         {
         }
      }

      throw std::runtime_error("cannot find device: " + name);
   }

   const boost::filesystem::path& path() const
   {
      return m_path;
   }

private:
   boost::filesystem::path m_path;
};

class temp
{
public:
   static void get_object_range(const boost::filesystem::path& path, size_t& beg, size_t& end)
   {
      beg = end = 1;

      while (object(path / ("temp" + boost::lexical_cast<std::string>(end) + "_label")).exists())
      {
         ++end;
      }
   }

   temp(const boost::filesystem::path& path, size_t index)
      : m_crit(path / ("temp" + boost::lexical_cast<std::string>(index) + "_crit"))
      , m_input(path / ("temp" + boost::lexical_cast<std::string>(index) + "_input"))
      , m_label(path / ("temp" + boost::lexical_cast<std::string>(index) + "_label"))
      , m_max(path / ("temp" + boost::lexical_cast<std::string>(index) + "_max"))
   {
   }

   double crit() const
   {
      return 1e-3*m_crit.get<double>();
   }

   double input() const
   {
      return 1e-3*m_input.get<double>();
   }

   std::string label() const
   {
      return m_label.get<std::string>();
   }

   double max() const
   {
      return 1e-3*m_max.get<double>();
   }

private:
   object m_crit;
   object m_input;
   object m_label;
   object m_max;
};

class coretemp
{
public:
   typedef std::vector<temp>::const_iterator const_iterator;

   coretemp(const std::string& basepath)
      : m_dev(basepath, "coretemp")
   {
      size_t beg, end;

      temp::get_object_range(m_dev.path(), beg, end);

      while (beg != end)
      {
         m_temp.push_back(temp(m_dev.path(), beg++));
      }
   }

   const_iterator begin() const
   {
      return m_temp.begin();
   }

   const_iterator end() const
   {
      return m_temp.end();
   }

   void dump(std::ostream& os) const
   {
      for (const_iterator it = begin(); it != end(); ++it)
      {
         os << it->label() << ": " << it->input() << "°C (max: " << it->max() << "°C, crit: " << it->crit() << "°C)\n";
      }
   }

   double current_max_temp() const
   {
      double cur = -300.0;

      for (const_iterator it = begin(); it != end(); ++it)
      {
         cur = std::max(cur, it->input());
      }

      return cur;
   }

   const boost::filesystem::path& path() const
   {
      return m_dev.path();
   }

private:
   device m_dev;
   std::vector<temp> m_temp;
};

class fan
{
public:
   static void get_object_range(const boost::filesystem::path& path, size_t& beg, size_t& end)
   {
      beg = end = 1;

      while (object(path / ("fan" + boost::lexical_cast<std::string>(end) + "_label")).exists())
      {
         ++end;
      }
   }

   fan(const boost::filesystem::path& path, size_t index)
      : m_input(path / ("fan" + boost::lexical_cast<std::string>(index) + "_input"))
      , m_label(path / ("fan" + boost::lexical_cast<std::string>(index) + "_label"))
      , m_manual(path / ("fan" + boost::lexical_cast<std::string>(index) + "_manual"))
      , m_max(path / ("fan" + boost::lexical_cast<std::string>(index) + "_max"))
      , m_min(path / ("fan" + boost::lexical_cast<std::string>(index) + "_min"))
      , m_output(path / ("fan" + boost::lexical_cast<std::string>(index) + "_output"))
   {
   }

   double input() const
   {
      return m_input.get<double>();
   }

   std::string label() const
   {
      return m_label.get<std::string>();
   }

   bool manual() const
   {
      return m_manual.get<bool>();
   }

   double max() const
   {
      return m_max.get<double>();
   }

   double min() const
   {
      return m_min.get<double>();
   }

   double output() const
   {
      return m_output.get<double>();
   }

   void set_manual(bool value) const
   {
      m_manual.set(value);
   }

   void set_output(unsigned value) const
   {
      m_output.set(value);
   }

private:
   object m_input;
   object m_label;
   object m_manual;
   object m_max;
   object m_min;
   object m_output;
};

class light
{
public:
   light(const boost::filesystem::path& path)
      : m_obj(path / "light")
   {
   }

   unsigned value() const
   {
      std::string val = m_obj.get<std::string>();
      unsigned left, right;
      ::sscanf(val.c_str(), "(%u,%u)", &left, &right);
      return left + right;
   }

private:
   object m_obj;
};

class cpu
{
public:
   cpu(const boost::filesystem::path& path)
      : m_bios_limit(path / "cpufreq" / "bios_limit")
      , m_cpuinfo_cur_freq(path / "cpufreq" / "cpuinfo_cur_freq")
      , m_cpuinfo_max_freq(path / "cpufreq" / "cpuinfo_max_freq")
      , m_cpuinfo_min_freq(path / "cpufreq" / "cpuinfo_min_freq")
      , m_scaling_available_frequencies(path / "cpufreq" / "scaling_available_frequencies")
      , m_scaling_cur_freq(path / "cpufreq" / "scaling_cur_freq")
      , m_scaling_max_freq(path / "cpufreq" / "scaling_max_freq")
      , m_scaling_min_freq(path / "cpufreq" / "scaling_min_freq")
      , m_scaling_governor(path / "cpufreq" / "scaling_governor")
      , m_core_id(path / "topology" / "core_id")
   {
   }

   bool configurable() const
   {
      return m_scaling_available_frequencies.exists();
   }

   unsigned bios_limit() const
   {
      return m_bios_limit.get<unsigned>();
   }

   unsigned cpuinfo_cur_freq() const
   {
      return m_cpuinfo_cur_freq.get<unsigned>();
   }

   unsigned cpuinfo_min_freq() const
   {
      return m_cpuinfo_min_freq.get<unsigned>();
   }

   unsigned cpuinfo_max_freq() const
   {
      return m_cpuinfo_max_freq.get<unsigned>();
   }

   unsigned scaling_cur_freq() const
   {
      return m_scaling_cur_freq.get<unsigned>();
   }

   unsigned scaling_min_freq() const
   {
      return m_scaling_min_freq.get<unsigned>();
   }

   unsigned scaling_max_freq() const
   {
      return m_scaling_max_freq.get<unsigned>();
   }

   std::string scaling_governor() const
   {
      return m_scaling_governor.get<std::string>();
   }

   template <typename OutputIterator>
   void scaling_available_frequencies(OutputIterator out) const
   {
      std::string freq = m_scaling_available_frequencies.get<std::string>();
      std::istringstream iss(freq);
      std::istream_iterator<unsigned> in(iss);
      std::copy(in, std::istream_iterator<unsigned>(), out);
   }

   unsigned core_id() const
   {
      return m_core_id.get<unsigned>();
   }

   void set_scaling_max_freq(unsigned value) const
   {
      m_scaling_max_freq.set(value);
   }

private:
   object m_bios_limit;
   object m_cpuinfo_cur_freq;
   object m_cpuinfo_max_freq;
   object m_cpuinfo_min_freq;
   object m_scaling_available_frequencies;
   object m_scaling_cur_freq;
   object m_scaling_max_freq;
   object m_scaling_min_freq;
   object m_scaling_governor;
   object m_core_id;
};

class cpuinfo
{
public:
   typedef std::vector<cpu>::const_iterator const_iterator;

   cpuinfo(const std::string& basepath)
   {
      boost::filesystem::path path(basepath);

      for (size_t ix = 0; ; ++ix)
      {
         object obj(path / ("cpu" + boost::lexical_cast<std::string>(ix)));

         if (!obj.exists())
         {
            break;
         }

         m_cpu.push_back(cpu(obj.path()));
      }
   }

   bool configurable() const
   {
     return !m_cpu.empty() && m_cpu.front().configurable();
   }

   const_iterator begin() const
   {
      return m_cpu.begin();
   }

   const_iterator end() const
   {
      return m_cpu.end();
   }

   static std::string freq2str(unsigned value)
   {
      std::ostringstream oss;
      double fv = value;
      static const char *prefix[] = { "k", "M", "G", "T" };
      size_t ix = 0;
      while (fv >= 1000.0 && ix < sizeof(prefix)/sizeof(prefix[0]) - 1)
      {
         fv /= 1000.0;
         ++ix;
      }
      oss << fv << ' ' << prefix[ix] << "Hz";
      return oss.str();
   }

   void dump(std::ostream& os) const
   {
      for (const_iterator it = begin(); it != end(); ++it)
      {
         os << "Core " << it->core_id() << ": " << freq2str(it->scaling_cur_freq()) << " (" << it->scaling_governor()
            << ", max: " << freq2str(it->scaling_max_freq()) << ")\n";
      }
   }

   unsigned scaling_max_freq() const
   {
      unsigned freq = 0;

      for (const_iterator it = begin(); it != end(); ++it)
      {
         freq = std::max(freq, it->scaling_max_freq());
      }

      return freq;
   }

   void set_scaling_max_freq(unsigned value) const
   {
      for (const_iterator it = begin(); it != end(); ++it)
      {
         it->set_scaling_max_freq(value);
      }
   }

   template <typename OutputIterator>
   void scaling_available_frequencies(OutputIterator out) const
   {
      m_cpu.front().scaling_available_frequencies(out);
   }

private:
   std::vector<cpu> m_cpu;
};

class power
{
public:
   power(const boost::filesystem::path& path)
      : m_online(path / "online")
      , m_present(path / "present")
      , m_type(path / "type")
      , m_energy_full(path / "charge_full")
      , m_energy_full_design(path / "charge_full_design")
      , m_energy_now(path / "charge_now")
      , m_voltage_min_design(path / "voltage_min_design")
      , m_voltage_now(path / "voltage_now")
      , m_power_now(path / "power_now")
   {
   }

   bool online() const
   {
      return m_online.get<bool>();
   }

   bool present() const
   {
      return m_present.get<bool>();
   }

   std::string type() const
   {
      return m_type.get<std::string>();
   }

   double energy_full() const
   {
      return 1e-6*m_energy_full.get<double>();
   }

   double energy_full_design() const
   {
      return 1e-6*m_energy_full_design.get<double>();
   }

   double energy_now() const
   {
      return 1e-6*m_energy_now.get<double>();
   }

   double voltage_min_design() const
   {
      return 1e-6*m_voltage_min_design.get<double>();
   }

   double voltage_now() const
   {
      return 1e-6*m_voltage_now.get<double>();
   }

   double power_now() const
   {
      return 1e-6*m_power_now.get<double>();
   }

private:
   object m_online;
   object m_present;
   object m_type;
   object m_energy_full;
   object m_energy_full_design;
   object m_energy_now;
   object m_voltage_min_design;
   object m_voltage_now;
   object m_power_now;
};

class led
{
public:
   led(const boost::filesystem::path& path)
      : m_actual_brightness(path / "actual_brightness")
      , m_brightness(path / "brightness")
      , m_max_brightness(path / "max_brightness")
   {
   }

   unsigned actual_brightness() const
   {
      return m_actual_brightness.get<unsigned>();
   }

   unsigned brightness() const
   {
      return m_brightness.get<unsigned>();
   }

   unsigned max_brightness() const
   {
      return m_max_brightness.get<unsigned>();
   }

   void set_brightness(unsigned brightness) const
   {
      m_brightness.set(brightness);
   }

private:
   object m_actual_brightness;
   object m_brightness;
   object m_max_brightness;
};

class applesmc
{
public:
   typedef std::vector<fan>::const_iterator const_fan_iterator;
   typedef std::vector<temp>::const_iterator const_temp_iterator;

   applesmc(const std::string& basepath)
      : m_dev(basepath, "applesmc")
      , m_light(m_dev.path())
      , m_kbd_backlight(m_dev.path() / "leds" / "smc::kbd_backlight")
   {
      size_t beg, end;

      fan::get_object_range(m_dev.path(), beg, end);

      while (beg != end)
      {
         m_fan.push_back(fan(m_dev.path(), beg++));
      }

      temp::get_object_range(m_dev.path(), beg, end);

      while (beg != end)
      {
         temp t(m_dev.path(), beg++);
         m_tmap[t.label()] = m_temp.size();
         m_temp.push_back(t);
      }
   }

   void dump(std::ostream& os) const
   {
      for (const_fan_iterator it = fan_begin(); it != fan_end(); ++it)
      {
         os << it->label() << ": " << it->input() << " rpm (" << it->output() << " rpm) [" << (it->manual() ? "MANUAL" : "AUTO") << "]\n";
      }

      os << "Palm Rest: " << get_temp("Ts0P").input() << "°C\n";
      os << "Ambient Light: " << m_light.value() << "\n";
      os << "Keyboard Backlight: " << m_kbd_backlight.brightness() << "/" << m_kbd_backlight.max_brightness() << "\n";
   }

   void set_fan_speed(unsigned value) const
   {
      for (const_fan_iterator it = fan_begin(); it != fan_end(); ++it)
      {
         if (!it->manual())
         {
            it->set_manual(true);
         }

         if (it->output() != value)
         {
            it->set_output(value);
         }
      }
   }

   const_fan_iterator fan_begin() const
   {
      return m_fan.begin();
   }

   const_fan_iterator fan_end() const
   {
      return m_fan.end();
   }

   const_temp_iterator temp_begin() const
   {
      return m_temp.begin();
   }

   const_temp_iterator temp_end() const
   {
      return m_temp.end();
   }

   const temp& get_temp(const std::string& name) const
   {
      std::map<std::string, size_t>::const_iterator it = m_tmap.find(name);

      if (it != m_tmap.end())
      {
         return m_temp[it->second];
      }

      throw std::runtime_error("no such temp sensor: " + name);
   }

   const led& keyboard_backlight() const
   {
      return m_kbd_backlight;
   }

   const boost::filesystem::path& path() const
   {
      return m_dev.path();
   }

private:
   device m_dev;
   std::vector<fan> m_fan;
   std::vector<temp> m_temp;
   std::map<std::string, size_t> m_tmap;
   light m_light;
   led m_kbd_backlight;
};

}

class monitor_impl : public boost::enable_shared_from_this<monitor_impl>
                   , public event_handler
                   , public status_provider
{
public:
   monitor_impl(boost::asio::io_service& ios, root_logger& root, const monitor::settings& set);
   void start();
   void stop();
   void ensure_safe_defaults();

   virtual void handle_event(event_code::type code);
   virtual void status(std::ostream& os) const;

private:
   void on_periodic_check(const boost::system::error_code& e);
   void on_idle(const boost::system::error_code& e);

   void restart_periodic_check();
   void restart_idle();

   void enter_idle(unsigned level);
   void leave_idle();

   const monitor::settings::power_mode& power_settings() const;

   void update_stats();
   void run_checks();
   void check_fan();
   void check_cpu();

   double current_power() const;
   unsigned cpu_max_speed() const;

   int calc_brightness(const monitor::settings::brightness& set, int cur, int max, bool up, bool slow) const;
   void set_display_brightness(bool up, bool slow);
   void set_keyboard_brightness(bool up, bool slow);

   static const size_t HISTORY_LENGTH = 300;  // 5 minutes

   boost::asio::io_service& m_ios;
   boost::asio::deadline_timer m_timer;
   boost::asio::deadline_timer m_idle_timer;
   coretemp m_coretemp;
   applesmc m_applesmc;
   cpuinfo m_cpuinfo;
   led m_backlight;
   power m_ac;
   power m_battery;
   unsigned m_original_display_backlight;
   unsigned m_original_keyboard_backlight;
   unsigned m_idle_level;
   unsigned m_saved_display_backlight;
   unsigned m_saved_keyboard_backlight;
   bool m_on_ac;
   std::vector<double> m_temp_history;
   std::vector<double> m_energy_history;
   size_t m_history_size;
   size_t m_history_count;
   double m_fan_temp;
   double m_fan_hot;
   double m_fan_cold;
   double m_cpu_temp;
   double m_cpu_hot;
   double m_cpu_cold;
   size_t m_cpu_throttle_time;
   size_t m_cpu_unthrottle_time;
   const monitor::settings m_set;
   logger m_log;
   bool m_stopped;
};

void monitor::settings::add_options(boost::program_options::options_description& od)
{
   using namespace boost::program_options;

   od.add_options()
      ("monitor.hwmon_base_path", value<std::string>(&hwmon_base_path)->default_value("/sys/devices/platform"))
      ("monitor.intel_backlight_path", value<std::string>(&intel_backlight_path)->default_value("/sys/class/backlight/intel_backlight"))
      ("monitor.battery_path", value<std::string>(&battery_path)->default_value("/sys/class/power_supply/BAT0"))
      ("monitor.ac_path", value<std::string>(&ac_path)->default_value("/sys/class/power_supply/ADP1"))
      ("monitor.cpu_base_path", value<std::string>(&cpu_base_path)->default_value("/sys/bus/cpu/devices"))

      ("monitor.check_interval", value<unsigned>(&check_interval)->default_value(1))
      ("monitor.power_interval", value<unsigned>(&power_interval)->default_value(30))
      ("monitor.power_measurements", value<unsigned>(&power_measurements)->default_value(3))
      ("monitor.idle_timeout:ac", value<unsigned>(&on_ac.idle_timeout)->default_value(120))
      ("monitor.idle_timeout:battery", value<unsigned>(&on_battery.idle_timeout)->default_value(30))

      ("display_backlight.exponent", value<double>(&display_backlight.exponent)->default_value(4.0))
      ("display_backlight.delta", value<double>(&display_backlight.delta)->default_value(0.2))
      ("display_backlight.delta_slow", value<double>(&display_backlight.delta_slow)->default_value(0.05))
      ("display_backlight.idle_level:ac", value<unsigned>(&on_ac.display_backlight_idle_level)->default_value(100))
      ("display_backlight.idle_level:battery", value<unsigned>(&on_battery.display_backlight_idle_level)->default_value(50))
      ("display_backlight.min_safe_level", value<unsigned>(&min_safe_display_backlight)->default_value(50))

      ("keyboard_backlight.exponent", value<double>(&keyboard_backlight.exponent)->default_value(2.0))
      ("keyboard_backlight.delta", value<double>(&keyboard_backlight.delta)->default_value(1.0))
      ("keyboard_backlight.delta_slow", value<double>(&keyboard_backlight.delta_slow)->default_value(0.25))
      ("keyboard_backlight.idle_level:ac", value<unsigned>(&on_ac.keyboard_backlight_idle_level)->default_value(0))
      ("keyboard_backlight.idle_level:battery", value<unsigned>(&on_battery.keyboard_backlight_idle_level)->default_value(0))

      ("fan.hot_delay:ac", value<unsigned>(&on_ac.fan_hot_delay)->default_value(40))
      ("fan.cold_delay:ac", value<unsigned>(&on_ac.fan_cold_delay)->default_value(20))
      ("fan.speed_min:ac", value<unsigned>(&on_ac.fan_speed_min)->default_value(2000))
      ("fan.speed_max:ac", value<unsigned>(&on_ac.fan_speed_max)->default_value(6500))
      ("fan.speed_delta:ac", value<unsigned>(&on_ac.fan_speed_delta)->default_value(500))
      ("fan.temp_min:ac", value<double>(&on_ac.fan_temp_min)->default_value(40.0))
      ("fan.temp_delta:ac", value<double>(&on_ac.fan_temp_delta)->default_value(5.0))

      ("fan.hot_delay:battery", value<unsigned>(&on_battery.fan_hot_delay)->default_value(40))
      ("fan.cold_delay:battery", value<unsigned>(&on_battery.fan_cold_delay)->default_value(20))
      ("fan.speed_min:battery", value<unsigned>(&on_battery.fan_speed_min)->default_value(2000))
      ("fan.speed_max:battery", value<unsigned>(&on_battery.fan_speed_max)->default_value(6500))
      ("fan.speed_delta:battery", value<unsigned>(&on_battery.fan_speed_delta)->default_value(500))
      ("fan.temp_min:battery", value<double>(&on_battery.fan_temp_min)->default_value(40.0))
      ("fan.temp_delta:battery", value<double>(&on_battery.fan_temp_delta)->default_value(5.0))

      ("cpu.hot_delay:ac", value<unsigned>(&on_ac.cpu_hot_delay)->default_value(10))
      ("cpu.cold_delay:ac", value<unsigned>(&on_ac.cpu_cold_delay)->default_value(20))
      ("cpu.temp_hot:ac", value<double>(&on_ac.cpu_temp_hot)->default_value(90.0))
      ("cpu.temp_cold:ac", value<double>(&on_ac.cpu_temp_cold)->default_value(70.0))
      ("cpu.throttle_delay:ac", value<unsigned>(&on_ac.cpu_throttle_delay)->default_value(10))
      ("cpu.unthrottle_delay:ac", value<unsigned>(&on_ac.cpu_unthrottle_delay)->default_value(10))
      ("cpu.max_speed:ac", value<unsigned>(&on_ac.cpu_max_speed)->default_value(2000000))

      ("cpu.hot_delay:battery", value<unsigned>(&on_battery.cpu_hot_delay)->default_value(10))
      ("cpu.cold_delay:battery", value<unsigned>(&on_battery.cpu_cold_delay)->default_value(20))
      ("cpu.temp_hot:battery", value<double>(&on_battery.cpu_temp_hot)->default_value(90.0))
      ("cpu.temp_cold:battery", value<double>(&on_battery.cpu_temp_cold)->default_value(70.0))
      ("cpu.throttle_delay:battery", value<unsigned>(&on_battery.cpu_throttle_delay)->default_value(10))
      ("cpu.unthrottle_delay:battery", value<unsigned>(&on_battery.cpu_unthrottle_delay)->default_value(10))
      ("cpu.max_speed:battery", value<unsigned>(&on_battery.cpu_max_speed)->default_value(1600000))

      ("powersave.min_energy_percent", value<double>(&powersave_min_energy_percent)->default_value(10.0))
      ("powersave.cpu_max_speed", value<unsigned>(&powersave_cpu_max_speed)->default_value(1000000))
      ;
}

monitor::monitor(boost::asio::io_service& ios, root_logger& root, const settings& set)
   : m_impl(new monitor_impl(ios, root, set))
{
}

void monitor::start()
{
   m_impl->start();
}

void monitor::stop()
{
   m_impl->stop();
}

void monitor::ensure_safe_defaults()
{
   m_impl->ensure_safe_defaults();
}

boost::shared_ptr<event_handler> monitor::get_event_handler()
{
   return m_impl;
}

boost::shared_ptr<status_provider> monitor::get_status_provider()
{
   return m_impl;
}

monitor_impl::monitor_impl(boost::asio::io_service& ios, root_logger& root, const monitor::settings& set)
   : m_ios(ios)
   , m_timer(ios)
   , m_idle_timer(ios)
   , m_coretemp(set.hwmon_base_path)
   , m_applesmc(set.hwmon_base_path)
   , m_cpuinfo(set.cpu_base_path)
   , m_backlight(set.intel_backlight_path)
   , m_ac(set.ac_path)
   , m_battery(set.battery_path)
   , m_original_display_backlight(m_backlight.brightness())
   , m_original_keyboard_backlight(m_applesmc.keyboard_backlight().brightness())
   , m_idle_level(0)
   , m_saved_display_backlight(0)
   , m_saved_keyboard_backlight(0)
   , m_on_ac(m_ac.online())
   , m_history_size((HISTORY_LENGTH + set.check_interval - 1)/set.check_interval)
   , m_history_count(0)
   , m_fan_temp(-300.0)
   , m_fan_hot(0.0)
   , m_fan_cold(0.0)
   , m_cpu_temp(-300.0)
   , m_cpu_hot(0.0)
   , m_cpu_cold(0.0)
   , m_cpu_throttle_time(0)
   , m_cpu_unthrottle_time(0)
   , m_set(set)
   , m_log(root, "monitor")
   , m_stopped(true)
{
   m_energy_history.resize(m_history_size);
   m_temp_history.resize(m_history_size);

   LINFO(m_log, "coretemp path: " << m_coretemp.path());
   LINFO(m_log, "applesmc path: " << m_applesmc.path());
}

void monitor_impl::start()
{
   m_stopped = false;
   restart_periodic_check();
   restart_idle();
}

void monitor_impl::stop()
{
   if (!m_stopped)
   {
      m_stopped = true;
      m_timer.cancel();
      m_idle_timer.cancel();
   }
}

void monitor_impl::ensure_safe_defaults()
{
   LINFO(m_log, "setting safe defaults");

   const led& keyboard_backlight = m_applesmc.keyboard_backlight();
   unsigned backlight = std::max(m_original_display_backlight, m_set.min_safe_display_backlight);

   LDEBUG(m_log, "display: " << backlight << " (" << m_original_display_backlight << "), keyboard: " << m_original_keyboard_backlight);

   if (m_backlight.brightness() < backlight)
   {
      m_backlight.set_brightness(backlight);
   }

   if (keyboard_backlight.brightness() < m_original_keyboard_backlight)
   {
      keyboard_backlight.set_brightness(m_original_keyboard_backlight);
   }
}

void monitor_impl::restart_periodic_check()
{
   m_timer.expires_from_now(boost::posix_time::seconds(m_set.check_interval));
   m_timer.async_wait(boost::bind(&monitor_impl::on_periodic_check, shared_from_this(), boost::asio::placeholders::error));
}

void monitor_impl::restart_idle()
{
   m_idle_timer.expires_from_now(boost::posix_time::seconds(power_settings().idle_timeout));
   m_idle_timer.async_wait(boost::bind(&monitor_impl::on_idle, shared_from_this(), boost::asio::placeholders::error));
}

void monitor_impl::update_stats()
{
   m_on_ac = m_ac.online();

   size_t index = ++m_history_count % m_history_size;

   m_temp_history[index] = m_coretemp.current_max_temp();
   m_energy_history[index] = m_battery.energy_now();
}

void monitor_impl::check_fan()
{
   const monitor::settings::power_mode& power_set = power_settings();

   if (m_fan_hot > m_fan_temp)
   {
      m_fan_temp = m_fan_hot;
   }
   else if (m_fan_cold < m_fan_temp)
   {
      m_fan_temp = m_fan_cold;
   }

   unsigned fan_ix = (m_fan_temp - power_set.fan_temp_min)/power_set.fan_temp_delta;
   unsigned fan_speed = std::min(power_set.fan_speed_min + fan_ix*power_set.fan_speed_delta, power_set.fan_speed_max);

   LDEBUG(m_log, "fan_speed=" << fan_speed);

   m_applesmc.set_fan_speed(fan_speed);
}

unsigned monitor_impl::cpu_max_speed() const
{
   if (!m_on_ac)
   {
      if (100.0*m_battery.energy_now()/m_battery.energy_full() < m_set.powersave_min_energy_percent)
      {
         return m_set.powersave_cpu_max_speed;
      }
   }

   return power_settings().cpu_max_speed;
}

void monitor_impl::check_cpu()
{
   const monitor::settings::power_mode& power_set = power_settings();

   if (m_cpu_hot > m_cpu_temp)
   {
      m_cpu_temp = m_cpu_hot;
   }
   else if (m_cpu_cold < m_cpu_temp)
   {
      m_cpu_temp = m_cpu_cold;
   }

   bool throttle = false;
   bool unthrottle = false;

   if (m_cpu_throttle_time == 0)
   {
      if (m_cpu_temp > power_set.cpu_temp_hot)
      {
         throttle = true;
      }
   }
   else
   {
      if (m_cpu_throttle_time > m_set.check_interval)
      {
         m_cpu_throttle_time -= m_set.check_interval;
      }
      else
      {
         m_cpu_throttle_time = 0;
      }
   }

   if (m_cpu_unthrottle_time == 0)
   {
      if (m_cpu_temp < power_set.cpu_temp_cold)
      {
         unthrottle = true;
      }
   }
   else
   {
      if (m_cpu_unthrottle_time > m_set.check_interval)
      {
         m_cpu_unthrottle_time -= m_set.check_interval;
      }
      else
      {
         m_cpu_unthrottle_time = 0;
      }
   }

   std::vector<unsigned> available;
   m_cpuinfo.scaling_available_frequencies(std::back_inserter(available));
   std::sort(available.begin(), available.end());

   unsigned current = m_cpuinfo.scaling_max_freq();
   size_t ix = std::lower_bound(available.begin(), available.end(), current) - available.begin();
   size_t max_ix = std::lower_bound(available.begin(), available.end(), cpu_max_speed()) - available.begin();
   if (max_ix >= available.size())
   {
      max_ix = available.size() - 1;
   }

   size_t new_ix = std::min(ix, max_ix);

   if (throttle)
   {
      if (new_ix == ix && new_ix > 0)
      {
         --new_ix;
      }
   }
   else if (unthrottle)
   {
      if (new_ix < max_ix)
      {
         ++new_ix;
      }
   }

   LDEBUG(m_log, "throttle_time=" << m_cpu_throttle_time << ", unthrottle_time=" << m_cpu_unthrottle_time << ", cpu_temp=" << m_cpu_temp);
   LDEBUG(m_log, "throttle=" << throttle << ", unthrottle=" << unthrottle << ", ix: " << ix << " -> " << new_ix << " (" << available[ix] << " -> " << available[new_ix] << ")");

   if (new_ix != ix)
   {
      m_cpuinfo.set_scaling_max_freq(available[new_ix]);

      if (throttle)
      {
         m_cpu_throttle_time = power_set.cpu_throttle_delay;
      }
      else if (unthrottle)
      {
         m_cpu_unthrottle_time = power_set.cpu_unthrottle_delay;
      }
   }
}

void monitor_impl::run_checks()
{
   const monitor::settings::power_mode& power_set = power_settings();

   unsigned delay = std::max(std::max(power_set.fan_hot_delay, power_set.fan_cold_delay),
                             std::max(power_set.cpu_hot_delay, power_set.cpu_cold_delay));

   if (delay/m_set.check_interval < m_history_count)
   {
      double fan_cold = -300.0;
      double fan_hot = 1000.0;
      double cpu_cold = -300.0;
      double cpu_hot = 1000.0;

      for (unsigned i = 0, dt = 0; dt <= delay; ++i, dt += m_set.check_interval)
      {
         double t = m_temp_history[(m_history_count - i) % m_history_size];

         if (dt <= power_set.fan_hot_delay)
         {
            fan_hot = std::min(fan_hot, t);
         }

         if (dt <= power_set.fan_cold_delay)
         {
            fan_cold = std::max(fan_cold, t);
         }

         if (dt <= power_set.cpu_hot_delay)
         {
            cpu_hot = std::min(cpu_hot, t);
         }

         if (dt <= power_set.cpu_cold_delay)
         {
            cpu_cold = std::max(cpu_cold, t);
         }
      }

      LDEBUG(m_log, "fan_hot=" << fan_hot << ", fan_cold=" << fan_cold << ", cpu_hot=" << cpu_hot << ", cpu_cold=" << cpu_cold);

      m_fan_hot = fan_hot;
      m_fan_cold = fan_cold;

      if (m_fan_temp < -280.0)
      {
         m_fan_temp = (m_fan_hot + m_fan_cold)/2.0;
      }

      m_cpu_hot = cpu_hot;
      m_cpu_cold = cpu_cold;

      if (m_cpu_temp < -280.0)
      {
         m_cpu_temp = (m_cpu_hot + m_cpu_cold)/2.0;
      }

      check_fan();
      if (m_cpuinfo.configurable()) {
        check_cpu();
      }
   }
}

void monitor_impl::on_periodic_check(const boost::system::error_code& e)
{
   if (e != boost::asio::error::operation_aborted)
   {
      try
      {
         update_stats();
         run_checks();
      }
      catch (const std::runtime_error& e)
      {
         LWARN(m_log, e.what());
      }

      restart_periodic_check();
   }
}

void monitor_impl::on_idle(const boost::system::error_code& e)
{
   if (e != boost::asio::error::operation_aborted)
   {
      LINFO(m_log, "idle");
      enter_idle(1);
   }
}

int monitor_impl::calc_brightness(const monitor::settings::brightness& set, int cur, int max, bool up, bool slow) const
{
   double norm = std::pow(cur, 1.0/set.exponent);
   double delta = slow ? set.delta_slow : set.delta;

   norm += up ? delta : -delta;

   if (norm < 0.0)
   {
      norm = 0.0;
   }

   int tmp = int(std::pow(norm, set.exponent) + 0.5);

   LDEBUG(m_log, "cur=" << cur << ", norm=" << norm << ", delta=" << delta << ", tmp=" << tmp);

   if (tmp == cur)
   {
      tmp = up ? cur + 1 : cur - 1;
   }

   return std::min(max, std::max(0, tmp));
}

void monitor_impl::set_display_brightness(bool up, bool slow)
{
   int max = m_backlight.max_brightness();
   int cur = m_backlight.actual_brightness();

   unsigned brightness = calc_brightness(m_set.display_backlight, cur, max, up, slow);

   if (brightness != cur)
   {
      m_backlight.set_brightness(brightness);
   }
}

void monitor_impl::set_keyboard_brightness(bool up, bool slow)
{
   const led& backlight = m_applesmc.keyboard_backlight();
   int max = backlight.max_brightness();
   int cur = backlight.brightness();

   unsigned brightness = calc_brightness(m_set.keyboard_backlight, cur, max, up, slow);

   if (brightness != cur)
   {
      backlight.set_brightness(brightness);
   }
}

const monitor::settings::power_mode& monitor_impl::power_settings() const
{
   return m_on_ac ? m_set.on_ac : m_set.on_battery;
}

void monitor_impl::enter_idle(unsigned level)
{
   LDEBUG(m_log, "enter_idle(" << level << ")");

   if (level > m_idle_level)
   {
      unsigned display_current = m_backlight.actual_brightness();
      unsigned keyboard_current = m_applesmc.keyboard_backlight().brightness();
      unsigned display_target = 0;
      unsigned keyboard_target = 0;

      if (m_idle_level == 0)
      {
         m_saved_display_backlight = display_current;
         m_saved_keyboard_backlight = keyboard_current;
      }

      if (level == 1)
      {
         const monitor::settings::power_mode& power_set = power_settings();
         display_target = power_set.display_backlight_idle_level;
         keyboard_target = power_set.keyboard_backlight_idle_level;
      }

      if (display_target < display_current)
      {
         m_backlight.set_brightness(display_target);
      }

      if (keyboard_target < keyboard_current)
      {
         m_applesmc.keyboard_backlight().set_brightness(keyboard_target);
      }

      m_idle_level = level;
   }
}

void monitor_impl::leave_idle()
{
   if (m_idle_level > 0)
   {
      LDEBUG(m_log, "leave_idle() [" << m_idle_level << ", " << m_saved_display_backlight << ", " << m_saved_keyboard_backlight << "]");

      m_backlight.set_brightness(m_saved_display_backlight);
      m_applesmc.keyboard_backlight().set_brightness(m_saved_keyboard_backlight);
      m_idle_level = 0;
   }

   restart_idle();
}

double monitor_impl::current_power() const
{
   size_t delta = m_set.power_interval/m_set.check_interval;

   if (m_history_count <= delta + m_set.power_measurements)
   {
      return 0.0;
   }

   double old = 0.0, now = 0.0;

   for (size_t i = 0; i < m_set.power_measurements; ++i)
   {
      now += m_energy_history[(m_history_count - i) % m_history_size];
      old += m_energy_history[(m_history_count - (delta + i)) % m_history_size];
   }

   return 3600.0*(old - now)/(m_set.power_measurements*m_set.power_interval);
}

void monitor_impl::status(std::ostream& os) const
{
   m_coretemp.dump(os);
   m_applesmc.dump(os);
   m_cpuinfo.dump(os);
   os << "Display Backlight: " << m_backlight.actual_brightness() << "/" << m_backlight.max_brightness() << "\n";
   os << "Running on " << (m_on_ac ? "AC" : "battery");
   if (!m_on_ac)
   {
      os << ", current power consumption: " << m_battery.power_now() << " W (" << current_power() << " W)\n";
   }
   os << "\n";
}

void monitor_impl::handle_event(event_code::type code)
{
   if (code == event_code::LID_CLOSED)
   {
      LINFO(m_log, "lid closed");
      enter_idle(2);
   }
   else if (code == event_code::LID_OPENED)
   {
      LINFO(m_log, "lid opened");
      leave_idle();
   }
   else if (m_idle_level < 2)
   {
      leave_idle();
   }

   switch (code)
   {
      case event_code::DISPLAY_BRIGHTNESS_UP:
         set_display_brightness(true, false);
         break;

      case event_code::DISPLAY_BRIGHTNESS_DOWN:
         set_display_brightness(false, false);
         break;

      case event_code::DISPLAY_BRIGHTNESS_UP_SLOW:
         set_display_brightness(true, true);
         break;

      case event_code::DISPLAY_BRIGHTNESS_DOWN_SLOW:
         set_display_brightness(false, true);
         break;

      case event_code::KEYBOARD_BRIGHTNESS_UP:
         set_keyboard_brightness(true, false);
         break;

      case event_code::KEYBOARD_BRIGHTNESS_DOWN:
         set_keyboard_brightness(false, false);
         break;

      case event_code::KEYBOARD_BRIGHTNESS_UP_SLOW:
         set_keyboard_brightness(true, true);
         break;

      case event_code::KEYBOARD_BRIGHTNESS_DOWN_SLOW:
         set_keyboard_brightness(false, true);
         break;

      default:
         break;
   }
}

}
