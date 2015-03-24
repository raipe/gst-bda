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

/**
 * SECTION:element-bdasrc
 *
 * bdasrc can be used to capture MPEG-2 transport stream from Windows BDA devices: DVB-C, DVB-S or DVB-T.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstbdasrc.h"
#include <gst/gst.h>
#include <string.h>
#include <comdef.h>
#include <control.h>
#include <dshow.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdatypes.h>
#include <bdamedia.h>
#include <bdaiface.h>
#include <tuner.h>
#include "gstbdagrabber.h"

GST_DEBUG_CATEGORY_STATIC (gstbdasrc_debug);
#define GST_CAT_DEFAULT (gstbdasrc_debug)

/* Arguments */
enum
{
  ARG_0,
  ARG_BDASRC_FREQUENCY,
  ARG_BDASRC_SYM_RATE,
  ARG_BDASRC_BANDWIDTH,
  ARG_BDASRC_CODE_RATE_HP,
  ARG_BDASRC_CODE_RATE_LP,
  ARG_BDASRC_GUARD_INTERVAL,
  ARG_BDASRC_MODULATION,
  ARG_BDASRC_TRANSMISSION_MODE,
  ARG_BDASRC_HIERARCHY_INF
};

#define DEFAULT_ADAPTER 0
#define DEFAULT_FREQUENCY 0
#define DEFAULT_SYMBOL_RATE 0
#define DEFAULT_BANDWIDTH 8
#define DEFAULT_CODE_RATE_HP BDA_BCC_RATE_NOT_SET
#define DEFAULT_CODE_RATE_LP BDA_BCC_RATE_1_2
#define DEFAULT_GUARD BDA_GUARD_1_16
#define DEFAULT_MODULATION BDA_MOD_16QAM
#define DEFAULT_TRANSMISSION_MODE BDA_XMIT_MODE_8K
#define DEFAULT_HIERARCHY BDA_HALPHA_NOT_SET

/* Define smart pointers for BDA COM interface types.
   Unlike CComPtr, these don't require ATL.  */
_COM_SMARTPTR_TYPEDEF (IBaseFilter, __uuidof (IBaseFilter));
_COM_SMARTPTR_TYPEDEF (ICreateDevEnum, __uuidof (ICreateDevEnum));
_COM_SMARTPTR_TYPEDEF (IDVBCLocator, __uuidof (IDVBCLocator));
_COM_SMARTPTR_TYPEDEF (IDVBTuneRequest, __uuidof (IDVBTuneRequest));
_COM_SMARTPTR_TYPEDEF (IDVBTuningSpace, __uuidof (IDVBTuningSpace));
_COM_SMARTPTR_TYPEDEF (IEnumPins, __uuidof (IEnumPins));
_COM_SMARTPTR_TYPEDEF (IPin, __uuidof (IPin));
_COM_SMARTPTR_TYPEDEF (IScanningTuner, __uuidof (IScanningTuner));
_COM_SMARTPTR_TYPEDEF (ITuneRequest, __uuidof (ITuneRequest));

static void gst_bdasrc_output_frontend_stats (GstBdaSrc * src);

#define GST_TYPE_BDASRC_CODE_RATE (gst_bdasrc_code_rate_get_type ())
static GType
gst_bdasrc_code_rate_get_type (void)
{
  static GType bdasrc_code_rate_type = 0;
  static GEnumValue code_rate_types[] = {
    {BDA_BCC_RATE_NOT_SET, "NONE", "NONE"},
    {BDA_BCC_RATE_1_2, "1/2", "1/2"},
    {BDA_BCC_RATE_2_3, "2/3", "2/3"},
    {BDA_BCC_RATE_3_4, "3/4", "3/4"},
    {BDA_BCC_RATE_4_5, "4/5", "4/5"},
    {BDA_BCC_RATE_5_6, "5/6", "5/6"},
    {BDA_BCC_RATE_6_7, "6/7", "6/7"},
    {BDA_BCC_RATE_7_8, "7/8", "7/8"},
    {BDA_BCC_RATE_8_9, "8/9", "8/9"},
    {0, NULL, NULL},
  };

  if (!bdasrc_code_rate_type) {
    bdasrc_code_rate_type =
        g_enum_register_static ("GstBdaSrcCodeRate", code_rate_types);
  }
  return bdasrc_code_rate_type;
}

