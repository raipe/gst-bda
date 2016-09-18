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

#ifndef __GST_BDATYPES_H__
#define __GST_BDATYPES_H__

#ifdef __GNUC__
  // MinGW comdef.h requires this for sprintf_s
  #include <stdio.h>

  // This is missing from MinGW qedit.h
  extern "C" const CLSID CLSID_SampleGrabber;

  // These are missing from MinGW bdamedia.h
  const GUID KSCATEGORY_BDA_NETWORK_TUNER = { 0x71985f48, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x00, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };
  const GUID KSCATEGORY_BDA_TRANSPORT_INFORMATION = { 0xa2e3074f, 0x6c3d, 0x11d3, 0xb6, 0x53, 0x0, 0xc0, 0x4f, 0x79, 0x49, 0x8e };
  const GUID KSCATEGORY_BDA_RECEIVER_COMPONENT = { 0xfd0a5af4, 0xb41d, 0x11d2, 0x9c, 0x95, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };
  const GUID KSNODE_BDA_QAM_DEMODULATOR = { 0x71985f4d, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };
  const GUID KSNODE_BDA_QPSK_DEMODULATOR = { 0x6390c905, 0x27c1, 0x4d67, 0xbd, 0xb7, 0x77, 0xc5, 0xd, 0x7, 0x93, 0x0 };
  const GUID KSNODE_BDA_COFDM_DEMODULATOR = { 0x2dac6e05, 0xedbe, 0x4b9c, 0xb3, 0x87, 0x1b, 0x6f, 0xad, 0x7d, 0x64, 0x95 };
  const GUID KSNODE_BDA_8VSB_DEMODULATOR = { 0x71985f4f, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x0, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };
#endif

#include <gst/gst.h>
#include <comdef.h>
// To obtain Qedit.h, download the Microsoft Windows SDK Update for Windows Vista and .NET Framework 3.0:
// http://go.microsoft.com/fwlink/p/?linkid=129787
#include <qedit.h>
#include <strmif.h>

/* Supported BDA input device types. */
typedef enum
{
  GST_BDA_UNKNOWN,
  GST_BDA_ATSC,
  GST_BDA_DVB_C,
  GST_BDA_DVB_S,
  GST_BDA_DVB_T
} GstBdaInputType;

/* Define smart pointers for BDA COM interface types.
   Unlike CComPtr, these don't require ATL. */
_COM_SMARTPTR_TYPEDEF (IATSCLocator, __uuidof (IATSCLocator));
_COM_SMARTPTR_TYPEDEF (IATSCTuningSpace, __uuidof (IATSCTuningSpace));
_COM_SMARTPTR_TYPEDEF (IBaseFilter, __uuidof (IBaseFilter));
_COM_SMARTPTR_TYPEDEF (IBDA_SignalStatistics, __uuidof (IBDA_SignalStatistics));
_COM_SMARTPTR_TYPEDEF (IBDA_Topology, __uuidof (IBDA_Topology));
_COM_SMARTPTR_TYPEDEF (ICreateDevEnum, __uuidof (ICreateDevEnum));
_COM_SMARTPTR_TYPEDEF (IDVBCLocator, __uuidof (IDVBCLocator));
_COM_SMARTPTR_TYPEDEF (IDVBSLocator, __uuidof (IDVBSLocator));
_COM_SMARTPTR_TYPEDEF (IDVBTLocator, __uuidof (IDVBTLocator));
_COM_SMARTPTR_TYPEDEF (IDVBTuneRequest, __uuidof (IDVBTuneRequest));
_COM_SMARTPTR_TYPEDEF (IDVBSTuningSpace, __uuidof (IDVBSTuningSpace));
_COM_SMARTPTR_TYPEDEF (IDVBTuningSpace, __uuidof (IDVBTuningSpace));
_COM_SMARTPTR_TYPEDEF (IEnumMoniker, __uuidof (IEnumMoniker));
_COM_SMARTPTR_TYPEDEF (IEnumPins, __uuidof (IEnumPins));
_COM_SMARTPTR_TYPEDEF (IEnumTuningSpaces, __uuidof (IEnumTuningSpaces));
_COM_SMARTPTR_TYPEDEF (IDigitalLocator, __uuidof (IDigitalLocator));
_COM_SMARTPTR_TYPEDEF (IMoniker, __uuidof (IMoniker));
_COM_SMARTPTR_TYPEDEF (IPin, __uuidof (IPin));
_COM_SMARTPTR_TYPEDEF (IPropertyBag, __uuidof (IPropertyBag));
_COM_SMARTPTR_TYPEDEF (ISampleGrabber, __uuidof (ISampleGrabber));
_COM_SMARTPTR_TYPEDEF (IScanningTuner, __uuidof (IScanningTuner));
_COM_SMARTPTR_TYPEDEF (ITuneRequest, __uuidof (ITuneRequest));
_COM_SMARTPTR_TYPEDEF (ITuningSpace, __uuidof (ITuningSpace));
_COM_SMARTPTR_TYPEDEF (ITuningSpaceContainer, __uuidof (ITuningSpaceContainer));
_COM_SMARTPTR_TYPEDEF (IUnknown, __uuidof (IUnknown));

#endif
