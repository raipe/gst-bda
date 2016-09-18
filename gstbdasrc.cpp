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
#include <control.h>
#include <dshow.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <bdatypes.h>
#include <bdamedia.h>
#include <bdaiface.h>
#include "gstbdagrabber.h"
#include "gstbdautil.h"

GST_DEBUG_CATEGORY (gstbdasrc_debug);

enum
{
  PROP_0,
  PROP_BUFFER_SIZE,
  PROP_DEVICE_INDEX,
  PROP_FREQUENCY,
  PROP_SYMBOL_RATE,
  PROP_BANDWIDTH,
  PROP_CODE_RATE_HP,
  PROP_CODE_RATE_LP,
  PROP_GUARD_INTERVAL,
  PROP_MODULATION,
  PROP_TRANSMISSION_MODE,
  PROP_HIERARCHY,
  PROP_ORBITAL_POSITION,
  PROP_WEST_POSITION,
  PROP_POLARISATION,
  PROP_INNER_FEC_RATE
};

#define DEFAULT_BUFFER_SIZE 50
#define DEFAULT_DEVICE_INDEX 0
#define DEFAULT_FREQUENCY 0
#define DEFAULT_SYMBOL_RATE 0
#define DEFAULT_BANDWIDTH 8
#define DEFAULT_GUARD BDA_GUARD_1_16
#define DEFAULT_MODULATION BDA_MOD_16QAM
#define DEFAULT_TRANSMISSION_MODE BDA_XMIT_MODE_8K
#define DEFAULT_HIERARCHY BDA_HALPHA_NOT_SET
#define DEFAULT_ORBITAL_POSITION 0
#define DEFAULT_WEST_POSITION FALSE
#define DEFAULT_POLARISATION BDA_POLARISATION_NOT_SET
#define DEFAULT_INNER_FEC_RATE BDA_BCC_RATE_NOT_SET

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

#define GST_TYPE_BDASRC_FEC_RATE (gst_bdasrc_fec_rate_get_type ())
static GType
gst_bdasrc_fec_rate_get_type (void)
{
  static GType bdasrc_fec_rate_type = 0;
  static GEnumValue fec_rate_types[] = {
    {BDA_BCC_RATE_1_2, "1/2", "1/2"},
    {BDA_BCC_RATE_2_3, "2/3", "2/3"},
    {BDA_BCC_RATE_3_4, "3/4", "3/4"},
    {BDA_BCC_RATE_4_5, "4/5", "4/5"},
    {BDA_BCC_RATE_5_6, "5/6", "5/6"},
    {BDA_BCC_RATE_6_7, "6/7", "6/7"},
    {BDA_BCC_RATE_7_8, "7/8", "7/8"},
    {BDA_BCC_RATE_8_9, "8/9", "8/9"},
    {BDA_BCC_RATE_NOT_SET, "NONE", "NONE"},
    {0, NULL, NULL},
  };

  if (!bdasrc_fec_rate_type) {
    bdasrc_fec_rate_type =
        g_enum_register_static ("GstBdaSrcFecRate", fec_rate_types);
  }
  return bdasrc_fec_rate_type;
}

#define GST_TYPE_BDASRC_POLARISATION (gst_bdasrc_polarisation_get_type ())
static GType
gst_bdasrc_polarisation_get_type (void)
{
  static GType bdasrc_polarisation_type = 0;
  static GEnumValue polarisation_types[] = {
    {BDA_POLARISATION_LINEAR_H, "Linear horizontal", "Linear horizontal"},
    {BDA_POLARISATION_LINEAR_V, "Linear vertical", "Linear vertical"},
    {BDA_POLARISATION_CIRCULAR_L, "Circular left", "Circular left"},
    {BDA_POLARISATION_CIRCULAR_R, "Circular right", "Circular right"},
    {BDA_POLARISATION_NOT_SET, "NONE", "NONE"},
    {0, NULL, NULL},
  };

  if (!bdasrc_polarisation_type) {
    bdasrc_polarisation_type =
        g_enum_register_static ("GstBdaSrcPolarisation", polarisation_types);
  }
  return bdasrc_polarisation_type;
}

static void gst_bdasrc_finalize (GObject * object);
static void gst_bdasrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bdasrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_bdasrc_sample_received (GstBdaSrc * self, gpointer data,
    gsize size);
