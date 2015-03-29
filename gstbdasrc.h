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

class GstBdaGrabber;

G_BEGIN_DECLS

#define GST_TYPE_BDASRC \
  (gst_bdasrc_get_type())
#define GST_BDASRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BDASRC,GstBdaSrc))
#define GST_BDASRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BDASRC,GstBdaSrcClass))
#define GST_IS_BDASRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BDASRC))
#define GST_IS_BDASRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BDASRC))

  typedef struct _GstBdaSrc GstBdaSrc;
  typedef struct _GstBdaSrcClass GstBdaSrcClass;
  typedef struct _GstBdaSrcParam GstBdaSrcParam;

  struct _GstBdaSrc
  {
    GstPushSrc element;
    GstPad *srcpad;

    gboolean need_tune;

    int device_index;

    int frequency;
    int symbol_rate;

    int bandwidth;
    int code_rate_hp;
    int code_rate_lp;
    ModulationType modulation;
    int guard_interval;
    int transmission_mode;
    int hierarchy_information;

    IBaseFilter *tuner;
    IBaseFilter *capture;
    IGraphBuilder *filter_graph;
    IMediaControl *media_control;

    GstBdaGrabber *ts_grabber;

    GCond cond;
    GMutex lock;
    gboolean flushing;
    /* MPEG-2 transport stream samples. */
    GQueue ts_samples;

    /* Callback function for GstBdaGrabber. */
    void (*sample_received) (GstBdaSrc *bda_src, gpointer data, gsize size);
  };

  struct _GstBdaSrcClass
  {
    GstPushSrcClass parent_class;
  };

  GType gst_bdasrc_get_type (void);
  gboolean gst_bdasrc_plugin_init (GstPlugin *plugin);

G_END_DECLS

#endif
