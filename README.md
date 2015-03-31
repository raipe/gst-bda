## GStreamer BDA capture plugin
This is a Windows BDA capture plugin for GStreamer. It can be used capture
MPEG-2 transport stream from BDA devices.

## Supported input devices

Only DVB-C (cable) input devices have been tested. DVB-T is supported, but
untested.

## Build requirements
- [Visual Studio 2013 or later](https://www.visualstudio.com/)
- [GStreamer 1.0 SDK](http://gstreamer.freedesktop.org/data/pkg/windows/)

## Sample pipelines

Plays a random program from a DVB-C input:

gst-launch-1.0 bdasrc device=0 frequency=154000 symbol-rate=6900 modulation="QAM 128" ! decodebin name=dbin ! queue ! autovideosink dbin. ! queue ! audioconvert ! autoaudiosink
