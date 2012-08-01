aird
====

A highly configurable backlight and fan control daemon for MacBook Air.

**WARNING** This code is the result of one weekend of intense hacking.
In its current state, it *will* contain bugs, very likely loads of them.

History
-------

So, I got this shiny new MacBook Air (5,1) about a week ago and after
installing Linux on it, I was quite dissatisfied (as with most of my
previous laptops) with the automatic fan control.  Also, I found that
pommed didn't support the new hardware. After I patched in support for
the new Air, I didn't like the way the buttons controlled the backlight.
Too coarse at the dark end and too fine at the bright end. Last but not
least, I was missing the dim-display-backlight-when-idle feature that
I had commonly seen on MacOS X before. So I decided to just write my
own tool that did exactly what I wanted.

Features
--------

Currently, *aird* (the Air Daemon) has the following features:

* Supports MacBook Air 5,1 (and likely also the 5,2). Support for other
  models is most probably easy to add, but not currently my priority.

* Dimming of display and keyboard backlight after a configurable idle
  timeout (i.e. no keyboard or mouse input). Dimming levels are also
  configurable.

* Brightness of display and keyboard backlight can be adjusted using
  the "standard" keys but using an exponential instead of a linear
  curve, which actually *feels* very linear when using it. The curve
  parameters (exponent and step size) are configurable. If the normal
  spacing is too coarse you can switch to a finer step size using the
  control key.

* An integrated server accepts TCP connections and outputs status
  information. This can be used with a tool like conky simply by
  adding something like this to the config:

    ${exec nc localhost 21577 2>/dev/null}

* When closing the lid, display and keyboard backlight will be dimmed
  to zero (what's the point in leaving the light on when you can't see
  anything?) and when opening the lid, the previous levels will be
  restored.

* Fan speed can be configured depending on processor temperature. In
  order to avoid the fan spinning up and down all the time, the CPU
  temperature must pass a certain threshold for a certain amount of
  time. Thus, short peaks in CPU Load (and temperature) won't cause
  the fan to spin up immediately.

* In addition, the CPU can be throttled once the temperature goes past
  a certain threshold.

* When the remaining battery energy goes below a certain threshold,
  the maximum CPU speed can be limited. Also, the CPU speed can be
  limited in general when running on battery.

* Most parameters can be configured independently for both AC and
  battery operation.

* Fine-grained logging using either syslog when running as a daemon
  or the console when running in debug mode.

* When exiting/killing *aird*, it will try to restore "safe" brightness
  levels.

Installation
------------

When you're running Gentoo, you can use the ebuild supplied in:

    contrib/gentoo/aird.ebuild

Just copy it to your local portage overlay:

    # mkdir -p /usr/local/portage/app-laptop/aird
    # cp contrib/gentoo/aird.ebuild /usr/local/portage/app-laptop/aird-0.1.0.ebuild
    # cd /usr/local/portage/app-laptop/aird
    # ebuild aird-0.1.0.ebuild digest

If you've never used a local portage overlay before, you'll have to
configure it in /etc/make.conf by adding the following line:

    PORTDIR_OVERLAY="/usr/local/portage"

Now you can emerge *aird*:

    # emerge aird

Otherwise, you can build and install the code like this:

    # wget https://github.com/downloads/mhx/aird/aird-0.1.0.tar.bz2
    # tar xvjf aird-0.1.0.tar.bz2
    # mkdir aird-build
    # cd aird-build
    # cmake ../aird-0.1.0
    # make -j
    # sudo make install

*aird* depends on cmake and the boost libraries.

Usage
-----

On Gentoo you can start and stop *aird* like any other system service:

    # /etc/init.d/aird start
    # /etc/init.d/aird stop

Otherwise, you can either write some simple init script that works on
your specific distribution or you can just run *aird* manually:

    # sudo aird

Configuration
-------------

Have a look at:

    /etc/aird.cfg

There's loads of stuff in there. It currently lacks documentation,
though.

Feedback
--------

Feedback is more than welcome. If you've found (or even fixed!) a bug,
please let me (github@mhxnet.de) know.

Enjoy,
Marcus
