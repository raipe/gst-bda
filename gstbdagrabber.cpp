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

#include "gstbdagrabber.h"

GstBdaGrabber::GstBdaGrabber(GstBdaSrc *bda_src)
  : bda_src(bda_src) {
}

GstBdaGrabber::~GstBdaGrabber() {
}

STDMETHODIMP_(ULONG) GstBdaGrabber::AddRef() {
  // Reference counting is not implemented.
  return 1;
}

STDMETHODIMP_(ULONG) GstBdaGrabber::Release() {
  // Reference counting is not implemented.
  return 1;
}

STDMETHODIMP GstBdaGrabber::QueryInterface(REFIID riid, void** object) {
  return E_NOTIMPL;
}

STDMETHODIMP GstBdaGrabber::SampleCB(double time, IMediaSample* sample) {
  BYTE* data = nullptr;
  HRESULT hr = sample->GetPointer(&data);
  if (FAILED(hr)) {
    GST_ERROR_OBJECT(bda_src, "Unable to create TS capture: %ld", hr);
    return S_FALSE;
  }

  bda_src->sample_received(bda_src, data, sample->GetActualDataLength());

  return S_OK;
}

STDMETHODIMP GstBdaGrabber::BufferCB(double time, BYTE* buffer, long bufferLen) {
  return E_FAIL;
}
