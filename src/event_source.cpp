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

#include <sstream>
#include <vector>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>

#include <boost/filesystem.hpp>

#include "event_device.h"
#include "event_handler.h"
#include "event_source.h"
#include "log.h"
#include "mouse_device.h"

namespace aird {

class event_source_impl
{
public:
   event_source_impl(boost::asio::io_service& ios, root_logger& root, const event_source::settings& set);
   void start(boost::shared_ptr<event_handler> handler);
   void stop();

private:
   void add_device(int fd);
   bool is_lid_switch(const input_id& id) const;
   bool is_apple_keyboard(const input_id& id) const;
#ifndef NDEBUG
   void debug_evbit(int fd, const unsigned long *evbit) const;
#endif

   typedef std::vector< boost::shared_ptr<input_device> > device_list;

   boost::asio::io_service& m_ios;
   device_list m_dev;
   logger m_log;
};

// TODO
#define DIV_ROUND_UP(n,d)       (((n) + (d) - 1)/(d))
#define BITS_PER_BYTE           8
#define BITS_PER_LONG           (sizeof(long)*BITS_PER_BYTE)
#define BIT_MASK(nr)            (1UL << ((nr)%BITS_PER_LONG))
#define BIT_WORD(nr)            ((nr)/BITS_PER_LONG)
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE*sizeof(long))

#define TEST_BIT(array, bit)    ((array)[BIT_WORD(bit)] & BIT_MASK(bit))


event_handler::~event_handler()
{
}

void event_source::settings::add_options(boost::program_options::options_description& od)
{
   using namespace boost::program_options;

   od.add_options()
      ("event.device_base", value<std::string>(&device_base)->default_value("/dev/input"))
      ;
}

event_source::event_source(boost::asio::io_service& ios, root_logger& root, const settings& set)
   : m_impl(new event_source_impl(ios, root, set))
{
}

void event_source::start(boost::shared_ptr<event_handler> handler)
{
   m_impl->start(handler);
}

void event_source::stop()
{
   m_impl->stop();
}

void event_source_impl::start(boost::shared_ptr<event_handler> handler)
{
   for (device_list::iterator it = m_dev.begin(); it != m_dev.end(); ++it)
   {
      (*it)->start(handler);
   }
}

void event_source_impl::stop()
{
   for (device_list::iterator it = m_dev.begin(); it != m_dev.end(); ++it)
   {
      (*it)->stop();
   }
}

event_source_impl::event_source_impl(boost::asio::io_service& ios, root_logger& root, const event_source::settings& set)
   : m_ios(ios)
   , m_log(root, "event_source")
{
   boost::filesystem::path p(set.device_base);

   for (boost::filesystem::directory_iterator it(p); it != boost::filesystem::directory_iterator(); ++it)
   {
      LDEBUG(m_log, "checking device " << *it);

      const boost::filesystem::path& dev = it->path();

      if (!is_directory(dev))
      {
         int fd = ::open(dev.c_str(), O_RDWR);

         if (fd >= 0)
         {
            if (dev.filename() == "mice")
            {
               m_dev.push_back(boost::shared_ptr<input_device>(new mouse_device(m_ios, m_log.root(), fd, dev.native())));
            }
            else
            {
               try
               {
                  add_device(fd);
               }
               catch (const std::exception& e)
               {
                  LDEBUG(m_log, *it << e.what());
                  ::close(fd);
               }
            }
         }
         else
         {
            LWARN(m_log, "error opening device: " << dev);
         }
      }
   }
}

