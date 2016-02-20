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
#include "gstbdatypes.h"

#define GST_CAT_DEFAULT (gstbdasrc_debug)

static const char *
trim_string (char *str)
{
  /* Trim leading whitespace. */
  while (*str && isspace (*str)) {
    str++;
  }

  size_t len = strlen(str);
  if (len == 0) {
    return str;
  }

  /* Trim trailing whitespace. */
  for (size_t i = len - 1; i != 0; i--) {
    if (!isspace (str[i])) {
      break;
    }

    str[i] = 0;
  }

  return str;
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
  HRESULT res = tuner_moniker->BindToStorage (NULL, NULL, IID_IPropertyBag,
      reinterpret_cast < void **>(&property_bag));
  if (FAILED (res)) {
    return name;
  }

  VARIANT var_name;
  VariantInit (&var_name);
  res = property_bag->Read (L"FriendlyName", &var_name, NULL);

  if (FAILED (res)) {
    VariantClear (&var_name);
    return name;
  }

  char mb[512];
  size_t size;
  errno_t err = wcstombs_s (&size, mb, var_name.bstrVal, sizeof (mb));
  if (err == 0) {
    name = mb;
  }

  VariantClear (&var_name);

  return name;
}

GstBdaInputType
gst_bdasrc_get_input_type (GstBdaSrc * bda_src)
{
  IBDA_TopologyPtr bda_topology;
  HRESULT res = bda_src->network_tuner->QueryInterface (&bda_topology);
  if (FAILED (res)) {
    return GST_BDA_UNKNOWN;
  }

  BDANODE_DESCRIPTOR desc[32];
  ULONG n_desc = 0;
  res = bda_topology->GetNodeDescriptors (&n_desc, _countof (desc), desc);
  if (FAILED (res)) {
    return GST_BDA_UNKNOWN;
  }

  for (ULONG i = 0; i < n_desc; i++) {
    const GUID & guid = desc[i].guidFunction;
    if (IsEqualGUID (guid, KSNODE_BDA_QAM_DEMODULATOR)) {
      return GST_BDA_DVB_C;
    } else if (IsEqualGUID (guid, KSNODE_BDA_QPSK_DEMODULATOR)) {
      return GST_BDA_DVB_S;
    } else if (IsEqualGUID (guid, KSNODE_BDA_COFDM_DEMODULATOR)) {
      return GST_BDA_DVB_T;
    } else if (IsEqualGUID (guid, KSNODE_BDA_8VSB_DEMODULATOR)) {
      return GST_BDA_ATSC;
    }
  }

  return GST_BDA_UNKNOWN;
}

BOOL
gst_bdasrc_get_network_type (GstBdaInputType input_type, CLSID & network_type)
{
  switch (input_type) {
    case GST_BDA_ATSC:
      network_type = CLSID_ATSCNetworkProvider;
      return TRUE;
    case GST_BDA_DVB_C:
      network_type = CLSID_DVBCNetworkProvider;
      return TRUE;
    case GST_BDA_DVB_S:
      network_type = CLSID_DVBSNetworkProvider;
      return TRUE;
    case GST_BDA_DVB_T:
      network_type = CLSID_DVBTNetworkProvider;
      return TRUE;
    default:
      return FALSE;
  }
}