static GstFlowReturn gst_bdasrc_create (GstPushSrc * src, GstBuffer ** buffer);

static GstStateChangeReturn gst_bdasrc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_bdasrc_unlock (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_unlock_stop (GstBaseSrc * bsrc);

static gboolean gst_bdasrc_tune (GstBdaSrc * self);

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
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_bdasrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_bdasrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_bdasrc_create);

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size",
          "Size of internal buffer in number of TS samples", 1,
          G_MAXINT, DEFAULT_BUFFER_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_uint ("device", "Device index", "BDA device index, e.g. 0"
          " for the first device", 0, 64, DEFAULT_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FREQUENCY,
      g_param_spec_uint ("frequency", "Frequency", "Frequency in kHz",
          0, G_MAXUINT, DEFAULT_FREQUENCY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SYMBOL_RATE,
      g_param_spec_uint ("symbol-rate", "Symbol rate in kHz",
          "Symbol Rate in kHz (DVB-S, DVB-C)",
          0, G_MAXUINT, DEFAULT_SYMBOL_RATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH,
      g_param_spec_int ("bandwidth", "Bandwidth in MHz",
          "Bandwidth in MHz (DVB-T)", 5, 8, DEFAULT_BANDWIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_GUARD_INTERVAL,
      g_param_spec_enum ("guard-interval", "Guard interval",
          "Guard Interval (DVB-T)",
          GST_TYPE_BDASRC_GUARD_INTERVAL, DEFAULT_GUARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MODULATION,
      g_param_spec_enum ("modulation", "Modulation",
          "Modulation (DVB-T and DVB-C)",
          GST_TYPE_BDASRC_MODULATION, DEFAULT_MODULATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_TRANSMISSION_MODE,
      g_param_spec_enum ("transmission-mode", "Transmission mode",
          "Transmission Mode (DVB-T)", GST_TYPE_BDASRC_TRANSMISSION_MODE,
          DEFAULT_TRANSMISSION_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_HIERARCHY,
      g_param_spec_enum ("hierarchy", "hierarchy",
          "Hierarchy Information (DVB-T)",
          GST_TYPE_BDASRC_HIERARCHY, DEFAULT_HIERARCHY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_ORBITAL_POSITION,
      g_param_spec_int ("orbital-position", "Orbital position",
          "Satellite's longitude in tenths of a degree (DVB-S)", 0, G_MAXINT,
          DEFAULT_ORBITAL_POSITION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WEST_POSITION,
      g_param_spec_boolean ("west-position", "West position",
          "Longitudinal position, true for west longitude (DVB-S)",
          DEFAULT_WEST_POSITION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_POLARISATION,
      g_param_spec_enum ("polarisation", "Polarisation",
          "Polarisation (DVB-S)", GST_TYPE_BDASRC_POLARISATION,
          DEFAULT_POLARISATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INNER_FEC_RATE,
      g_param_spec_enum ("inner-fec-rate", "Inner FEC rate",
          "Inner FEC rate (DVB-S)", GST_TYPE_BDASRC_FEC_RATE,
          DEFAULT_INNER_FEC_RATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_bdasrc_init (GstBdaSrc * self)
{
  CoInitializeEx (NULL, COINIT_MULTITHREADED);

  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);

  self->buffer_size = DEFAULT_BUFFER_SIZE;
  self->input_type = GST_BDA_UNKNOWN;

  self->device_index = DEFAULT_DEVICE_INDEX;
  self->frequency = 0;
  self->symbol_rate = DEFAULT_SYMBOL_RATE;
  self->bandwidth = DEFAULT_BANDWIDTH;
  self->guard_interval = DEFAULT_GUARD;
  self->modulation = DEFAULT_MODULATION;
  self->transmission_mode = DEFAULT_TRANSMISSION_MODE;
  self->hierarchy_information = DEFAULT_HIERARCHY;
  self->orbital_position = DEFAULT_ORBITAL_POSITION;
  self->west_position = DEFAULT_WEST_POSITION;
  self->polarisation = DEFAULT_POLARISATION;
  self->inner_fec_rate = DEFAULT_INNER_FEC_RATE;

  self->network_tuner = NULL;
  self->receiver = NULL;
  self->filter_graph = NULL;
  self->media_control = NULL;
  self->ts_grabber = new GstBdaGrabber (self);

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  g_queue_init (&self->ts_samples);

  self->sample_received = gst_bdasrc_sample_received;
}

static void
gst_bdasrc_set_property (GObject * _object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBdaSrc *self;

  g_return_if_fail (GST_IS_BDASRC (_object));
  self = GST_BDASRC (_object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      break;
    case PROP_DEVICE_INDEX:
      self->device_index = g_value_get_uint (value);
      break;
    case PROP_FREQUENCY:
      self->frequency = g_value_get_uint (value);
      break;
    case PROP_SYMBOL_RATE:
      self->symbol_rate = g_value_get_uint (value);
      break;
    case PROP_BANDWIDTH:
      self->bandwidth = g_value_get_int (value);
      break;
    case PROP_GUARD_INTERVAL:
      self->guard_interval = (GuardInterval) g_value_get_enum (value);
      break;
    case PROP_MODULATION:
      self->modulation = (ModulationType) g_value_get_enum (value);
      break;
    case PROP_TRANSMISSION_MODE:
      self->transmission_mode = (TransmissionMode) g_value_get_enum (value);
      break;
    case PROP_HIERARCHY:
      self->hierarchy_information = (HierarchyAlpha) g_value_get_enum (value);
      break;
    case PROP_ORBITAL_POSITION:
      self->orbital_position = g_value_get_int (value);
      break;
    case PROP_WEST_POSITION:
      self->west_position = g_value_get_boolean (value);
      break;
    case PROP_POLARISATION:
      self->polarisation = (Polarisation) g_value_get_enum (value);
      break;
    case PROP_INNER_FEC_RATE:
      self->inner_fec_rate =
          (BinaryConvolutionCodeRate) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_bdasrc_get_property (GObject * _object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBdaSrc *self;

  g_return_if_fail (GST_IS_BDASRC (_object));
  self = GST_BDASRC (_object);

  switch (prop_id) {
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_uint (value, self->device_index);
      break;
    case PROP_FREQUENCY:
      g_value_set_uint (value, self->frequency);
      break;
    case PROP_SYMBOL_RATE:
      g_value_set_uint (value, self->symbol_rate);
      break;
    case PROP_BANDWIDTH:
      g_value_set_int (value, self->bandwidth);
      break;
    case PROP_GUARD_INTERVAL:
      g_value_set_enum (value, self->guard_interval);
      break;
    case PROP_MODULATION:
      g_value_set_enum (value, self->modulation);
      break;
    case PROP_TRANSMISSION_MODE:
      g_value_set_enum (value, self->transmission_mode);
      break;
    case PROP_HIERARCHY:
      g_value_set_enum (value, self->hierarchy_information);
      break;
    case PROP_ORBITAL_POSITION:
      g_value_set_int (value, self->orbital_position);
      break;
    case PROP_WEST_POSITION:
      g_value_set_boolean (value, self->west_position);
      break;
    case PROP_POLARISATION:
      g_value_set_enum (value, self->polarisation);
      break;
    case PROP_INNER_FEC_RATE:
      g_value_set_enum (value, self->inner_fec_rate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

/* Creates the DirectShow filter graph. */
static gboolean
gst_bdasrc_create_graph (GstBdaSrc * self)
{
  HRESULT res = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_ALL,
      __uuidof (IGraphBuilder), (LPVOID *) & self->filter_graph);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to create filter graph");
    return FALSE;
  }

  res = self->filter_graph->QueryInterface (&self->media_control);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to get media control");
    return FALSE;
  }

  ICreateDevEnumPtr sys_dev_enum;
  res = sys_dev_enum.CreateInstance (CLSID_SystemDeviceEnum);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to enumerate BDA devices");
    return FALSE;
  }

  IEnumMonikerPtr enum_tuner;
  res =
      sys_dev_enum->CreateClassEnumerator (KSCATEGORY_BDA_NETWORK_TUNER,
      &enum_tuner, 0);
  if (res == S_FALSE) {
    /* The device category does not exist or is empty. */
    GST_ERROR_OBJECT (self, "No BDA tuner devices");
    return FALSE;
  } else if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to enumerate BDA tuner devices");
    return FALSE;
  }

  if (self->device_index > 0) {
    res = enum_tuner->Skip (self->device_index);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (self, "BDA device %d doesn't exist",
          self->device_index);
      return FALSE;
    }
  }

  IMonikerPtr tuner_moniker;
  ULONG fetched;
  res = enum_tuner->Next (1, &tuner_moniker, &fetched);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to get BDA tuner");
    return FALSE;
  }

  std::string tuner_name = bda_get_tuner_name (tuner_moniker);

  res = tuner_moniker->BindToObject (NULL, NULL, IID_IBaseFilter,
      (void **) &self->network_tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to bind to BDA tuner '%s'",
        tuner_name.c_str ());
    return FALSE;
  }

  self->input_type = gst_bdasrc_get_input_type (self);
  if (self->input_type == GST_BDA_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Can't determine device type for BDA tuner '%s'",
        tuner_name.c_str ());
    return FALSE;
  }

  GST_INFO_OBJECT (self, "Using %s tuner device '%s'",
      gst_bdasrc_get_input_type_name (self->input_type), tuner_name.c_str ());

  ITuningSpacePtr tuning_space;
  if (!gst_bdasrc_create_tuning_space (self, tuning_space)) {
    GST_ERROR_OBJECT (self, "Unable to create tuning space");
    return FALSE;
  }

  CLSID network_type;
  if (!gst_bdasrc_get_network_type (self->input_type, network_type)) {
    GST_ERROR_OBJECT (self, "Can't determine network type");
    return FALSE;
  }

  IBaseFilterPtr network_provider;

  res = network_provider.CreateInstance (network_type);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to create network provider");
    return FALSE;
  }

  res = self->filter_graph->AddFilter (network_provider, L"Network Provider");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to add network provider to graph");
    return FALSE;
  }

  IScanningTunerPtr tuner;
  res = network_provider->QueryInterface (&tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to get tuner interface");
    return FALSE;
  }

  IDVBTuneRequestPtr dvb_tune_request;
  ITuneRequestPtr tune_request;

  res = tuning_space->CreateTuneRequest (&tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to create tune request");
    return FALSE;
  }

  res = tune_request->QueryInterface (&dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to get DVB tune request interface");
    return FALSE;
  }

  if (!gst_bdasrc_init_tune_request (self, dvb_tune_request)) {
    GST_ERROR_OBJECT (self, "Unable to initialise tune request");
    return FALSE;
  }

  res = tuner->Validate (dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to validate tune request");
    return FALSE;
  }

  res = tuner->put_TuneRequest (dvb_tune_request);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to submit tune request");
    return FALSE;
  }

  res = self->filter_graph->AddFilter (self->network_tuner, L"Tuner device");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to add tuner to filter graph");
    return FALSE;
  }

  res =
      gst_bdasrc_connect_filters (self, network_provider, self->network_tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to connect tuner: %s (0x%lx)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  IBaseFilterPtr demux;
  res = demux.CreateInstance (CLSID_MPEG2Demultiplexer);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to create MPEG2Demultiplexer");
    return FALSE;
  }

  res = self->filter_graph->AddFilter (demux, L"Demux");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to add demux filter to graph");
    return FALSE;
  }

  IBaseFilterPtr ts_capture;
  if (!gst_bdasrc_create_ts_capture (self, sys_dev_enum, ts_capture)) {
    return FALSE;
  }

  res = gst_bdasrc_connect_filters (self, ts_capture, demux);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to connect TS capture to demux: %s (0x%lx)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  IBaseFilterPtr tif;
  res =
      gst_bdasrc_load_filter (self, sys_dev_enum,
      KSCATEGORY_BDA_TRANSPORT_INFORMATION, demux, &tif);

  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Unable to load transport information filter");
    return FALSE;
  }

  return TRUE;
}

/* Releases the DirectShow filter graph. */
static void
gst_bdasrc_release_graph (GstBdaSrc * self)
{
  if (self->media_control) {
    self->media_control->Stop ();
    self->media_control->Release ();
    self->media_control = NULL;
  }
  if (self->receiver && self->receiver != self->network_tuner) {
    self->receiver->Release ();
    self->receiver = NULL;
  }
  if (self->network_tuner) {
    self->network_tuner->Release ();
    self->network_tuner = NULL;
  }
  if (self->filter_graph) {
    self->filter_graph->Release ();
    self->filter_graph = NULL;
  }
}

static void
gst_bda_release_samples (GstBdaSrc * self)
{
  g_mutex_lock (&self->lock);
  g_queue_foreach (&self->ts_samples, (GFunc) gst_buffer_unref, NULL);
  g_queue_clear (&self->ts_samples);
  g_mutex_unlock (&self->lock);
}

static void
gst_bdasrc_finalize (GObject * object)
{
  GstBdaSrc *self;

  g_return_if_fail (GST_IS_BDASRC (object));
  self = GST_BDASRC (object);

  gst_bda_release_samples (self);
  gst_bdasrc_release_graph (self);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);
  delete self->ts_grabber;

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
  GstMapInfo map;

  g_mutex_lock (&self->lock);

  if (!self->flushing) {
    while (g_queue_get_length (&self->ts_samples) >= self->buffer_size) {
      buffer = (GstBuffer *) g_queue_pop_head (&self->ts_samples);
      GST_WARNING_OBJECT (self, "Dropping TS sample");
      gst_buffer_unref (buffer);
    }

    buffer = gst_buffer_new_and_alloc (size);
    gst_buffer_map (buffer, &map, GST_MAP_WRITE);
    memcpy (map.data, data, size);
    gst_buffer_unmap (buffer, &map);

    g_queue_push_tail (&self->ts_samples, buffer);
    g_cond_signal (&self->cond);
  }
  g_mutex_unlock (&self->lock);
}

