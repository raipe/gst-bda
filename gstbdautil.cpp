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

#include "gstbdautil.h"
#include <windows.h>
#include <errors.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdatypes.h>
#include <bdamedia.h>
#include <bdaiface.h>
#include <tuner.h>
#include <qedit.h>
#include "gstbdagrabber.h"

static const char *
trim_string (char *str)
{
  /* Trim leading whitespace. */
  while (*str && isspace (*str)) {
    str++;
  }

  /* Trim trailing whitespace. */
  for (int i = strlen (str) - 1; i >= 0; i--) {
    if (!isspace (str[i])) {
      break;
    }

    str[i] = 0;
  }

  return str;
}

static std::string
mb_to_wc (const wchar_t * wc)
{
  char mb[512];
  size_t size;
  errno_t err = wcstombs_s (&size, mb, wc, sizeof (mb));

  if (err != 0) {
    return "";
  }

  return mb;
}

std::string
bda_err_to_str (HRESULT hr)
{
  char error_string[MAX_ERROR_TEXT_LEN];
  DWORD res = AMGetErrorTextA (hr, error_string, sizeof (error_string));

  if (res == 0) {
    return "Unknown error";
  }

  return trim_string (error_string);
}

std::string
bda_get_tuner_name (IMoniker * tuner_moniker)
{
  std::string name;

  IPropertyBagPtr property_bag;
  HRESULT res = tuner_moniker->BindToStorage (0, 0, IID_IPropertyBag,
      reinterpret_cast < void **>(&property_bag));
  if (FAILED (res)) {
    return name;
  }

  VARIANT var_name;
  VariantInit (&var_name);
  res = property_bag->Read (L"FriendlyName", &var_name, 0);

  if (FAILED (res)) {
    VariantClear (&var_name);
    return name;
  }

  name = mb_to_wc (var_name.bstrVal);
  VariantClear (&var_name);

  return name;
}

HRESULT
gst_bdasrc_connect_filters (GstBdaSrc * src, IBaseFilter * filter_upstream,
    IBaseFilter * filter_downstream, IGraphBuilder * filter_graph)
{
  IPinPtr pin_upstream;
  PIN_INFO pin_info_upstream;
  PIN_INFO pin_info_downstream;

  IEnumPinsPtr enum_pins_upstream;
  HRESULT res = filter_upstream->EnumPins (&enum_pins_upstream);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Can't enumerate upstream filter's pins");
    return res;
  }

  while (enum_pins_upstream->Next (1, &pin_upstream, 0) == S_OK) {
    res = pin_upstream->QueryPinInfo (&pin_info_upstream);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Can't get upstream filter pin info");
      return res;
    }

    IPinPtr pin_down;
    pin_upstream->ConnectedTo (&pin_down);

    /* Bail if pins are connected, otherwise check direction and connect. */
    if ((pin_info_upstream.dir == PINDIR_OUTPUT) && (pin_down == NULL)) {
      /* Grab downstream filter's enumerator. */
      IEnumPinsPtr enumPinsDownstream;
      res = filter_downstream->EnumPins (&enumPinsDownstream);
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Can't enumerate downstream filter pins");
        return res;
      }

      IPinPtr pin_downstream;
      while (enumPinsDownstream->Next (1, &pin_downstream, 0) == S_OK) {
        if (SUCCEEDED (pin_downstream->QueryPinInfo (&pin_info_downstream))) {
          IPinPtr pinUp;

          /* Determine if the pin is already connected. VFW_E_NOT_CONNECTED is
             expected if the pin isn't connected yet. */
          res = pin_downstream->ConnectedTo (&pinUp);
          if (FAILED (res) && res != VFW_E_NOT_CONNECTED)
            continue;

          if ((pin_info_downstream.dir == PINDIR_INPUT) && (pinUp == NULL)) {
            res = filter_graph->Connect (pin_upstream, pin_downstream);
            if (SUCCEEDED (res)) {
              pin_info_downstream.pFilter->Release ();
              pin_info_upstream.pFilter->Release ();

              return S_OK;
            }
          }
        }

        pin_info_downstream.pFilter->Release ();
      }
    }

    pin_info_upstream.pFilter->Release ();
  }

  return E_FAIL;
}

