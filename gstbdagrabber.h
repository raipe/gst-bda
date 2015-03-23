/* GStreamer
 * Copyright (C) 2015 Raimo Järvi <raimo.jarvi@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef _GST_BDAGRABBER_H__
#define _GST_BDAGRABBER_H__

#include <winsock2.h>
// To obtain Qedit.h, download the Microsoft Windows SDK Update for Windows Vista and .NET Framework 3.0:
// http://go.microsoft.com/fwlink/p/?linkid=129787
#include <qedit.h>
#include "gstbdasrc.h"

/** ISampleGrabber filter calls SampleCB function on incoming transport stream
    samples. */
class GstBdaGrabber : public ISampleGrabberCB {
public:
  GstBdaGrabber(GstBdaSrc *bda_src);
  virtual ~GstBdaGrabber();

  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();
  virtual STDMETHODIMP QueryInterface(REFIID riid, void** object);
  virtual STDMETHODIMP SampleCB(double time, IMediaSample* sample);
  virtual STDMETHODIMP BufferCB(double time, BYTE* buffer, long bufferLen);

private:
  GstBdaSrc *bda_src;
};

#endif