#define GST_TYPE_BDASRC_MODULATION (gst_bdasrc_modulation_get_type ())
static GType
gst_bdasrc_modulation_get_type (void)
{
  static GType bdasrc_modulation_type = 0;
  static GEnumValue modulation_types[] = {
    {BDA_MOD_QPSK, "QPSK", "QPSK"},
    {BDA_MOD_16QAM, "QAM 16", "QAM 16"},
    {BDA_MOD_32QAM, "QAM 32", "QAM 32"},
    {BDA_MOD_64QAM, "QAM 64", "QAM 64"},
    {BDA_MOD_128QAM, "QAM 128", "QAM 128"},
    {BDA_MOD_256QAM, "QAM 256", "QAM 256"},
    {BDA_MOD_8VSB, "8VSB", "8VSB"},
    {BDA_MOD_16VSB, "16VSB", "16VSB"},
    {BDA_MOD_NOT_SET, "NONE", "NONE"},
    {0, NULL, NULL},
  };

  if (!bdasrc_modulation_type) {
    bdasrc_modulation_type =
        g_enum_register_static ("GstBdaSrcModulation", modulation_types);
  }
  return bdasrc_modulation_type;
}

#define GST_TYPE_BDASRC_TRANSMISSION_MODE (gst_bdasrc_transmission_mode_get_type ())
static GType
gst_bdasrc_transmission_mode_get_type (void)
{
  static GType bdasrc_transmission_mode_type = 0;
  static GEnumValue transmission_mode_types[] = {
    {BDA_XMIT_MODE_2K, "2k", "2k"},
    {BDA_XMIT_MODE_8K, "8k", "8k"},
    {BDA_XMIT_MODE_NOT_SET, "NONE", "NONE"},
    {0, NULL, NULL},
  };

  if (!bdasrc_transmission_mode_type) {
    bdasrc_transmission_mode_type =
        g_enum_register_static ("GstBdaSrcTransmissionMode",
        transmission_mode_types);
  }
  return bdasrc_transmission_mode_type;
}

#define GST_TYPE_BDASRC_GUARD_INTERVAL (gst_bdasrc_guard_get_type ())
static GType
gst_bdasrc_guard_get_type (void)
{
  static GType bdasrc_guard_type = 0;
  static GEnumValue guard_types[] = {
    {BDA_GUARD_1_32, "32", "32"},
    {BDA_GUARD_1_16, "16", "16"},
    {BDA_GUARD_1_8, "8", "8"},
    {BDA_GUARD_1_4, "4", "4"},
    {BDA_GUARD_NOT_SET, "NONE", "NONE"},
    {0, NULL, NULL},
  };

  if (!bdasrc_guard_type) {
    bdasrc_guard_type = g_enum_register_static ("GstBdaSrcGuard", guard_types);
  }
  return bdasrc_guard_type;
}

#define GST_TYPE_BDASRC_HIERARCHY (gst_bdasrc_hierarchy_get_type ())
static GType
gst_bdasrc_hierarchy_get_type (void)
{
  static GType bdasrc_hierarchy_type = 0;
  static GEnumValue hierarchy_types[] = {
    {BDA_HALPHA_NOT_SET, "NONE", "NONE"},
    {BDA_HALPHA_1, "1", "1"},
    {BDA_HALPHA_2, "2", "2"},
    {BDA_HALPHA_4, "4", "4"},
    {0, NULL, NULL},
  };

  if (!bdasrc_hierarchy_type) {
    bdasrc_hierarchy_type =
        g_enum_register_static ("GstBdaSrcHierarchy", hierarchy_types);
  }
  return bdasrc_hierarchy_type;
}

static void gst_bdasrc_finalize (GObject * object);
static void gst_bdasrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bdasrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_bdasrc_sample_received (GstBdaSrc * self, gpointer data,
    gsize size);
