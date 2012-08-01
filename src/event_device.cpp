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

#include "event_device.h"
#include "event_handler.h"

namespace aird {

event_device::event_device(boost::asio::io_service& ios, root_logger& root, int fd, const std::string& name)
   : m_dev(ios, fd)
   , m_mod(0)
   , m_log(root, "event_device(" + name + ")")
   , m_stopped(true)
{
}

void event_device::start(boost::shared_ptr<event_handler> handler)
{
   LINFO(m_log, "starting");
   m_stopped = false;
   m_handler = handler;
   read_next_event();
}

void event_device::stop()
{
   if (!m_stopped)
   {
      m_stopped = true;
      m_dev.close();
   }
}

void event_device::read_next_event()
{
   boost::asio::async_read(m_dev, boost::asio::buffer(&m_iev, sizeof(m_iev)),
      boost::bind(&input_device::handle_input_event, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void event_device::handle_input_event(const boost::system::error_code& e, size_t /*bytes_read*/)
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

#ifndef NDEBUG
   if (m_log(log_level::DEBUG))
   {
      debug_input_event();
   }
#endif

   switch (m_iev.type)
   {
      case EV_KEY:
         {
            event_code::type code = event_code::KEYBOARD_ACTIVITY;

            switch (m_iev.code)
            {
               case KEY_LEFTCTRL:
                  setmod(M_CTRL, m_iev.value > 0);
                  break;

               case KEY_LEFTSHIFT:
                  setmod(M_SHIFT, m_iev.value > 0);
                  break;

               case KEY_LEFTALT:
                  setmod(M_ALT, m_iev.value > 0);
                  break;

               case KEY_LEFTMETA:
                  setmod(M_META, m_iev.value > 0);
                  break;

               case KEY_BRIGHTNESSUP:
                  code = m_mod == M_CTRL ? event_code::DISPLAY_BRIGHTNESS_UP_SLOW : event_code::DISPLAY_BRIGHTNESS_UP;
                  break;

               case KEY_BRIGHTNESSDOWN:
                  code = m_mod == M_CTRL ? event_code::DISPLAY_BRIGHTNESS_DOWN_SLOW : event_code::DISPLAY_BRIGHTNESS_DOWN;
                  break;

               case KEY_KBDILLUMUP:
                  code = m_mod == M_CTRL ? event_code::KEYBOARD_BRIGHTNESS_UP_SLOW : event_code::KEYBOARD_BRIGHTNESS_UP;
                  break;

               case KEY_KBDILLUMDOWN:
                  code = m_mod == M_CTRL ? event_code::KEYBOARD_BRIGHTNESS_DOWN_SLOW : event_code::KEYBOARD_BRIGHTNESS_DOWN;
                  break;
            }

            m_handler->handle_event(code);
         }

      case EV_SW:
         switch (m_iev.code)
         {
            case SW_LID:
               m_handler->handle_event(m_iev.value > 0 ? event_code::LID_CLOSED : event_code::LID_OPENED);
               break;
         }

      default:
         break;
   }

   read_next_event();
}

#ifndef NDEBUG

void event_device::debug_input_event() const
{
   std::string typestr(boost::lexical_cast<std::string>(m_iev.type));
   std::string codestr(boost::lexical_cast<std::string>(m_iev.code));
   const char *type = typestr.c_str(), *code = codestr.c_str();

   switch (m_iev.type)
   {
      case EV_SYN: type = "SYN"; break;
      case EV_KEY: type = "KEY"; break;
      case EV_REL: type = "REL"; break;
      case EV_ABS: type = "ABS"; break;
      case EV_MSC: type = "MSC"; break;
      case EV_SW:  type = "SW";  break;
      case EV_LED: type = "LED"; break;
      case EV_SND: type = "SND"; break;
      case EV_REP: type = "REP"; break;
      case EV_FF:  type = "FF";  break;
      case EV_PWR: type = "PWR"; break;
      case EV_FF_STATUS: type = "FF_STATUS"; break;
   }

   if (m_iev.type == EV_KEY)
   {
      switch (m_iev.code)
      {
         case KEY_RESERVED: code = "RESERVED"; break;
         case KEY_ESC: code = "ESC"; break;
         case KEY_1: code = "1"; break;
         case KEY_2: code = "2"; break;
         case KEY_3: code = "3"; break;
         case KEY_4: code = "4"; break;
         case KEY_5: code = "5"; break;
         case KEY_6: code = "6"; break;
         case KEY_7: code = "7"; break;
         case KEY_8: code = "8"; break;
         case KEY_9: code = "9"; break;
         case KEY_0: code = "0"; break;
         case KEY_MINUS: code = "MINUS"; break;
         case KEY_EQUAL: code = "EQUAL"; break;
         case KEY_BACKSPACE: code = "BACKSPACE"; break;
         case KEY_TAB: code = "TAB"; break;
         case KEY_Q: code = "Q"; break;
         case KEY_W: code = "W"; break;
         case KEY_E: code = "E"; break;
         case KEY_R: code = "R"; break;
         case KEY_T: code = "T"; break;
         case KEY_Y: code = "Y"; break;
         case KEY_U: code = "U"; break;
         case KEY_I: code = "I"; break;
         case KEY_O: code = "O"; break;
         case KEY_P: code = "P"; break;
         case KEY_LEFTBRACE: code = "LEFTBRACE"; break;
         case KEY_RIGHTBRACE: code = "RIGHTBRACE"; break;
         case KEY_ENTER: code = "ENTER"; break;
         case KEY_LEFTCTRL: code = "LEFTCTRL"; break;
         case KEY_A: code = "A"; break;
         case KEY_S: code = "S"; break;
         case KEY_D: code = "D"; break;
         case KEY_F: code = "F"; break;
         case KEY_G: code = "G"; break;
         case KEY_H: code = "H"; break;
         case KEY_J: code = "J"; break;
         case KEY_K: code = "K"; break;
         case KEY_L: code = "L"; break;
         case KEY_SEMICOLON: code = "SEMICOLON"; break;
         case KEY_APOSTROPHE: code = "APOSTROPHE"; break;
         case KEY_GRAVE: code = "GRAVE"; break;
         case KEY_LEFTSHIFT: code = "LEFTSHIFT"; break;
         case KEY_BACKSLASH: code = "BACKSLASH"; break;
         case KEY_Z: code = "Z"; break;
         case KEY_X: code = "X"; break;
         case KEY_C: code = "C"; break;
         case KEY_V: code = "V"; break;
         case KEY_B: code = "B"; break;
         case KEY_N: code = "N"; break;
         case KEY_M: code = "M"; break;
         case KEY_COMMA: code = "COMMA"; break;
         case KEY_DOT: code = "DOT"; break;
         case KEY_SLASH: code = "SLASH"; break;
         case KEY_RIGHTSHIFT: code = "RIGHTSHIFT"; break;
         case KEY_KPASTERISK: code = "KPASTERISK"; break;
         case KEY_LEFTALT: code = "LEFTALT"; break;
         case KEY_SPACE: code = "SPACE"; break;
         case KEY_CAPSLOCK: code = "CAPSLOCK"; break;
         case KEY_F1: code = "F1"; break;
         case KEY_F2: code = "F2"; break;
         case KEY_F3: code = "F3"; break;
         case KEY_F4: code = "F4"; break;
         case KEY_F5: code = "F5"; break;
         case KEY_F6: code = "F6"; break;
         case KEY_F7: code = "F7"; break;
         case KEY_F8: code = "F8"; break;
         case KEY_F9: code = "F9"; break;
         case KEY_F10: code = "F10"; break;
         case KEY_NUMLOCK: code = "NUMLOCK"; break;
         case KEY_SCROLLLOCK: code = "SCROLLLOCK"; break;
         case KEY_KP7: code = "KP7"; break;
         case KEY_KP8: code = "KP8"; break;
         case KEY_KP9: code = "KP9"; break;
         case KEY_KPMINUS: code = "KPMINUS"; break;
         case KEY_KP4: code = "KP4"; break;
         case KEY_KP5: code = "KP5"; break;
         case KEY_KP6: code = "KP6"; break;
         case KEY_KPPLUS: code = "KPPLUS"; break;
         case KEY_KP1: code = "KP1"; break;
         case KEY_KP2: code = "KP2"; break;
         case KEY_KP3: code = "KP3"; break;
         case KEY_KP0: code = "KP0"; break;
         case KEY_KPDOT: code = "KPDOT"; break;

         case KEY_ZENKAKUHANKAKU: code = "ZENKAKUHANKAKU"; break;
         case KEY_102ND: code = "102ND"; break;
         case KEY_F11: code = "F11"; break;
         case KEY_F12: code = "F12"; break;
         case KEY_RO: code = "RO"; break;
         case KEY_KATAKANA: code = "KATAKANA"; break;
         case KEY_HIRAGANA: code = "HIRAGANA"; break;
         case KEY_HENKAN: code = "HENKAN"; break;
         case KEY_KATAKANAHIRAGANA: code = "KATAKANAHIRAGANA"; break;
         case KEY_MUHENKAN: code = "MUHENKAN"; break;
         case KEY_KPJPCOMMA: code = "KPJPCOMMA"; break;
         case KEY_KPENTER: code = "KPENTER"; break;
         case KEY_RIGHTCTRL: code = "RIGHTCTRL"; break;
         case KEY_KPSLASH: code = "KPSLASH"; break;
         case KEY_SYSRQ: code = "SYSRQ"; break;
         case KEY_RIGHTALT: code = "RIGHTALT"; break;
         case KEY_LINEFEED: code = "LINEFEED"; break;
         case KEY_HOME: code = "HOME"; break;
         case KEY_UP: code = "UP"; break;
         case KEY_PAGEUP: code = "PAGEUP"; break;
         case KEY_LEFT: code = "LEFT"; break;
         case KEY_RIGHT: code = "RIGHT"; break;
         case KEY_END: code = "END"; break;
         case KEY_DOWN: code = "DOWN"; break;
         case KEY_PAGEDOWN: code = "PAGEDOWN"; break;
         case KEY_INSERT: code = "INSERT"; break;
         case KEY_DELETE: code = "DELETE"; break;
         case KEY_MACRO: code = "MACRO"; break;
         case KEY_MUTE: code = "MUTE"; break;
         case KEY_VOLUMEDOWN: code = "VOLUMEDOWN"; break;
         case KEY_VOLUMEUP: code = "VOLUMEUP"; break;
         case KEY_POWER: code = "POWER"; break;
         case KEY_KPEQUAL: code = "KPEQUAL"; break;
         case KEY_KPPLUSMINUS: code = "KPPLUSMINUS"; break;
         case KEY_PAUSE: code = "PAUSE"; break;
         case KEY_SCALE: code = "SCALE"; break;

         case KEY_KPCOMMA: code = "KPCOMMA"; break;
         case KEY_HANGEUL: code = "HANGEUL"; break;

         case KEY_HANJA: code = "HANJA"; break;
         case KEY_YEN: code = "YEN"; break;
         case KEY_LEFTMETA: code = "LEFTMETA"; break;
         case KEY_RIGHTMETA: code = "RIGHTMETA"; break;
         case KEY_COMPOSE: code = "COMPOSE"; break;

         case KEY_STOP: code = "STOP"; break;
         case KEY_AGAIN: code = "AGAIN"; break;
         case KEY_PROPS: code = "PROPS"; break;
         case KEY_UNDO: code = "UNDO"; break;
         case KEY_FRONT: code = "FRONT"; break;
         case KEY_COPY: code = "COPY"; break;
         case KEY_OPEN: code = "OPEN"; break;
         case KEY_PASTE: code = "PASTE"; break;
         case KEY_FIND: code = "FIND"; break;
         case KEY_CUT: code = "CUT"; break;
         case KEY_HELP: code = "HELP"; break;
         case KEY_MENU: code = "MENU"; break;
         case KEY_CALC: code = "CALC"; break;
         case KEY_SETUP: code = "SETUP"; break;
         case KEY_SLEEP: code = "SLEEP"; break;
         case KEY_WAKEUP: code = "WAKEUP"; break;
         case KEY_FILE: code = "FILE"; break;
         case KEY_SENDFILE: code = "SENDFILE"; break;
         case KEY_DELETEFILE: code = "DELETEFILE"; break;
         case KEY_XFER: code = "XFER"; break;
         case KEY_PROG1: code = "PROG1"; break;
         case KEY_PROG2: code = "PROG2"; break;
         case KEY_WWW: code = "WWW"; break;
         case KEY_MSDOS: code = "MSDOS"; break;
         case KEY_COFFEE: code = "COFFEE"; break;

         case KEY_DIRECTION: code = "DIRECTION"; break;
         case KEY_CYCLEWINDOWS: code = "CYCLEWINDOWS"; break;
         case KEY_MAIL: code = "MAIL"; break;
         case KEY_BOOKMARKS: code = "BOOKMARKS"; break;
         case KEY_COMPUTER: code = "COMPUTER"; break;
         case KEY_BACK: code = "BACK"; break;
         case KEY_FORWARD: code = "FORWARD"; break;
         case KEY_CLOSECD: code = "CLOSECD"; break;
         case KEY_EJECTCD: code = "EJECTCD"; break;
         case KEY_EJECTCLOSECD: code = "EJECTCLOSECD"; break;
         case KEY_NEXTSONG: code = "NEXTSONG"; break;
         case KEY_PLAYPAUSE: code = "PLAYPAUSE"; break;
         case KEY_PREVIOUSSONG: code = "PREVIOUSSONG"; break;
         case KEY_STOPCD: code = "STOPCD"; break;
         case KEY_RECORD: code = "RECORD"; break;
         case KEY_REWIND: code = "REWIND"; break;
         case KEY_PHONE: code = "PHONE"; break;
         case KEY_ISO: code = "ISO"; break;
         case KEY_CONFIG: code = "CONFIG"; break;
         case KEY_HOMEPAGE: code = "HOMEPAGE"; break;
         case KEY_REFRESH: code = "REFRESH"; break;
         case KEY_EXIT: code = "EXIT"; break;
         case KEY_MOVE: code = "MOVE"; break;
         case KEY_EDIT: code = "EDIT"; break;
         case KEY_SCROLLUP: code = "SCROLLUP"; break;
         case KEY_SCROLLDOWN: code = "SCROLLDOWN"; break;
         case KEY_KPLEFTPAREN: code = "KPLEFTPAREN"; break;
         case KEY_KPRIGHTPAREN: code = "KPRIGHTPAREN"; break;
         case KEY_NEW: code = "NEW"; break;
         case KEY_REDO: code = "REDO"; break;

         case KEY_F13: code = "F13"; break;
         case KEY_F14: code = "F14"; break;
         case KEY_F15: code = "F15"; break;
         case KEY_F16: code = "F16"; break;
         case KEY_F17: code = "F17"; break;
         case KEY_F18: code = "F18"; break;
         case KEY_F19: code = "F19"; break;
         case KEY_F20: code = "F20"; break;
         case KEY_F21: code = "F21"; break;
         case KEY_F22: code = "F22"; break;
         case KEY_F23: code = "F23"; break;
         case KEY_F24: code = "F24"; break;

         case KEY_PLAYCD: code = "PLAYCD"; break;
         case KEY_PAUSECD: code = "PAUSECD"; break;
         case KEY_PROG3: code = "PROG3"; break;
         case KEY_PROG4: code = "PROG4"; break;
         case KEY_DASHBOARD: code = "DASHBOARD"; break;
         case KEY_SUSPEND: code = "SUSPEND"; break;
         case KEY_CLOSE: code = "CLOSE"; break;
         case KEY_PLAY: code = "PLAY"; break;
         case KEY_FASTFORWARD: code = "FASTFORWARD"; break;
         case KEY_BASSBOOST: code = "BASSBOOST"; break;
         case KEY_PRINT: code = "PRINT"; break;
         case KEY_HP: code = "HP"; break;
         case KEY_CAMERA: code = "CAMERA"; break;
         case KEY_SOUND: code = "SOUND"; break;
         case KEY_QUESTION: code = "QUESTION"; break;
         case KEY_EMAIL: code = "EMAIL"; break;
         case KEY_CHAT: code = "CHAT"; break;
         case KEY_SEARCH: code = "SEARCH"; break;
         case KEY_CONNECT: code = "CONNECT"; break;
         case KEY_FINANCE: code = "FINANCE"; break;
         case KEY_SPORT: code = "SPORT"; break;
         case KEY_SHOP: code = "SHOP"; break;
         case KEY_ALTERASE: code = "ALTERASE"; break;
         case KEY_CANCEL: code = "CANCEL"; break;
         case KEY_BRIGHTNESSDOWN: code = "BRIGHTNESSDOWN"; break;
         case KEY_BRIGHTNESSUP: code = "BRIGHTNESSUP"; break;
         case KEY_MEDIA: code = "MEDIA"; break;

         case KEY_SWITCHVIDEOMODE: code = "SWITCHVIDEOMODE"; break;
         case KEY_KBDILLUMTOGGLE: code = "KBDILLUMTOGGLE"; break;
         case KEY_KBDILLUMDOWN: code = "KBDILLUMDOWN"; break;
         case KEY_KBDILLUMUP: code = "KBDILLUMUP"; break;

         case KEY_SEND: code = "SEND"; break;
         case KEY_REPLY: code = "REPLY"; break;
         case KEY_FORWARDMAIL: code = "FORWARDMAIL"; break;
         case KEY_SAVE: code = "SAVE"; break;
         case KEY_DOCUMENTS: code = "DOCUMENTS"; break;

         case KEY_BATTERY: code = "BATTERY"; break;

         case KEY_BLUETOOTH: code = "BLUETOOTH"; break;
         case KEY_WLAN: code = "WLAN"; break;
         case KEY_UWB: code = "UWB"; break;

         case KEY_UNKNOWN: code = "UNKNOWN"; break;

         case KEY_VIDEO_NEXT: code = "VIDEO_NEXT"; break;
         case KEY_VIDEO_PREV: code = "VIDEO_PREV"; break;
         case KEY_BRIGHTNESS_CYCLE: code = "BRIGHTNESS_CYCLE"; break;
         case KEY_BRIGHTNESS_ZERO: code = "BRIGHTNESS_ZERO"; break;
         case KEY_DISPLAY_OFF: code = "DISPLAY_OFF"; break;

         case KEY_WIMAX: code = "WIMAX"; break;
         case KEY_RFKILL: code = "RFKILL"; break;

         case KEY_MICMUTE: code = "MICMUTE"; break;

         case BTN_0: code = "0"; break;
         case BTN_1: code = "1"; break;
         case BTN_2: code = "2"; break;
         case BTN_3: code = "3"; break;
         case BTN_4: code = "4"; break;
         case BTN_5: code = "5"; break;
         case BTN_6: code = "6"; break;
         case BTN_7: code = "7"; break;
         case BTN_8: code = "8"; break;
         case BTN_9: code = "9"; break;

         case BTN_LEFT: code = "LEFT"; break;
         case BTN_RIGHT: code = "RIGHT"; break;
         case BTN_MIDDLE: code = "MIDDLE"; break;
         case BTN_SIDE: code = "SIDE"; break;
         case BTN_EXTRA: code = "EXTRA"; break;
         case BTN_FORWARD: code = "FORWARD"; break;
         case BTN_BACK: code = "BACK"; break;
         case BTN_TASK: code = "TASK"; break;

         case BTN_TRIGGER: code = "TRIGGER"; break;
         case BTN_THUMB: code = "THUMB"; break;
         case BTN_THUMB2: code = "THUMB2"; break;
         case BTN_TOP: code = "TOP"; break;
         case BTN_TOP2: code = "TOP2"; break;
         case BTN_PINKIE: code = "PINKIE"; break;
         case BTN_BASE: code = "BASE"; break;
         case BTN_BASE2: code = "BASE2"; break;
         case BTN_BASE3: code = "BASE3"; break;
         case BTN_BASE4: code = "BASE4"; break;
         case BTN_BASE5: code = "BASE5"; break;
         case BTN_BASE6: code = "BASE6"; break;
         case BTN_DEAD: code = "DEAD"; break;

         case BTN_A: code = "A"; break;
         case BTN_B: code = "B"; break;
         case BTN_C: code = "C"; break;
         case BTN_X: code = "X"; break;
         case BTN_Y: code = "Y"; break;
         case BTN_Z: code = "Z"; break;
         case BTN_TL: code = "TL"; break;
         case BTN_TR: code = "TR"; break;
         case BTN_TL2: code = "TL2"; break;
         case BTN_TR2: code = "TR2"; break;
         case BTN_SELECT: code = "SELECT"; break;
         case BTN_START: code = "START"; break;
         case BTN_MODE: code = "MODE"; break;
         case BTN_THUMBL: code = "THUMBL"; break;
         case BTN_THUMBR: code = "THUMBR"; break;

         case BTN_TOOL_PEN: code = "TOOL_PEN"; break;
         case BTN_TOOL_RUBBER: code = "TOOL_RUBBER"; break;
         case BTN_TOOL_BRUSH: code = "TOOL_BRUSH"; break;
         case BTN_TOOL_PENCIL: code = "TOOL_PENCIL"; break;
         case BTN_TOOL_AIRBRUSH: code = "TOOL_AIRBRUSH"; break;
         case BTN_TOOL_FINGER: code = "TOOL_FINGER"; break;
         case BTN_TOOL_MOUSE: code = "TOOL_MOUSE"; break;
         case BTN_TOOL_LENS: code = "TOOL_LENS"; break;
         case BTN_TOOL_QUINTTAP: code = "TOOL_QUINTTAP"; break;
         case BTN_TOUCH: code = "TOUCH"; break;
         case BTN_STYLUS: code = "STYLUS"; break;
         case BTN_STYLUS2: code = "STYLUS2"; break;
         case BTN_TOOL_DOUBLETAP: code = "TOOL_DOUBLETAP"; break;
         case BTN_TOOL_TRIPLETAP: code = "TOOL_TRIPLETAP"; break;
         case BTN_TOOL_QUADTAP: code = "TOOL_QUADTAP"; break;

         case BTN_GEAR_DOWN: code = "GEAR_DOWN"; break;
         case BTN_GEAR_UP: code = "GEAR_UP"; break;

         case KEY_OK: code = "OK"; break;
         case KEY_SELECT: code = "SELECT"; break;
         case KEY_GOTO: code = "GOTO"; break;
         case KEY_CLEAR: code = "CLEAR"; break;
         case KEY_POWER2: code = "POWER2"; break;
         case KEY_OPTION: code = "OPTION"; break;
         case KEY_INFO: code = "INFO"; break;
         case KEY_TIME: code = "TIME"; break;
         case KEY_VENDOR: code = "VENDOR"; break;
         case KEY_ARCHIVE: code = "ARCHIVE"; break;
         case KEY_PROGRAM: code = "PROGRAM"; break;
         case KEY_CHANNEL: code = "CHANNEL"; break;
         case KEY_FAVORITES: code = "FAVORITES"; break;
         case KEY_EPG: code = "EPG"; break;
         case KEY_PVR: code = "PVR"; break;
         case KEY_MHP: code = "MHP"; break;
         case KEY_LANGUAGE: code = "LANGUAGE"; break;
         case KEY_TITLE: code = "TITLE"; break;
         case KEY_SUBTITLE: code = "SUBTITLE"; break;
         case KEY_ANGLE: code = "ANGLE"; break;
         case KEY_ZOOM: code = "ZOOM"; break;
         case KEY_MODE: code = "MODE"; break;
         case KEY_KEYBOARD: code = "KEYBOARD"; break;
         case KEY_SCREEN: code = "SCREEN"; break;
         case KEY_PC: code = "PC"; break;
         case KEY_TV: code = "TV"; break;
         case KEY_TV2: code = "TV2"; break;
         case KEY_VCR: code = "VCR"; break;
         case KEY_VCR2: code = "VCR2"; break;
         case KEY_SAT: code = "SAT"; break;
         case KEY_SAT2: code = "SAT2"; break;
         case KEY_CD: code = "CD"; break;
         case KEY_TAPE: code = "TAPE"; break;
         case KEY_RADIO: code = "RADIO"; break;
         case KEY_TUNER: code = "TUNER"; break;
         case KEY_PLAYER: code = "PLAYER"; break;
         case KEY_TEXT: code = "TEXT"; break;
         case KEY_DVD: code = "DVD"; break;
         case KEY_AUX: code = "AUX"; break;
         case KEY_MP3: code = "MP3"; break;
         case KEY_AUDIO: code = "AUDIO"; break;
         case KEY_VIDEO: code = "VIDEO"; break;
         case KEY_DIRECTORY: code = "DIRECTORY"; break;
         case KEY_LIST: code = "LIST"; break;
         case KEY_MEMO: code = "MEMO"; break;
         case KEY_CALENDAR: code = "CALENDAR"; break;
         case KEY_RED: code = "RED"; break;
         case KEY_GREEN: code = "GREEN"; break;
         case KEY_YELLOW: code = "YELLOW"; break;
         case KEY_BLUE: code = "BLUE"; break;
         case KEY_CHANNELUP: code = "CHANNELUP"; break;
         case KEY_CHANNELDOWN: code = "CHANNELDOWN"; break;
         case KEY_FIRST: code = "FIRST"; break;
         case KEY_LAST: code = "LAST"; break;
         case KEY_AB: code = "AB"; break;
         case KEY_NEXT: code = "NEXT"; break;
         case KEY_RESTART: code = "RESTART"; break;
         case KEY_SLOW: code = "SLOW"; break;
         case KEY_SHUFFLE: code = "SHUFFLE"; break;
         case KEY_BREAK: code = "BREAK"; break;
         case KEY_PREVIOUS: code = "PREVIOUS"; break;
         case KEY_DIGITS: code = "DIGITS"; break;
         case KEY_TEEN: code = "TEEN"; break;
         case KEY_TWEN: code = "TWEN"; break;
         case KEY_VIDEOPHONE: code = "VIDEOPHONE"; break;
         case KEY_GAMES: code = "GAMES"; break;
         case KEY_ZOOMIN: code = "ZOOMIN"; break;
         case KEY_ZOOMOUT: code = "ZOOMOUT"; break;
         case KEY_ZOOMRESET: code = "ZOOMRESET"; break;
         case KEY_WORDPROCESSOR: code = "WORDPROCESSOR"; break;
         case KEY_EDITOR: code = "EDITOR"; break;
         case KEY_SPREADSHEET: code = "SPREADSHEET"; break;
         case KEY_GRAPHICSEDITOR: code = "GRAPHICSEDITOR"; break;
         case KEY_PRESENTATION: code = "PRESENTATION"; break;
         case KEY_DATABASE: code = "DATABASE"; break;
         case KEY_NEWS: code = "NEWS"; break;
         case KEY_VOICEMAIL: code = "VOICEMAIL"; break;
         case KEY_ADDRESSBOOK: code = "ADDRESSBOOK"; break;
         case KEY_MESSENGER: code = "MESSENGER"; break;
         case KEY_DISPLAYTOGGLE: code = "DISPLAYTOGGLE"; break;
         case KEY_SPELLCHECK: code = "SPELLCHECK"; break;
         case KEY_LOGOFF: code = "LOGOFF"; break;

         case KEY_DOLLAR: code = "DOLLAR"; break;
         case KEY_EURO: code = "EURO"; break;

         case KEY_FRAMEBACK: code = "FRAMEBACK"; break;
         case KEY_FRAMEFORWARD: code = "FRAMEFORWARD"; break;
         case KEY_CONTEXT_MENU: code = "CONTEXT_MENU"; break;
         case KEY_MEDIA_REPEAT: code = "MEDIA_REPEAT"; break;
         case KEY_10CHANNELSUP: code = "10CHANNELSUP"; break;
         case KEY_10CHANNELSDOWN: code = "10CHANNELSDOWN"; break;
         case KEY_IMAGES: code = "IMAGES"; break;

         case KEY_DEL_EOL: code = "DEL_EOL"; break;
         case KEY_DEL_EOS: code = "DEL_EOS"; break;
         case KEY_INS_LINE: code = "INS_LINE"; break;
         case KEY_DEL_LINE: code = "DEL_LINE"; break;

         case KEY_FN: code = "FN"; break;
         case KEY_FN_ESC: code = "FN_ESC"; break;
         case KEY_FN_F1: code = "FN_F1"; break;
         case KEY_FN_F2: code = "FN_F2"; break;
         case KEY_FN_F3: code = "FN_F3"; break;
         case KEY_FN_F4: code = "FN_F4"; break;
         case KEY_FN_F5: code = "FN_F5"; break;
         case KEY_FN_F6: code = "FN_F6"; break;
         case KEY_FN_F7: code = "FN_F7"; break;
         case KEY_FN_F8: code = "FN_F8"; break;
         case KEY_FN_F9: code = "FN_F9"; break;
         case KEY_FN_F10: code = "FN_F10"; break;
         case KEY_FN_F11: code = "FN_F11"; break;
         case KEY_FN_F12: code = "FN_F12"; break;
         case KEY_FN_1: code = "FN_1"; break;
         case KEY_FN_2: code = "FN_2"; break;
         case KEY_FN_D: code = "FN_D"; break;
         case KEY_FN_E: code = "FN_E"; break;
         case KEY_FN_F: code = "FN_F"; break;
         case KEY_FN_S: code = "FN_S"; break;
         case KEY_FN_B: code = "FN_B"; break;

         case KEY_BRL_DOT1: code = "BRL_DOT1"; break;
         case KEY_BRL_DOT2: code = "BRL_DOT2"; break;
         case KEY_BRL_DOT3: code = "BRL_DOT3"; break;
         case KEY_BRL_DOT4: code = "BRL_DOT4"; break;
         case KEY_BRL_DOT5: code = "BRL_DOT5"; break;
         case KEY_BRL_DOT6: code = "BRL_DOT6"; break;
         case KEY_BRL_DOT7: code = "BRL_DOT7"; break;
         case KEY_BRL_DOT8: code = "BRL_DOT8"; break;
         case KEY_BRL_DOT9: code = "BRL_DOT9"; break;
         case KEY_BRL_DOT10: code = "BRL_DOT10"; break;

         case KEY_NUMERIC_0: code = "NUMERIC_0"; break;
         case KEY_NUMERIC_1: code = "NUMERIC_1"; break;
         case KEY_NUMERIC_2: code = "NUMERIC_2"; break;
         case KEY_NUMERIC_3: code = "NUMERIC_3"; break;
         case KEY_NUMERIC_4: code = "NUMERIC_4"; break;
         case KEY_NUMERIC_5: code = "NUMERIC_5"; break;
         case KEY_NUMERIC_6: code = "NUMERIC_6"; break;
         case KEY_NUMERIC_7: code = "NUMERIC_7"; break;
         case KEY_NUMERIC_8: code = "NUMERIC_8"; break;
         case KEY_NUMERIC_9: code = "NUMERIC_9"; break;
         case KEY_NUMERIC_STAR: code = "NUMERIC_STAR"; break;
         case KEY_NUMERIC_POUND: code = "NUMERIC_POUND"; break;

         case KEY_CAMERA_FOCUS: code = "CAMERA_FOCUS"; break;
         case KEY_WPS_BUTTON: code = "WPS_BUTTON"; break;

         case KEY_TOUCHPAD_TOGGLE: code = "TOUCHPAD_TOGGLE"; break;
         case KEY_TOUCHPAD_ON: code = "TOUCHPAD_ON"; break;
         case KEY_TOUCHPAD_OFF: code = "TOUCHPAD_OFF"; break;

         case KEY_CAMERA_ZOOMIN: code = "CAMERA_ZOOMIN"; break;
         case KEY_CAMERA_ZOOMOUT: code = "CAMERA_ZOOMOUT"; break;
         case KEY_CAMERA_UP: code = "CAMERA_UP"; break;
         case KEY_CAMERA_DOWN: code = "CAMERA_DOWN"; break;
         case KEY_CAMERA_LEFT: code = "CAMERA_LEFT"; break;
         case KEY_CAMERA_RIGHT: code = "CAMERA_RIGHT"; break;

         default: code = codestr.c_str(); break;
      }
   }

   char buffer[256];
   sprintf(buffer, "%d.%06d type=%s code=%s -> %d", int(m_iev.time.tv_sec), int(m_iev.time.tv_usec), type, code, m_iev.value);
   m_log(log_level::DEBUG, std::string(buffer));
}

#endif

}
