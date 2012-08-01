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

#ifndef AIRD_EVENT_HANDLER_H_
#define AIRD_EVENT_HANDLER_H_

namespace aird {

class event_code
{
public:
   enum type
   {
      FAN_SPEED_UP,
      FAN_SPEED_DOWN,
      FAN_SPEED_UP_SLOW,
      FAN_SPEED_DOWN_SLOW,
      DISPLAY_BRIGHTNESS_UP,
      DISPLAY_BRIGHTNESS_DOWN,
      DISPLAY_BRIGHTNESS_UP_SLOW,
      DISPLAY_BRIGHTNESS_DOWN_SLOW,
      KEYBOARD_BRIGHTNESS_UP,
      KEYBOARD_BRIGHTNESS_DOWN,
      KEYBOARD_BRIGHTNESS_UP_SLOW,
      KEYBOARD_BRIGHTNESS_DOWN_SLOW,
      MOUSE_ACTIVITY,
      KEYBOARD_ACTIVITY,
      LID_CLOSED,
      LID_OPENED
   };
};

class event_handler
{
public:
   virtual ~event_handler();
   virtual void handle_event(event_code::type code) = 0;
};

}

#endif
