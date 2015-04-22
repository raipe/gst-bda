## GStreamer BDA capture plugin
This is a Windows [BDA](http://en.wikipedia.org/wiki/Broadcast_Driver_Architecture)
capture plugin for GStreamer. It can be used capture MPEG-2 transport stream
from BDA devices.

## Supported input devices

Only DVB-C (cable) input devices have been tested. DVB-S (satellite) and DVB-T
(terrestrial) are supported, but untested.

## Build requirements
- [Visual Studio 2013 or later](https://www.visualstudio.com/)
- [qedit.h](http://blogs.msdn.com/b/karinm/archive/2010/01/15/where-is-qedit-h.aspx)
- [GStreamer 1.0 SDK](http://gstreamer.freedesktop.org/data/pkg/windows/)

## Sample pipelines

Plays a random program from a DVB-C input:

  > gst-launch-1.0 bdasrc device=0 frequency=154000 symbol-rate=6900 modulation="QAM 128" ! decodebin name=dbin ! queue ! autovideosink dbin. ! queue ! audioconvert ! autoaudiosink

Plays program 49 from a DVB-C input:

  > gst-launch-1.0 bdasrc device=1 frequency=154000 symbol-rate=6900 modulation="QAM 128" ! tsdemux program-number=49 name=demux demux. ! "video/mpeg" ! decodebin ! queue ! autovideosink demux. ! "audio/mpeg" ! queue ! decodebin ! audioconvert ! autoaudiosink
