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

#ifndef __GST_BDASRC_H__
#define __GST_BDASRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <winsock2.h>
#include <bdatypes.h>
#include <control.h>
#include <tuner.h>
#include "gstbdatypes.h"

GST_DEBUG_CATEGORY_EXTERN(gstbdasrc_debug);
#define GST_CAT_DEFAULT (gstbdasrc_debug)

class GstBdaGrabber;

G_BEGIN_DECLS

#define GST_TYPE_BDASRC (gst_bdasrc_get_type())
#define GST_BDASRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BDASRC,GstBdaSrc))
#define GST_BDASRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BDASRC,GstBdaSrcClass))
#define GST_IS_BDASRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BDASRC))
#define GST_IS_BDASRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BDASRC))

typedef struct _GstBdaSrc GstBdaSrc;
typedef struct _GstBdaSrcClass GstBdaSrcClass;
typedef struct _GstBdaSrcParam GstBdaSrcParam;

struct _GstBdaSrc {
  GstPushSrc element;
  GstPad *srcpad;

  gboolean need_tune;

  int device_index;

  GstBdaInputType input_type;

  /* Frequency in kHz */
  int frequency;
  /* Symbol rate in kHz */
  int symbol_rate;

  /* Bandwidth in MHz */
  int bandwidth;
  ModulationType modulation;
  GuardInterval guard_interval;
  TransmissionMode transmission_mode;
  HierarchyAlpha hierarchy_information;
  /* DVB-S: Satellite's longitude in tenths of a degree */
  int orbital_position;
  /* DVB-S: TRUE for west longitude */
  gboolean west_position;
  Polarisation polarisation;
  BinaryConvolutionCodeRate inner_fec_rate;

  /* BDA network tuner filter */
  IBaseFilter *network_tuner;
  /* BDA receiver filter */
  IBaseFilter *receiver;
  IGraphBuilder *filter_graph;
  IMediaControl *media_control;

  GstBdaGrabber *ts_grabber;

  GCond cond;
  GMutex lock;
  gboolean flushing;
  /* Queue of MPEG-2 transport stream samples. */
  GQueue ts_samples;
  /* Max size of ts_samples. */
  guint buffer_size;

  /* Callback function for GstBdaGrabber. */
  void (*sample_received) (GstBdaSrc *bda_src, gpointer data, gsize size);
};

struct _GstBdaSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_bdasrc_get_type (void);
gboolean gst_bdasrc_plugin_init (GstPlugin *plugin);

G_END_DECLS

#endif