HRESULT
gst_bdasrc_load_filter (GstBdaSrc * src, ICreateDevEnum * sys_dev_enum,
    REFCLSID clsid, IBaseFilter * upstream_filter,
    IBaseFilter ** downstream_filter)
{
  IEnumMonikerPtr enum_moniker;
  HRESULT res = sys_dev_enum->CreateClassEnumerator (clsid, &enum_moniker, 0);
  if (res == S_FALSE) {
    /* The device category does not exist or is empty. */
    return E_UNEXPECTED;
  } else if (FAILED (res)) {
    return res;
  }

  IMonikerPtr moniker;
  int monikerIndex = -1;
  while (enum_moniker->Next (1, &moniker, 0) == S_OK) {
    monikerIndex++;

    IPropertyBagPtr bag;
    res = moniker->BindToStorage (NULL, NULL, IID_IPropertyBag, (void **) &bag);
    if (FAILED (res)) {
      return res;
    }

    wchar_t *wname;
    VARIANT nameBstr;
    VariantInit (&nameBstr);
    res = bag->Read (L"FriendlyName", &nameBstr, NULL);
    if (FAILED (res)) {
      VariantClear (&nameBstr);
      continue;
    } else {
      wname = nameBstr.bstrVal;

      /* FIXME: This is stupid. */
      if (wcscmp (wname, L"BDA MPE Filter") == 0
          || wcscmp (wname, L"BDA Slip De-Framer") == 0) {
        VariantClear (&nameBstr);
        continue;
      }
    }

    IBaseFilterPtr filter;
    res =
        moniker->BindToObject (NULL, NULL, IID_IBaseFilter, (void **) &filter);
    if (FAILED (res)) {
      VariantClear (&nameBstr);
      continue;
    }

    res = src->filter_graph->AddFilter (filter, wname);
    VariantClear (&nameBstr);
    if (FAILED (res)) {
      return res;
    }
    /* Test connection to upstream filter. */
    res =
        gst_bdasrc_connect_filters (src, upstream_filter, filter,
        src->filter_graph);
    if (SUCCEEDED (res)) {
      /* It's the filter we want. */
      filter->QueryInterface (downstream_filter);
      return S_OK;
    } else {
      /* It wasn't the the filter we want, unload and try the next one. */
      res = src->filter_graph->RemoveFilter (filter);
      if (FAILED (res)) {
        return res;
      }
    }
  }

  return E_FAIL;
}

BOOL
gst_bdasrc_create_ts_capture (GstBdaSrc * bda_src,
    ICreateDevEnum * sys_dev_enum, IBaseFilterPtr & ts_capture)
{
  HRESULT res = ts_capture.CreateInstance (CLSID_SampleGrabber);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (bda_src, "Unable to create TS capture");
    return FALSE;
  }

  res = bda_src->filter_graph->AddFilter (ts_capture, L"TS capture");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (bda_src, "Unable to add TS capture filter to graph");
    return FALSE;
  }

  ISampleGrabberPtr sample_grabber;
  res = ts_capture->QueryInterface (&sample_grabber);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (bda_src, "Unable to query ISampleGrabber interface: %s"
        " (0x%x)", bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  res =
      gst_bdasrc_load_filter (bda_src, sys_dev_enum,
      KSCATEGORY_BDA_RECEIVER_COMPONENT, bda_src->tuner, &bda_src->capture);
  if (FAILED (res)) {
    bda_src->capture = bda_src->tuner;
  }

  AM_MEDIA_TYPE media_type;
  ZeroMemory (&media_type, sizeof (AM_MEDIA_TYPE));
  media_type.majortype = MEDIATYPE_Stream;
  media_type.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
  if (FAILED (sample_grabber->SetMediaType (&media_type))) {
    GST_ERROR_OBJECT (bda_src, "Unable to set TS grabber media type");
    return FALSE;
  }
  res =
      gst_bdasrc_connect_filters (bda_src, bda_src->capture, ts_capture,
      bda_src->filter_graph);
  if (FAILED (res)) {
    media_type.subtype = KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
    if (FAILED (sample_grabber->SetMediaType (&media_type))) {
      GST_ERROR_OBJECT (bda_src, "Unable to set TS grabber media type");
      return FALSE;
    }

    res =
        gst_bdasrc_connect_filters (bda_src, bda_src->capture, ts_capture,
        bda_src->filter_graph);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (bda_src, "Unable to connect TS capture: %s"
          " (0x%x)", bda_err_to_str (res).c_str (), res);
      return FALSE;
    }
  }

  if (FAILED (sample_grabber->SetBufferSamples (TRUE)) ||
      FAILED (sample_grabber->SetOneShot (FALSE)) ||
      FAILED (sample_grabber->SetCallback (bda_src->grabber, 0))) {
    GST_ERROR_OBJECT (bda_src,
        "Unable to configure ISampleGrabber interface: %s" " (0x%x)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  return TRUE;
}
