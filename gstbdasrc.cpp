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
#define GST_CAT_DEFAULT (gstbdasrc_debug)

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

static void gst_bdasrc_output_frontend_stats (GstBdaSrc * src);

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

static gboolean gst_bdasrc_start (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_stop (GstBaseSrc * bsrc);
static GstStateChangeReturn gst_bdasrc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_bdasrc_unlock (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_unlock_stop (GstBaseSrc * bsrc);

static gboolean gst_bdasrc_is_seekable (GstBaseSrc * bsrc);
static gboolean gst_bdasrc_get_size (GstBaseSrc * src, guint64 * size);

static gboolean gst_bdasrc_tune (GstBdaSrc * object);

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

  if (src->device_index > 0) {
    res = enum_tuner->Skip (src->device_index);
    if (FAILED (res)) {
      GST_ERROR_OBJECT (src, "BDA device %d doesn't exist", src->device_index);
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

  std::string tuner_name = bda_get_tuner_name (tuner_moniker);
  GST_INFO_OBJECT (src, "Using BDA tuner device '%s'", tuner_name.c_str ());

  res = tuner_moniker->BindToObject (NULL, NULL, IID_IBaseFilter,
      (void **) &src->network_tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to bind to BDA tuner");
    return FALSE;
  }

  src->input_type = gst_bdasrc_get_input_type (src);
  if (src->input_type == GST_BDA_UNKNOWN) {
    GST_ERROR_OBJECT (src, "Can't determine device type for BDA tuner '%s'",
        tuner_name.c_str ());
    return FALSE;
  }

  IDVBTuningSpacePtr tuning_space;
  if (!gst_bdasrc_create_tuning_space (src, tuning_space)) {
    GST_ERROR_OBJECT (src, "Unable to create tuning space");
    return FALSE;
  }

  CLSID network_type;
  if (!gst_bdasrc_get_network_type (src->input_type, network_type)) {
    GST_ERROR_OBJECT (src, "Can't determine network type");
    return FALSE;
  }

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

  if (!gst_bdasrc_init_tune_request (src, dvb_tune_request)) {
    GST_ERROR_OBJECT (src, "Unable to initialise tune request");
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

  res = src->filter_graph->AddFilter (src->network_tuner, L"Tuner device");
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to add tuner to filter graph");
    return FALSE;
  }

  res = gst_bdasrc_connect_filters (src, network_provider, src->network_tuner);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to connect tuner: %s (0x%x)",
        bda_err_to_str (res).c_str (), res);
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
  if (!gst_bdasrc_create_ts_capture (src, sys_dev_enum, ts_capture)) {
    return FALSE;
  }

  res = gst_bdasrc_connect_filters (src, ts_capture, demux);
  if (FAILED (res)) {
    GST_ERROR_OBJECT (src, "Unable to connect TS capture to demux: %s (0x%x)",
        bda_err_to_str (res).c_str (), res);
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
  GstBdaSrc *self;

  GST_DEBUG_OBJECT (object, "gst_bdasrc_finalize");

  g_return_if_fail (GST_IS_BDASRC (object));
  self = GST_BDASRC (object);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

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
  while (g_queue_is_empty (&self->ts_samples)) {
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
  GstBdaSrc *src;
  GstStateChangeReturn ret;

  src = GST_BDASRC (element);
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_bdasrc_create_graph (src))
        ret = GST_STATE_CHANGE_FAILURE;
      src->flushing = FALSE;
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_bdasrc_start (GstBaseSrc * base_src)
{
  GstBdaSrc *bda_src = GST_BDASRC (base_src);

  if (!gst_bdasrc_tune (bda_src)) {
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_bdasrc_stop (GstBaseSrc * bsrc)
{
  GstBdaSrc *src = GST_BDASRC (bsrc);

  if (src->media_control) {
    src->media_control->Pause ();
    src->media_control->Stop ();
  }
  if (src->ts_grabber) {
    delete src->ts_grabber;
    src->ts_grabber = NULL;
  }
  if (src->network_tuner) {
    src->network_tuner->Release ();
    src->network_tuner = NULL;
  }
  if (src->filter_graph) {
    src->filter_graph->Release ();
    src->filter_graph = NULL;
  }

  return TRUE;
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
gst_bdasrc_tune (GstBdaSrc * bda_src)
{
  HRESULT res = bda_src->media_control->Run ();
  if (FAILED (res)) {
    bda_src->media_control->Stop ();
    return FALSE;
  }

  IBDA_TopologyPtr bda_topology;
  res = bda_src->network_tuner->QueryInterface (&bda_topology);
  if (FAILED (res)) {
    bda_src->media_control->Stop ();
    return FALSE;
  }

  ULONG node_type_count;
  ULONG node_types[32];
  res =
      bda_topology->GetNodeTypes (&node_type_count, sizeof (node_types),
      node_types);
  if (FAILED (res)) {
    bda_src->media_control->Stop ();
    return FALSE;
  }

  IBDA_SignalStatisticsPtr signal_stats;
  for (ULONG i = 0; i < node_type_count; i++) {
    IUnknown *node = NULL;
    res = bda_topology->GetControlNode (0, 1, node_types[i], &node);
    if (res == S_OK) {
      res = node->QueryInterface (&signal_stats);
      node->Release ();

      BOOLEAN locked;
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