static GstFlowReturn gst_bdasrc_create (GstPushSrc * element,
    GstBuffer ** buffer);

static gboolean gst_bdasrc_start (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_stop (GstBaseSrc * bsrc);
static GstStateChangeReturn gst_bdasrc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_bdasrc_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_get_size (GstBaseSrc * src, guint64 * size);

static gboolean gst_bdasrc_tune (GstBdaSrc * object);
static HRESULT gst_bdasrc_connect_filters (GstBdaSrc * src,
    IBaseFilter * filter_upstream, IBaseFilter * filter_downstream,
    IGraphBuilder * filter_graph);
static HRESULT gst_bdasrc_load_filter (GstBdaSrc * src,
    ICreateDevEnum * sys_dev_enum, REFCLSID clsid,
    IBaseFilter * upstream_filter, IBaseFilter ** downstream_filter);

static GstStaticPadTemplate ts_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpegts, "
        "mpegversion = (int) 2," "systemstream = (boolean) TRUE"));

/* GObject Related */

#define gst_bdasrc_parent_class parent_class
G_DEFINE_TYPE (GstBdaSrc, gst_bdasrc, GST_TYPE_PUSH_SRC);

/* Initialize the plugin's class. */
static void
gst_bdasrc_class_init (GstBdaSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_bdasrc_set_property;
  gobject_class->get_property = gst_bdasrc_get_property;
  gobject_class->finalize = gst_bdasrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&ts_src_factory));

  gst_element_class_set_details_simple (gstelement_class, "BDA Source",
      "Source/Video",
      "Microsoft Broadcast Driver Architecture Source",
      "Raimo Järvi <raimo.jarvi@gmail.com>");

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_bdasrc_change_state);
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_bdasrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_bdasrc_stop);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_bdasrc_is_seekable);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_bdasrc_get_size);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_bdasrc_create);

  g_object_class_install_property (gobject_class, ARG_BDASRC_FREQUENCY,
      g_param_spec_uint ("frequency", "frequency", "Frequency in kHz",
          0, G_MAXUINT, DEFAULT_FREQUENCY, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_SYM_RATE,
      g_param_spec_uint ("symbol-rate", "symbol rate",
          "Symbol Rate in kHz (DVB-S, DVB-C)",
          0, G_MAXUINT, DEFAULT_SYMBOL_RATE, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_BANDWIDTH,
      g_param_spec_int ("bandwidth", "bandwidth",
          "Bandwidth (DVB-T)", 5, 8, DEFAULT_BANDWIDTH,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_CODE_RATE_HP,
      g_param_spec_enum ("code-rate-hp", "code-rate-hp",
          "High Priority Code Rate (DVB-T, DVB-S and DVB-C)",
          GST_TYPE_BDASRC_CODE_RATE, DEFAULT_CODE_RATE_HP,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_CODE_RATE_LP,
      g_param_spec_enum ("code-rate-lp", "code-rate-lp",
          "Low Priority Code Rate (DVB-T)",
          GST_TYPE_BDASRC_CODE_RATE, DEFAULT_CODE_RATE_LP,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_GUARD_INTERVAL,
      g_param_spec_enum ("guard-interval", "guard-interval",
          "Guard Interval (DVB-T)",
          GST_TYPE_BDASRC_GUARD_INTERVAL, DEFAULT_GUARD,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_MODULATION,
      g_param_spec_enum ("modulation", "modulation",
          "Modulation (DVB-T and DVB-C)",
          GST_TYPE_BDASRC_MODULATION, DEFAULT_MODULATION,
          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
      ARG_BDASRC_TRANSMISSION_MODE,
      g_param_spec_enum ("transmission-mode", "transmission-mode",
          "Transmission Mode (DVB-T)", GST_TYPE_BDASRC_TRANSMISSION_MODE,
          DEFAULT_TRANSMISSION_MODE, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_BDASRC_HIERARCHY_INF,
      g_param_spec_enum ("hierarchy", "hierarchy",
          "Hierarchy Information (DVB-T)",
          GST_TYPE_BDASRC_HIERARCHY, DEFAULT_HIERARCHY,
          (GParamFlags) G_PARAM_READWRITE));
}

static void
gst_bdasrc_init (GstBdaSrc * self)
{
  GST_INFO_OBJECT (self, "gst_bdasrc_init");

  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);

  self->adapter_number = DEFAULT_ADAPTER;
  self->frequency = 0;
  self->symbol_rate = DEFAULT_SYMBOL_RATE;
  self->bandwidth = DEFAULT_BANDWIDTH;
  self->code_rate_hp = DEFAULT_CODE_RATE_HP;
  self->code_rate_lp = DEFAULT_CODE_RATE_LP;
  self->guard_interval = DEFAULT_GUARD;
  self->modulation = DEFAULT_MODULATION;
  self->transmission_mode = DEFAULT_TRANSMISSION_MODE;
  self->hierarchy_information = DEFAULT_HIERARCHY;

  self->tuner = NULL;
  self->filter_graph = NULL;
  self->media_control = NULL;

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  g_queue_init (&self->ts_samples);

  self->sample_received = gst_bdasrc_sample_received;

  // FIXME
  CoInitializeEx (NULL, COINIT_MULTITHREADED);
}

static void
gst_bdasrc_set_property (GObject * _object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBdaSrc *object;

  g_return_if_fail (GST_IS_BDASRC (_object));
  object = GST_BDASRC (_object);

  switch (prop_id) {
    case ARG_BDASRC_FREQUENCY:
      object->frequency = g_value_get_uint (value);
      break;
    case ARG_BDASRC_SYM_RATE:
      object->symbol_rate = g_value_get_uint (value);
      break;
    case ARG_BDASRC_BANDWIDTH:
      object->bandwidth = g_value_get_int (value);
      break;
    case ARG_BDASRC_CODE_RATE_HP:
      object->code_rate_hp = g_value_get_enum (value);
      break;
    case ARG_BDASRC_CODE_RATE_LP:
      object->code_rate_lp = g_value_get_enum (value);
      break;
    case ARG_BDASRC_GUARD_INTERVAL:
      object->guard_interval = g_value_get_enum (value);
      break;
    case ARG_BDASRC_MODULATION:
      object->modulation = (ModulationType) g_value_get_enum (value);
      break;
    case ARG_BDASRC_TRANSMISSION_MODE:
      object->transmission_mode = g_value_get_enum (value);
      break;
    case ARG_BDASRC_HIERARCHY_INF:
      object->hierarchy_information = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_bdasrc_get_property (GObject * _object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBdaSrc *object;

  g_return_if_fail (GST_IS_BDASRC (_object));
  object = GST_BDASRC (_object);

  switch (prop_id) {
    case ARG_BDASRC_FREQUENCY:
      g_value_set_uint (value, object->frequency);
      break;
    case ARG_BDASRC_SYM_RATE:
      g_value_set_uint (value, object->symbol_rate);
      break;
    case ARG_BDASRC_BANDWIDTH:
      g_value_set_int (value, object->bandwidth);
      break;
    case ARG_BDASRC_CODE_RATE_HP:
      g_value_set_enum (value, object->code_rate_hp);
      break;
    case ARG_BDASRC_CODE_RATE_LP:
      g_value_set_enum (value, object->code_rate_lp);
      break;
    case ARG_BDASRC_GUARD_INTERVAL:
      g_value_set_enum (value, object->guard_interval);
      break;
    case ARG_BDASRC_MODULATION:
      g_value_set_enum (value, object->modulation);
      break;
    case ARG_BDASRC_TRANSMISSION_MODE:
      g_value_set_enum (value, object->transmission_mode);
      break;
    case ARG_BDASRC_HIERARCHY_INF:
      g_value_set_enum (value, object->hierarchy_information);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

/* Creates the filter graph. */
static gboolean
gst_bdasrc_create_graph (GstBdaSrc * src)
{
  HRESULT res = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_ALL,
      __uuidof (IGraphBuilder), (LPVOID *) & src->filter_graph);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create filter graph");
    return FALSE;
  }

  res = src->filter_graph->QueryInterface (&src->media_control);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to get media control");
    return FALSE;
  }

  ICreateDevEnumPtr sys_dev_enum;
  res = sys_dev_enum.CreateInstance (CLSID_SystemDeviceEnum);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to enumerate BDA devices");
    return FALSE;
  }

  IEnumMonikerPtr enum_tuner;
  res =
      sys_dev_enum->CreateClassEnumerator (KSCATEGORY_BDA_NETWORK_TUNER,
      &enum_tuner, 0);
  if (res == S_FALSE) {
    /* The device category does not exist or is empty. */
    GST_ERROR_OBJECT (src, "No BDA tuner devices");
    return FALSE;
  } else if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to enumerate BDA tuner devices");
    return FALSE;
  }

  if (src->adapter_number > 0) {
    res = enum_tuner->Skip (src->adapter_number);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "BDA adapter %d doesn't exist",
          src->adapter_number);
      return FALSE;
    }
  }

  IMonikerPtr tuner_moniker;
  ULONG fetched;
  res = enum_tuner->Next (1, &tuner_moniker, &fetched);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to get BDA tuner");
    return FALSE;
  }

  res = tuner_moniker->BindToObject (NULL, NULL, IID_IBaseFilter,
      (void **) &src->tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to bind to BDA tuner");
    return FALSE;
  }

  /* FIXME: Hard coded to DVB-C */
  IDVBTuningSpacePtr tuning_space;
  res = tuning_space.CreateInstance (__uuidof (DVBTuningSpace));
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create tuning space");
    return FALSE;
  }
  tuning_space->put__NetworkType (DVB_CABLE_TV_NETWORK_TYPE);
  tuning_space->put_SystemType (DVB_Cable);

  /* FIXME: Hard coded to DVB-C */
  IDVBCLocatorPtr locator;
  res = locator.CreateInstance (__uuidof (DVBCLocator));
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create DVB-C locator");
    return FALSE;
  }
  locator->put_CarrierFrequency (src->frequency);
  locator->put_SymbolRate (src->symbol_rate);
  locator->put_Modulation (src->modulation);
  locator->put_InnerFEC (BDA_FEC_METHOD_NOT_SET);
  locator->put_InnerFECRate (BDA_BCC_RATE_NOT_SET);
  locator->put_OuterFEC (BDA_FEC_METHOD_NOT_SET);
  locator->put_OuterFECRate (BDA_BCC_RATE_NOT_SET);

  /* FIXME: Hard coded to DVB-C */
  CLSID network_type = CLSID_DVBCNetworkProvider;
  IBaseFilterPtr network_provider;

  res = network_provider.CreateInstance (network_type);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create network provider");
    return FALSE;
  }

  res = src->filter_graph->AddFilter (network_provider, L"Network Provider");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to add network provider to graph");
    return FALSE;
  }

  IScanningTunerPtr tuner;
  res = network_provider->QueryInterface (&tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to get tuner interface");
    return FALSE;
  }

  IDVBTuneRequestPtr dvb_tune_request;
  ITuneRequestPtr tune_request;

  res = tuning_space->CreateTuneRequest (&tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create tune request");
    return FALSE;
  }

  res = tune_request->QueryInterface (&dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to get DVB tune request interface");
    return FALSE;
  }

  res = dvb_tune_request->put_Locator (locator);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to put locator");
    return FALSE;
  }

  res = tuner->Validate (dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to validate tune request");
    return FALSE;
  }

  res = tuner->put_TuneRequest (dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to submit tune request");
    return FALSE;
  }

  res = src->filter_graph->AddFilter (src->tuner, L"Tuner device");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to add tuner to filter graph");
    return FALSE;
  }

  res =
      gst_bdasrc_connect_filters (src, network_provider, src->tuner,
      src->filter_graph);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to connect tuner: 0x%x", res);
    return FALSE;
  }

  IBaseFilterPtr demux;
  res = demux.CreateInstance (CLSID_MPEG2Demultiplexer);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create MPEG2Demultiplexer");
    return FALSE;
  }

  res = src->filter_graph->AddFilter (demux, L"Demux");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to add demux filter to graph");
    return FALSE;
  }

  IBaseFilterPtr ts_capture;
  res = ts_capture.CreateInstance (CLSID_SampleGrabber);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to create TS capture");
    return FALSE;
  }

  res = src->filter_graph->AddFilter (ts_capture, L"TsSampleGrabber");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to add TS capture filter to graph");
    return FALSE;
  }

  res = gst_bdasrc_connect_filters (src, ts_capture, demux, src->filter_graph);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to connect TS capture");
    return FALSE;
  }

  IBaseFilterPtr tif;
  res =
      gst_bdasrc_load_filter (src, sys_dev_enum,
      KSCATEGORY_BDA_TRANSPORT_INFORMATION, demux, &tif);

  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to load transport information filter");
    return FALSE;
  }

  return TRUE;
}

