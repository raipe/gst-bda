## GStreamer BDA capture plugin
This is a Windows [BDA](http://en.wikipedia.org/wiki/Broadcast_Driver_Architecture)
capture plugin for GStreamer. It can be used capture MPEG-2 transport stream
from BDA devices.

## Supported input devices

- DVB-C (cable)
- DVB-T (terrestrial)
- DVB-S (satellite), untested
- ATSC, untested

## Build requirements
- [CMake](https://cmake.org/download/)
- [Visual Studio](https://www.visualstudio.com/) or [MinGW-w64](http://mingw-w64.org/doku.php)
- [GStreamer 1.0 SDK](http://gstreamer.freedesktop.org/data/pkg/windows/)

## Sample pipelines

Plays a random program from a DVB-C input:

  > gst-launch-1.0 bdasrc device=0 frequency=154000 symbol-rate=6900 modulation="QAM 128" ! decodebin name=dbin ! queue ! autovideosink dbin. ! queue ! audioconvert ! autoaudiosink

Plays program 49 from a DVB-C input:

  > gst-launch-1.0 bdasrc device=1 frequency=154000 symbol-rate=6900 modulation="QAM 128" ! tsdemux program-number=49 name=demux demux. ! "video/mpeg" ! decodebin ! queue ! autovideosink demux. ! "audio/mpeg" ! queue ! decodebin ! audioconvert ! autoaudiosink
