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

#include <comdef.h>
// To obtain Qedit.h, download the Microsoft Windows SDK Update for Windows Vista and .NET Framework 3.0:
// http://go.microsoft.com/fwlink/p/?linkid=129787
#include <qedit.h>
#include <strmif.h>

/* Define smart pointers for BDA COM interface types.
   Unlike CComPtr, these don't require ATL. */
_COM_SMARTPTR_TYPEDEF(IBaseFilter, __uuidof (IBaseFilter));
_COM_SMARTPTR_TYPEDEF(IBDA_SignalStatistics, __uuidof (IBDA_SignalStatistics));
_COM_SMARTPTR_TYPEDEF(IBDA_Topology, __uuidof (IBDA_Topology));
_COM_SMARTPTR_TYPEDEF(ICreateDevEnum, __uuidof (ICreateDevEnum));
_COM_SMARTPTR_TYPEDEF(IDVBCLocator, __uuidof (IDVBCLocator));
_COM_SMARTPTR_TYPEDEF(IDVBTuneRequest, __uuidof (IDVBTuneRequest));
_COM_SMARTPTR_TYPEDEF(IDVBTuningSpace, __uuidof (IDVBTuningSpace));
_COM_SMARTPTR_TYPEDEF(IEnumPins, __uuidof (IEnumPins));
_COM_SMARTPTR_TYPEDEF(IPin, __uuidof (IPin));
_COM_SMARTPTR_TYPEDEF(ISampleGrabber, __uuidof (ISampleGrabber));
_COM_SMARTPTR_TYPEDEF(IScanningTuner, __uuidof (IScanningTuner));
_COM_SMARTPTR_TYPEDEF(ITuneRequest, __uuidof (ITuneRequest));

#endif