BOOL
gst_bdasrc_create_tuning_space (GstBdaSrc * src, ITuningSpacePtr & tuning_space)
{
  CLSID network_type;
  if (!gst_bdasrc_get_network_type (src->input_type, network_type)) {
    return FALSE;
  }

  // First we look for an existing tuning space that matches network type.
  ITuningSpaceContainerPtr tuning_spaces;
  HRESULT res = tuning_spaces.CreateInstance (__uuidof (SystemTuningSpaces));
  if (FAILED (res)) {
    return FALSE;
  }

  IEnumTuningSpacesPtr space_enum;
  res = tuning_spaces->get_EnumTuningSpaces (&space_enum);
  if (FAILED (res)) {
    return FALSE;
  }

  ITuningSpacePtr ts;
  while (space_enum->Next (1, &ts, 0) == S_OK) {
    CLSID type;
    res = ts->get__NetworkType (&type);
    if (FAILED (res) || type == GUID_NULL) {
      continue;
    }

    if (type == network_type) {
      res = ts->QueryInterface (&tuning_space);
      if (SUCCEEDED (res)) {
        return TRUE;
      }
    }
  }

  // No existing tuning space found, create a new one.
  if (src->input_type == GST_BDA_ATSC) {
    HRESULT res = tuning_space.CreateInstance (__uuidof (ATSCTuningSpace));
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Error creating ATSC tuning space: %s (0x%x)",
          bda_err_to_str (res).c_str (), res);
      return FALSE;
    }

    tuning_space->put__NetworkType (ATSC_TERRESTRIAL_TV_NETWORK_TYPE);

    return TRUE;
  } else if (src->input_type == GST_BDA_DVB_C
      || src->input_type == GST_BDA_DVB_T) {
    HRESULT res = tuning_space.CreateInstance (__uuidof (DVBTuningSpace));
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Error creating DVB tuning space: %s (0x%x)",
          bda_err_to_str (res).c_str (), res);
      return FALSE;
    }

    IDVBTuningSpacePtr dvb_tuning_space;
    res = tuning_space->QueryInterface (&dvb_tuning_space);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Unable to get DVB tuning space interface: %s"
          " (0x%x)", bda_err_to_str (res).c_str (), res);
      return FALSE;
    }

    if (src->input_type == GST_BDA_DVB_C) {
      dvb_tuning_space->put__NetworkType (DVB_CABLE_TV_NETWORK_TYPE);
      dvb_tuning_space->put_SystemType (DVB_Cable);
    } else {
      dvb_tuning_space->put__NetworkType (DVB_TERRESTRIAL_TV_NETWORK_TYPE);
      dvb_tuning_space->put_SystemType (DVB_Terrestrial);
    }

    return TRUE;
  } else if (src->input_type == GST_BDA_DVB_S) {
    HRESULT res = tuning_space.CreateInstance (__uuidof (DVBSTuningSpace));
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Error creating DVB-S tuning space: %s (0x%x)",
          bda_err_to_str (res).c_str (), res);
      return FALSE;
    }

    IDVBSTuningSpacePtr dvbs_tuning_space;
    res = tuning_space->QueryInterface (&dvbs_tuning_space);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "Unable to get DVB-S tuning space interface: %s"
          " (0x%x)", bda_err_to_str (res).c_str (), res);
      return FALSE;
    }

    dvbs_tuning_space->put__NetworkType (DVB_SATELLITE_TV_NETWORK_TYPE);
    dvbs_tuning_space->put_SystemType (DVB_Satellite);
    return TRUE;
  }

  return FALSE;
}