void event_source_impl::add_device(int fd)
{
   char buffer[256];

   buffer[0] = '\0';
   if (::ioctl(fd, EVIOCGNAME(sizeof(buffer)), buffer) < 0)
   {
      throw std::runtime_error("cannot get name from device");
   }

   std::string name(buffer);
   LDEBUG(m_log, "investigating evdev " << name);

   input_id id;
   if (::ioctl(fd, EVIOCGID, &id) < 0)
   {
      throw std::runtime_error("cannot get ids from device");
   }

   if (m_log(log_level::DEBUG))
   {
      ::sprintf(buffer, "evdev: bus 0x%04x, vid 0x%04x, pid 0x%04x", id.bustype, id.vendor, id.product);
      m_log(log_level::DEBUG, std::string(buffer));
   }

   int version;

   if (::ioctl(fd, EVIOCGVERSION, &version) < 0)
   {
      throw std::runtime_error("device is not an input event device");
   }

   if (version < EV_VERSION)
   {
      throw std::runtime_error("device uses a different version of the event protocol");
   }

   unsigned long evbit[BITS_TO_LONGS(EV_CNT)];
   ::memset(evbit, 0, sizeof(evbit));

   if (::ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0)
   {
      throw std::runtime_error("device has no features");
   }

#ifndef NDEBUG
   if (m_log(log_level::DEBUG))
   {
      debug_evbit(fd, evbit);
   }
#endif

   if ((TEST_BIT(evbit, EV_SW) && is_lid_switch(id)) ||
       (TEST_BIT(evbit, EV_KEY) && TEST_BIT(evbit, EV_LED) && is_apple_keyboard(id)))
   {
      m_dev.push_back(boost::shared_ptr<input_device>(new event_device(m_ios, m_log.root(), fd, name)));
   }
   else
   {
      throw std::runtime_error("unsupported device");
   }
}

bool event_source_impl::is_lid_switch(const input_id& id) const
{
   return id.bustype == BUS_HOST &&
          id.vendor == 0x0000 &&
          id.product == 0x0005;
}

bool event_source_impl::is_apple_keyboard(const input_id& id) const
{
   return id.bustype == BUS_USB &&
          id.vendor == 0x05ac &&
          id.product == 0x0249;
}

#ifndef NDEBUG