static void
gst_bdasrc_finalize (GObject * object)
{
  GstBdaSrc *src;

  GST_DEBUG_OBJECT (object, "gst_bdasrc_finalize");

  g_return_if_fail (GST_IS_BDASRC (object));
  src = GST_BDASRC (object);

  if (G_OBJECT_CLASS (parent_class)->finalize)
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

gboolean
gst_bdasrc_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gstbdasrc_debug, "bdasrc", 0, "BDA Source Element");

  return gst_element_register (plugin, "bdasrc", GST_RANK_NONE,
      GST_TYPE_BDASRC);
}

static void
gst_bdasrc_sample_received (GstBdaSrc * self, gpointer data, gsize size)
{
  GstBuffer *buffer;
  GstMemory *memory;

  g_mutex_lock (&self->lock);

  /* FIXME: Hard coded max size. */
  while (g_queue_get_length (&self->ts_samples) >= 100) {
    buffer = (GstBuffer *) g_queue_pop_head (&self->ts_samples);
    GST_WARNING_OBJECT (self, "Dropping TS sample");
    gst_buffer_unref (buffer);
  }

  buffer = gst_buffer_new ();
  memory = gst_allocator_alloc (NULL, size, NULL);
  gst_buffer_insert_memory (buffer, -1, memory);

  g_queue_push_tail (&self->ts_samples, buffer);
  g_cond_signal (&self->cond);

  g_mutex_unlock (&self->lock);
}