BOOL
gst_bdasrc_init_tune_request (GstBdaSrc * src,
    IDVBTuneRequestPtr & tune_request)
{
  IDigitalLocatorPtr locator;
  switch (src->input_type) {
    case GST_BDA_ATSC:
    {
      HRESULT res = locator.CreateInstance (__uuidof (ATSCLocator));
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to create ATSC locator: %s (0x%x)",
            bda_err_to_str (res).c_str (), res);
        return FALSE;
      }
      // TODO: Do we need to call locator->put_PhysicalChannel?
      locator->put_CarrierFrequency (src->frequency);
      locator->put_Modulation (src->modulation);
      locator->put_InnerFECRate (BDA_BCC_RATE_NOT_SET);
      break;
    }
    case GST_BDA_DVB_C:
    {
      HRESULT res = locator.CreateInstance (__uuidof (DVBCLocator));
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to create DVB-C locator: %s (0x%x)",
            bda_err_to_str (res).c_str (), res);
        return FALSE;
      }
      locator->put_CarrierFrequency (src->frequency);
      locator->put_SymbolRate (src->symbol_rate);
      locator->put_Modulation (src->modulation);
      locator->put_InnerFEC (BDA_FEC_METHOD_NOT_SET);
      locator->put_InnerFECRate (BDA_BCC_RATE_NOT_SET);
      locator->put_OuterFEC (BDA_FEC_METHOD_NOT_SET);
      locator->put_OuterFECRate (BDA_BCC_RATE_NOT_SET);
      break;
    }
    case GST_BDA_DVB_T:
    {
      HRESULT res = locator.CreateInstance (__uuidof (DVBTLocator));
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to create DVB-T locator: %s (0x%x)",
            bda_err_to_str (res).c_str (), res);
        return FALSE;
      }

      IDVBTLocatorPtr dvb_t_locator;
      res = locator->QueryInterface (&dvb_t_locator);
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to get DVB-T locator interface: %s"
            " (0x%x)", bda_err_to_str (res).c_str (), res);
        return FALSE;
      }

      dvb_t_locator->put_CarrierFrequency (src->frequency);
      dvb_t_locator->put_Bandwidth (src->bandwidth);
      dvb_t_locator->put_Guard (src->guard_interval);
      dvb_t_locator->put_Mode (src->transmission_mode);
      dvb_t_locator->put_Modulation (src->modulation);
      dvb_t_locator->put_HAlpha (src->hierarchy_information);
      dvb_t_locator->put_InnerFECRate (BDA_BCC_RATE_NOT_SET);
      dvb_t_locator->put_LPInnerFECRate (BDA_BCC_RATE_NOT_SET);
      break;
    }
    case GST_BDA_DVB_S:
    {
      HRESULT res = locator.CreateInstance (__uuidof (DVBSLocator));
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to create DVB-S locator: %s (0x%x)",
            bda_err_to_str (res).c_str (), res);
        return FALSE;
      }

      IDVBSLocatorPtr dvb_s_locator;
      res = locator->QueryInterface (&dvb_s_locator);
      if (FAILED (res)) {
        GST_ERROR_OBJECT (src, "Unable to get DVB-S locator interface: %s"
            " (0x%x)", bda_err_to_str (res).c_str (), res);
        return FALSE;
      }
      dvb_s_locator->put_CarrierFrequency (src->frequency);
      dvb_s_locator->put_SymbolRate (src->symbol_rate);
      dvb_s_locator->put_Modulation (src->modulation);
      dvb_s_locator->put_OrbitalPosition (src->orbital_position);
      dvb_s_locator->put_WestPosition (src->west_position);
      dvb_s_locator->put_SignalPolarisation (src->polarisation);
      dvb_s_locator->put_InnerFECRate (src->inner_fec_rate);
      break;
    }
    default:
      return FALSE;
  }

  HRESULT res = tune_request->put_Locator (locator);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to set locator: %s (0x%x)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  return TRUE;
}

HRESULT
gst_bdasrc_connect_filters (GstBdaSrc * src, IBaseFilter * filter_upstream,
    IBaseFilter * filter_downstream)
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
            res = src->filter_graph->Connect (pin_upstream, pin_downstream);
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
    res = gst_bdasrc_connect_filters (src, upstream_filter, filter);
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
      KSCATEGORY_BDA_RECEIVER_COMPONENT, bda_src->network_tuner,
      &bda_src->receiver);
  if (FAILED (res)) {
    // There is no separate receiver filter, use network tuner instead.
    bda_src->receiver = bda_src->network_tuner;
  }

  AM_MEDIA_TYPE media_type;
  ZeroMemory (&media_type, sizeof (AM_MEDIA_TYPE));
  media_type.majortype = MEDIATYPE_Stream;
  media_type.subtype = MEDIASUBTYPE_MPEG2_TRANSPORT;
  if (FAILED (sample_grabber->SetMediaType (&media_type))) {
    GST_ERROR_OBJECT (bda_src, "Unable to set TS grabber media type");
    return FALSE;
  }
  res = gst_bdasrc_connect_filters (bda_src, bda_src->receiver, ts_capture);
  if (FAILED (res)) {
    media_type.subtype = KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT;
    if (FAILED (sample_grabber->SetMediaType (&media_type))) {
      GST_ERROR_OBJECT (bda_src, "Unable to set TS grabber media type");
      return FALSE;
    }

    res = gst_bdasrc_connect_filters (bda_src, bda_src->receiver, ts_capture);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (bda_src, "Unable to connect TS capture: %s"
          " (0x%x)", bda_err_to_str (res).c_str (), res);
      return FALSE;
    }
  }

  if (FAILED (res = sample_grabber->SetBufferSamples (TRUE)) ||
      FAILED (res = sample_grabber->SetOneShot (FALSE)) ||
      FAILED (res = sample_grabber->SetCallback (bda_src->ts_grabber, 0))) {
    GST_ERROR_OBJECT (bda_src,
        "Unable to configure ISampleGrabber interface: %s (0x%x)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  return TRUE;
}