static GstFlowReturn
gst_bdasrc_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstBdaSrc *self = GST_BDASRC (src);

  g_mutex_lock (&self->lock);
  while (g_queue_is_empty (&self->ts_samples) && !self->flushing) {
    g_cond_wait (&self->cond, &self->lock);
  }

  *buf = (GstBuffer *) g_queue_pop_head (&self->ts_samples);
  g_mutex_unlock (&self->lock);

  if (self->flushing) {
    if (*buf) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    GST_DEBUG_OBJECT (self, "Flushing");
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;
}

static GstStateChangeReturn
gst_bdasrc_change_state (GstElement * element, GstStateChange transition)
{
  GstBdaSrc *self;
  GstStateChangeReturn ret;

  self = GST_BDASRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_bdasrc_create_graph (self)) {
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      if (self->media_control) {
        self->media_control->Stop ();
      }
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_bdasrc_release_graph (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    return ret;
  }

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      gst_bda_release_samples (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      self->flushing = FALSE;
      if (!gst_bdasrc_tune (self)) {
        ret = GST_STATE_CHANGE_FAILURE;
        gst_bda_release_samples (self);
      }
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_bdasrc_unlock (GstBaseSrc * bsrc)
{
  GstBdaSrc *self = GST_BDASRC (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_bdasrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstBdaSrc *self = GST_BDASRC (bsrc);

  g_mutex_lock (&self->lock);
  self->flushing = FALSE;
  g_queue_foreach (&self->ts_samples, (GFunc) gst_buffer_unref, NULL);
  g_queue_clear (&self->ts_samples);
  g_mutex_unlock (&self->lock);

  return TRUE;
}

static gboolean
gst_bdasrc_tune (GstBdaSrc * self)
{
  HRESULT res = self->media_control->Run ();
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Error starting media control: %s (0x%lx)",
        bda_err_to_str (res).c_str (), res);
    self->media_control->Stop ();
    return FALSE;
  }

  IBDA_TopologyPtr bda_topology;
  res = self->network_tuner->QueryInterface (&bda_topology);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (self, "Error getting BDA topology interface: %s (0x%lx)",
        bda_err_to_str (res).c_str (), res);
    self->media_control->Stop ();
    return FALSE;
  }

  ULONG node_type_count = 0;
  ULONG node_types[32] = { };
  res =
      bda_topology->GetNodeTypes (&node_type_count, _countof (node_types),
      node_types);
  if (FAILED (res)) {
    self->media_control->Stop ();
    GST_WARNING_OBJECT (self,
        "Error getting BDA topology node types: %s (0x%lx)",
        bda_err_to_str (res).c_str (), res);
    return FALSE;
  }

  IBDA_SignalStatisticsPtr signal_stats;
  for (ULONG i = 0; i < node_type_count; i++) {
    IUnknownPtr node;
    res = bda_topology->GetControlNode (0, 1, node_types[i], &node);
    if (res == S_OK) {
      res = node->QueryInterface (&signal_stats);

      BOOLEAN locked = FALSE;
      if (SUCCEEDED (signal_stats->get_SignalLocked (&locked))) {
        return locked;
      }

      break;
    }
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR,
    bda, "BDA Source",
    gst_bdasrc_plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