static GstFlowReturn
gst_bdasrc_create (GstPushSrc * element, GstBuffer ** buf)
{
  // FIXME: read transport stream buffers from the queue
  return GST_FLOW_ERROR;
}

static GstStateChangeReturn
gst_bdasrc_change_state (GstElement * element, GstStateChange transition)
{
  GstBdaSrc *src;
  GstStateChangeReturn ret;

  src = GST_BDASRC (element);
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      gst_bdasrc_create_graph (src);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_bdasrc_start (GstBaseSrc * bsrc)
{
  GstBdaSrc *src = GST_BDASRC (bsrc);

  HRESULT hr = src->media_control->Run ();
  if (SUCCEEDED (hr)) {
    return TRUE;
  } else {
    src->media_control->Stop ();
    return FALSE;
  }
}

static gboolean
gst_bdasrc_stop (GstBaseSrc * bsrc)
{
  GstBdaSrc *src = GST_BDASRC (bsrc);

  src->media_control->Pause ();
  src->media_control->Stop ();

  if (src->tuner) {
    src->tuner->Release ();
    src->tuner = NULL;
  }
  if (src->filter_graph) {
    src->filter_graph->Release ();
    src->filter_graph = NULL;
  }
  if (src->media_control) {
    src->media_control->Release ();
    src->media_control = NULL;
  }

  return TRUE;
}

static gboolean
gst_bdasrc_is_seekable (GstBaseSrc * bsrc)
{
  return FALSE;
}

static gboolean
gst_bdasrc_get_size (GstBaseSrc * src, guint64 * size)
{
  return FALSE;
}

static gboolean
gst_bdasrc_tune (GstBdaSrc * object)
{
  /* FIXME */
  return TRUE;
}

static HRESULT
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

static HRESULT
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    bda, "BDA Source",
    gst_bdasrc_plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