void event_source_impl::debug_evbit(int fd, const unsigned long *evbit) const
{
   {
      std::ostringstream oss;
      if (TEST_BIT(evbit, EV_SYN))       oss << "[SYN]";
      if (TEST_BIT(evbit, EV_KEY))       oss << "[KEY]";
      if (TEST_BIT(evbit, EV_REL))       oss << "[REL]";
      if (TEST_BIT(evbit, EV_ABS))       oss << "[ABS]";
      if (TEST_BIT(evbit, EV_MSC))       oss << "[MSC]";
      if (TEST_BIT(evbit, EV_SW))        oss << "[SW]";
      if (TEST_BIT(evbit, EV_LED))       oss << "[LED]";
      if (TEST_BIT(evbit, EV_SND))       oss << "[SND]";
      if (TEST_BIT(evbit, EV_REP))       oss << "[REP]";
      if (TEST_BIT(evbit, EV_FF))        oss << "[FF]";
      if (TEST_BIT(evbit, EV_PWR))       oss << "[PWR]";
      if (TEST_BIT(evbit, EV_FF_STATUS)) oss << "[FF_STATUS]";
      m_log(log_level::DEBUG, oss.str());
   }

   if (TEST_BIT(evbit, EV_KEY))
   {
      unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];

      if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0)
      {
         throw std::runtime_error("device has no keys");
      }

      struct {
         const char * name;
         int bit;
      } defs[] = {
         { "KEY_MUTE", KEY_MUTE },
         { "KEY_VOLUMEDOWN", KEY_VOLUMEDOWN },
         { "KEY_VOLUMEUP", KEY_VOLUMEUP },
         { "KEY_POWER", KEY_POWER },
         { "KEY_KPEQUAL", KEY_KPEQUAL },
         { "KEY_KPPLUSMINUS", KEY_KPPLUSMINUS },
         { "KEY_PAUSE", KEY_PAUSE },
         { "KEY_SCALE", KEY_SCALE },
         { "KEY_KPCOMMA", KEY_KPCOMMA },
         { "KEY_HANGEUL", KEY_HANGEUL },
         { "KEY_HANJA", KEY_HANJA },
         { "KEY_YEN", KEY_YEN },
         { "KEY_LEFTMETA", KEY_LEFTMETA },
         { "KEY_RIGHTMETA", KEY_RIGHTMETA },
         { "KEY_COMPOSE", KEY_COMPOSE },
         { "KEY_STOP", KEY_STOP },
         { "KEY_AGAIN", KEY_AGAIN },
         { "KEY_PROPS", KEY_PROPS },
         { "KEY_UNDO", KEY_UNDO },
         { "KEY_FRONT", KEY_FRONT },
         { "KEY_COPY", KEY_COPY },
         { "KEY_OPEN", KEY_OPEN },
         { "KEY_PASTE", KEY_PASTE },
         { "KEY_FIND", KEY_FIND },
         { "KEY_CUT", KEY_CUT },
         { "KEY_HELP", KEY_HELP },
         { "KEY_MENU", KEY_MENU },
         { "KEY_CALC", KEY_CALC },
         { "KEY_SETUP", KEY_SETUP },
         { "KEY_SLEEP", KEY_SLEEP },
         { "KEY_WAKEUP", KEY_WAKEUP },
         { "KEY_FILE", KEY_FILE },
         { "KEY_SENDFILE", KEY_SENDFILE },
         { "KEY_DELETEFILE", KEY_DELETEFILE },
         { "KEY_XFER", KEY_XFER },
         { "KEY_PROG1", KEY_PROG1 },
         { "KEY_PROG2", KEY_PROG2 },
         { "KEY_WWW", KEY_WWW },
         { "KEY_MSDOS", KEY_MSDOS },
         { "KEY_COFFEE", KEY_COFFEE },
         { "KEY_DIRECTION", KEY_DIRECTION },
         { "KEY_CYCLEWINDOWS", KEY_CYCLEWINDOWS },
         { "KEY_MAIL", KEY_MAIL },
         { "KEY_BOOKMARKS", KEY_BOOKMARKS },
         { "KEY_COMPUTER", KEY_COMPUTER },
         { "KEY_BACK", KEY_BACK },
         { "KEY_FORWARD", KEY_FORWARD },
         { "KEY_CLOSECD", KEY_CLOSECD },
         { "KEY_EJECTCD", KEY_EJECTCD },
         { "KEY_EJECTCLOSECD", KEY_EJECTCLOSECD },
         { "KEY_NEXTSONG", KEY_NEXTSONG },
         { "KEY_PLAYPAUSE", KEY_PLAYPAUSE },
         { "KEY_PREVIOUSSONG", KEY_PREVIOUSSONG },
         { "KEY_STOPCD", KEY_STOPCD },
         { "KEY_RECORD", KEY_RECORD },
         { "KEY_REWIND", KEY_REWIND },
         { "KEY_PHONE", KEY_PHONE },
         { "KEY_ISO", KEY_ISO },
         { "KEY_CONFIG", KEY_CONFIG },
         { "KEY_HOMEPAGE", KEY_HOMEPAGE },
         { "KEY_REFRESH", KEY_REFRESH },
         { "KEY_EXIT", KEY_EXIT },
         { "KEY_MOVE", KEY_MOVE },
         { "KEY_EDIT", KEY_EDIT },
         { "KEY_SCROLLUP", KEY_SCROLLUP },
         { "KEY_SCROLLDOWN", KEY_SCROLLDOWN },
         { "KEY_KPLEFTPAREN", KEY_KPLEFTPAREN },
         { "KEY_KPRIGHTPAREN", KEY_KPRIGHTPAREN },
         { "KEY_NEW", KEY_NEW },
         { "KEY_REDO", KEY_REDO },
         { "KEY_F13", KEY_F13 },
         { "KEY_F14", KEY_F14 },
         { "KEY_F15", KEY_F15 },
         { "KEY_F16", KEY_F16 },
         { "KEY_F17", KEY_F17 },
         { "KEY_F18", KEY_F18 },
         { "KEY_F19", KEY_F19 },
         { "KEY_F20", KEY_F20 },
         { "KEY_F21", KEY_F21 },
         { "KEY_F22", KEY_F22 },
         { "KEY_F23", KEY_F23 },
         { "KEY_F24", KEY_F24 },
         { "KEY_PLAYCD", KEY_PLAYCD },
         { "KEY_PAUSECD", KEY_PAUSECD },
         { "KEY_PROG3", KEY_PROG3 },
         { "KEY_PROG4", KEY_PROG4 },
         { "KEY_DASHBOARD", KEY_DASHBOARD },
         { "KEY_SUSPEND", KEY_SUSPEND },
         { "KEY_CLOSE", KEY_CLOSE },
         { "KEY_PLAY", KEY_PLAY },
         { "KEY_FASTFORWARD", KEY_FASTFORWARD },
         { "KEY_BASSBOOST", KEY_BASSBOOST },
         { "KEY_PRINT", KEY_PRINT },
         { "KEY_HP", KEY_HP },
         { "KEY_CAMERA", KEY_CAMERA },
         { "KEY_SOUND", KEY_SOUND },
         { "KEY_QUESTION", KEY_QUESTION },
         { "KEY_EMAIL", KEY_EMAIL },
         { "KEY_CHAT", KEY_CHAT },
         { "KEY_SEARCH", KEY_SEARCH },
         { "KEY_CONNECT", KEY_CONNECT },
         { "KEY_FINANCE", KEY_FINANCE },
         { "KEY_SPORT", KEY_SPORT },
         { "KEY_SHOP", KEY_SHOP },
         { "KEY_ALTERASE", KEY_ALTERASE },
         { "KEY_CANCEL", KEY_CANCEL },
         { "KEY_BRIGHTNESSDOWN", KEY_BRIGHTNESSDOWN },
         { "KEY_BRIGHTNESSUP", KEY_BRIGHTNESSUP },
         { "KEY_MEDIA", KEY_MEDIA },
         { "KEY_SWITCHVIDEOMODE", KEY_SWITCHVIDEOMODE },
         { "KEY_KBDILLUMTOGGLE", KEY_KBDILLUMTOGGLE },
         { "KEY_KBDILLUMDOWN", KEY_KBDILLUMDOWN },
         { "KEY_KBDILLUMUP", KEY_KBDILLUMUP },
         { "KEY_SEND", KEY_SEND },
         { "KEY_REPLY", KEY_REPLY },
         { "KEY_FORWARDMAIL", KEY_FORWARDMAIL },
         { "KEY_SAVE", KEY_SAVE },
         { "KEY_DOCUMENTS", KEY_DOCUMENTS },
         { "KEY_BATTERY", KEY_BATTERY },
         { "KEY_BLUETOOTH", KEY_BLUETOOTH },
         { "KEY_WLAN", KEY_WLAN },
         { "KEY_UWB", KEY_UWB },
         { "KEY_UNKNOWN", KEY_UNKNOWN },
         { "KEY_VIDEO_NEXT", KEY_VIDEO_NEXT },
         { "KEY_VIDEO_PREV", KEY_VIDEO_PREV },
         { "KEY_BRIGHTNESS_CYCLE", KEY_BRIGHTNESS_CYCLE },
         { "KEY_BRIGHTNESS_ZERO", KEY_BRIGHTNESS_ZERO },
         { "KEY_DISPLAY_OFF", KEY_DISPLAY_OFF },
         { "KEY_WIMAX", KEY_WIMAX },
         { "KEY_RFKILL", KEY_RFKILL },
         { "KEY_MICMUTE", KEY_MICMUTE },
         { "BTN_MISC", BTN_MISC },
         { "BTN_0", BTN_0 },
         { "BTN_1", BTN_1 },
         { "BTN_2", BTN_2 },
         { "BTN_3", BTN_3 },
         { "BTN_4", BTN_4 },
         { "BTN_5", BTN_5 },
         { "BTN_6", BTN_6 },
         { "BTN_7", BTN_7 },
         { "BTN_8", BTN_8 },
         { "BTN_9", BTN_9 },
         { "BTN_MOUSE", BTN_MOUSE },
         { "BTN_LEFT", BTN_LEFT },
         { "BTN_RIGHT", BTN_RIGHT },
         { "BTN_MIDDLE", BTN_MIDDLE },
         { "BTN_SIDE", BTN_SIDE },
         { "BTN_EXTRA", BTN_EXTRA },
         { "BTN_FORWARD", BTN_FORWARD },
         { "BTN_BACK", BTN_BACK },
         { "BTN_TASK", BTN_TASK },
         { "BTN_JOYSTICK", BTN_JOYSTICK },
         { "BTN_TRIGGER", BTN_TRIGGER },
         { "BTN_THUMB", BTN_THUMB },
         { "BTN_THUMB2", BTN_THUMB2 },
         { "BTN_TOP", BTN_TOP },
         { "BTN_TOP2", BTN_TOP2 },
         { "BTN_PINKIE", BTN_PINKIE },
         { "BTN_BASE", BTN_BASE },
         { "BTN_BASE2", BTN_BASE2 },
         { "BTN_BASE3", BTN_BASE3 },
         { "BTN_BASE4", BTN_BASE4 },
         { "BTN_BASE5", BTN_BASE5 },
         { "BTN_BASE6", BTN_BASE6 },
         { "BTN_DEAD", BTN_DEAD },
         { "BTN_GAMEPAD", BTN_GAMEPAD },
         { "BTN_A", BTN_A },
         { "BTN_B", BTN_B },
         { "BTN_C", BTN_C },
         { "BTN_X", BTN_X },
         { "BTN_Y", BTN_Y },
         { "BTN_Z", BTN_Z },
         { "BTN_TL", BTN_TL },
         { "BTN_TR", BTN_TR },
         { "BTN_TL2", BTN_TL2 },
         { "BTN_TR2", BTN_TR2 },
         { "BTN_SELECT", BTN_SELECT },
         { "BTN_START", BTN_START },
         { "BTN_MODE", BTN_MODE },
         { "BTN_THUMBL", BTN_THUMBL },
         { "BTN_THUMBR", BTN_THUMBR },
         { "BTN_DIGI", BTN_DIGI },
         { "BTN_TOOL_PEN", BTN_TOOL_PEN },
         { "BTN_TOOL_RUBBER", BTN_TOOL_RUBBER },
         { "BTN_TOOL_BRUSH", BTN_TOOL_BRUSH },
         { "BTN_TOOL_PENCIL", BTN_TOOL_PENCIL },
         { "BTN_TOOL_AIRBRUSH", BTN_TOOL_AIRBRUSH },
         { "BTN_TOOL_FINGER", BTN_TOOL_FINGER },
         { "BTN_TOOL_MOUSE", BTN_TOOL_MOUSE },
         { "BTN_TOOL_LENS", BTN_TOOL_LENS },
         { "BTN_TOOL_QUINTTAP", BTN_TOOL_QUINTTAP },
         { "BTN_TOUCH", BTN_TOUCH },
         { "BTN_STYLUS", BTN_STYLUS },
         { "BTN_STYLUS2", BTN_STYLUS2 },
         { "BTN_TOOL_DOUBLETAP", BTN_TOOL_DOUBLETAP },
         { "BTN_TOOL_TRIPLETAP", BTN_TOOL_TRIPLETAP },
         { "BTN_TOOL_QUADTAP", BTN_TOOL_QUADTAP },
         { "BTN_WHEEL", BTN_WHEEL },
         { "BTN_GEAR_DOWN", BTN_GEAR_DOWN },
         { "BTN_GEAR_UP", BTN_GEAR_UP },
         { "KEY_OK", KEY_OK },
         { "KEY_SELECT", KEY_SELECT },
         { "KEY_GOTO", KEY_GOTO },
         { "KEY_CLEAR", KEY_CLEAR },
         { "KEY_POWER2", KEY_POWER2 },
         { "KEY_OPTION", KEY_OPTION },
         { "KEY_INFO", KEY_INFO },
         { "KEY_TIME", KEY_TIME },
         { "KEY_VENDOR", KEY_VENDOR },
         { "KEY_ARCHIVE", KEY_ARCHIVE },
         { "KEY_PROGRAM", KEY_PROGRAM },
         { "KEY_CHANNEL", KEY_CHANNEL },
         { "KEY_FAVORITES", KEY_FAVORITES },
         { "KEY_EPG", KEY_EPG },
         { "KEY_PVR", KEY_PVR },
         { "KEY_MHP", KEY_MHP },
         { "KEY_LANGUAGE", KEY_LANGUAGE },
         { "KEY_TITLE", KEY_TITLE },
         { "KEY_SUBTITLE", KEY_SUBTITLE },
         { "KEY_ANGLE", KEY_ANGLE },
         { "KEY_ZOOM", KEY_ZOOM },
         { "KEY_MODE", KEY_MODE },
         { "KEY_KEYBOARD", KEY_KEYBOARD },
         { "KEY_SCREEN", KEY_SCREEN },
         { "KEY_PC", KEY_PC },
         { "KEY_TV", KEY_TV },
         { "KEY_TV2", KEY_TV2 },
         { "KEY_VCR", KEY_VCR },
         { "KEY_VCR2", KEY_VCR2 },
         { "KEY_SAT", KEY_SAT },
         { "KEY_SAT2", KEY_SAT2 },
         { "KEY_CD", KEY_CD },
         { "KEY_TAPE", KEY_TAPE },
         { "KEY_RADIO", KEY_RADIO },
         { "KEY_TUNER", KEY_TUNER },
         { "KEY_PLAYER", KEY_PLAYER },
         { "KEY_TEXT", KEY_TEXT },
         { "KEY_DVD", KEY_DVD },
         { "KEY_AUX", KEY_AUX },
         { "KEY_MP3", KEY_MP3 },
         { "KEY_AUDIO", KEY_AUDIO },
         { "KEY_VIDEO", KEY_VIDEO },
         { "KEY_DIRECTORY", KEY_DIRECTORY },
         { "KEY_LIST", KEY_LIST },
         { "KEY_MEMO", KEY_MEMO },
         { "KEY_CALENDAR", KEY_CALENDAR },
         { "KEY_RED", KEY_RED },
         { "KEY_GREEN", KEY_GREEN },
         { "KEY_YELLOW", KEY_YELLOW },
         { "KEY_BLUE", KEY_BLUE },
         { "KEY_CHANNELUP", KEY_CHANNELUP },
         { "KEY_CHANNELDOWN", KEY_CHANNELDOWN },
         { "KEY_FIRST", KEY_FIRST },
         { "KEY_LAST", KEY_LAST },
         { "KEY_AB", KEY_AB },
         { "KEY_NEXT", KEY_NEXT },
         { "KEY_RESTART", KEY_RESTART },
         { "KEY_SLOW", KEY_SLOW },
         { "KEY_SHUFFLE", KEY_SHUFFLE },
         { "KEY_BREAK", KEY_BREAK },
         { "KEY_PREVIOUS", KEY_PREVIOUS },
         { "KEY_DIGITS", KEY_DIGITS },
         { "KEY_TEEN", KEY_TEEN },
         { "KEY_TWEN", KEY_TWEN },
         { "KEY_VIDEOPHONE", KEY_VIDEOPHONE },
         { "KEY_GAMES", KEY_GAMES },
         { "KEY_ZOOMIN", KEY_ZOOMIN },
         { "KEY_ZOOMOUT", KEY_ZOOMOUT },
         { "KEY_ZOOMRESET", KEY_ZOOMRESET },
         { "KEY_WORDPROCESSOR", KEY_WORDPROCESSOR },
         { "KEY_EDITOR", KEY_EDITOR },
         { "KEY_SPREADSHEET", KEY_SPREADSHEET },
         { "KEY_GRAPHICSEDITOR", KEY_GRAPHICSEDITOR },
         { "KEY_PRESENTATION", KEY_PRESENTATION },
         { "KEY_DATABASE", KEY_DATABASE },
         { "KEY_NEWS", KEY_NEWS },
         { "KEY_VOICEMAIL", KEY_VOICEMAIL },
         { "KEY_ADDRESSBOOK", KEY_ADDRESSBOOK },
         { "KEY_MESSENGER", KEY_MESSENGER },
         { "KEY_DISPLAYTOGGLE", KEY_DISPLAYTOGGLE },
         { "KEY_SPELLCHECK", KEY_SPELLCHECK },
         { "KEY_LOGOFF", KEY_LOGOFF },
         { "KEY_DOLLAR", KEY_DOLLAR },
         { "KEY_EURO", KEY_EURO },
         { "KEY_FRAMEBACK", KEY_FRAMEBACK },
         { "KEY_FRAMEFORWARD", KEY_FRAMEFORWARD },
         { "KEY_CONTEXT_MENU", KEY_CONTEXT_MENU },
         { "KEY_MEDIA_REPEAT", KEY_MEDIA_REPEAT },
         { "KEY_10CHANNELSUP", KEY_10CHANNELSUP },
         { "KEY_10CHANNELSDOWN", KEY_10CHANNELSDOWN },
         { "KEY_IMAGES", KEY_IMAGES },
         { "KEY_DEL_EOL", KEY_DEL_EOL },
         { "KEY_DEL_EOS", KEY_DEL_EOS },
         { "KEY_INS_LINE", KEY_INS_LINE },
         { "KEY_DEL_LINE", KEY_DEL_LINE },
         { "KEY_FN", KEY_FN },
         { "KEY_FN_ESC", KEY_FN_ESC },
         { "KEY_FN_F1", KEY_FN_F1 },
         { "KEY_FN_F2", KEY_FN_F2 },
         { "KEY_FN_F3", KEY_FN_F3 },
         { "KEY_FN_F4", KEY_FN_F4 },
         { "KEY_FN_F5", KEY_FN_F5 },
         { "KEY_FN_F6", KEY_FN_F6 },
         { "KEY_FN_F7", KEY_FN_F7 },
         { "KEY_FN_F8", KEY_FN_F8 },
         { "KEY_FN_F9", KEY_FN_F9 },
         { "KEY_FN_F10", KEY_FN_F10 },
         { "KEY_FN_F11", KEY_FN_F11 },
         { "KEY_FN_F12", KEY_FN_F12 },
         { "KEY_FN_1", KEY_FN_1 },
         { "KEY_FN_2", KEY_FN_2 },
         { "KEY_FN_D", KEY_FN_D },
         { "KEY_FN_E", KEY_FN_E },
         { "KEY_FN_F", KEY_FN_F },
         { "KEY_FN_S", KEY_FN_S },
         { "KEY_FN_B", KEY_FN_B },
         { "KEY_BRL_DOT1", KEY_BRL_DOT1 },
         { "KEY_BRL_DOT2", KEY_BRL_DOT2 },
         { "KEY_BRL_DOT3", KEY_BRL_DOT3 },
         { "KEY_BRL_DOT4", KEY_BRL_DOT4 },
         { "KEY_BRL_DOT5", KEY_BRL_DOT5 },
         { "KEY_BRL_DOT6", KEY_BRL_DOT6 },
         { "KEY_BRL_DOT7", KEY_BRL_DOT7 },
         { "KEY_BRL_DOT8", KEY_BRL_DOT8 },
         { "KEY_BRL_DOT9", KEY_BRL_DOT9 },
         { "KEY_BRL_DOT10", KEY_BRL_DOT10 },
         { "KEY_NUMERIC_0", KEY_NUMERIC_0 },
         { "KEY_NUMERIC_1", KEY_NUMERIC_1 },
         { "KEY_NUMERIC_2", KEY_NUMERIC_2 },
         { "KEY_NUMERIC_3", KEY_NUMERIC_3 },
         { "KEY_NUMERIC_4", KEY_NUMERIC_4 },
         { "KEY_NUMERIC_5", KEY_NUMERIC_5 },
         { "KEY_NUMERIC_6", KEY_NUMERIC_6 },
         { "KEY_NUMERIC_7", KEY_NUMERIC_7 },
         { "KEY_NUMERIC_8", KEY_NUMERIC_8 },
         { "KEY_NUMERIC_9", KEY_NUMERIC_9 },
         { "KEY_NUMERIC_STAR", KEY_NUMERIC_STAR },
         { "KEY_NUMERIC_POUND", KEY_NUMERIC_POUND },
         { "KEY_CAMERA_FOCUS", KEY_CAMERA_FOCUS },
         { "KEY_WPS_BUTTON", KEY_WPS_BUTTON },
         { "KEY_TOUCHPAD_TOGGLE", KEY_TOUCHPAD_TOGGLE },
         { "KEY_TOUCHPAD_ON", KEY_TOUCHPAD_ON },
         { "KEY_TOUCHPAD_OFF", KEY_TOUCHPAD_OFF },
         { "KEY_CAMERA_ZOOMIN", KEY_CAMERA_ZOOMIN },
         { "KEY_CAMERA_ZOOMOUT", KEY_CAMERA_ZOOMOUT },
         { "KEY_CAMERA_UP", KEY_CAMERA_UP },
         { "KEY_CAMERA_DOWN", KEY_CAMERA_DOWN },
         { "KEY_CAMERA_LEFT", KEY_CAMERA_LEFT },
         { "KEY_CAMERA_RIGHT", KEY_CAMERA_RIGHT },
      };

      std::ostringstream oss;
      for (size_t i = 0; i < sizeof(defs)/sizeof(defs[0]); ++i)
      {
         if (TEST_BIT(keybit, defs[i].bit))
         {
            oss << "[" << defs[i].name << "]";
         }
      }
      m_log(log_level::DEBUG, oss.str());
   }

   if (TEST_BIT(evbit, EV_LED))
   {
      unsigned long ledbit[BITS_TO_LONGS(LED_CNT)];

      if (::ioctl(fd, EVIOCGBIT(EV_LED, sizeof(ledbit)), ledbit) < 0)
      {
         throw std::runtime_error("device has no leds");
      }

      struct {
         const char * name;
         int bit;
      } defs[] = {
         { "LED_NUML", LED_NUML },
         { "LED_CAPSL", LED_CAPSL },
         { "LED_SCROLLL", LED_SCROLLL },
         { "LED_COMPOSE", LED_COMPOSE },
         { "LED_KANA", LED_KANA },
         { "LED_SLEEP", LED_SLEEP },
         { "LED_SUSPEND", LED_SUSPEND },
         { "LED_MUTE", LED_MUTE },
         { "LED_MISC", LED_MISC },
         { "LED_MAIL", LED_MAIL },
         { "LED_CHARGING", LED_CHARGING }
      };

      std::ostringstream oss;
      for (size_t i = 0; i < sizeof(defs)/sizeof(defs[0]); ++i)
      {
         if (TEST_BIT(ledbit, defs[i].bit))
         {
            oss << "[" << defs[i].name << "]";
         }
      }
      m_log(log_level::DEBUG, oss.str());
   }
}

#endif

}
