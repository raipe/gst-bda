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

#ifndef __GST_BDAUTIL_H__
#define __GST_BDAUTIL_H__

#include <winsock2.h>
#include <string>
#include <comdef.h>
#include <winerror.h>
#include <qedit.h>
#include "gstbdasrc.h"

std::string bda_err_to_str (HRESULT hr);
std::string bda_get_tuner_name (IMoniker * tuner_moniker);

GstBdaInputType gst_bdasrc_get_input_type (GstBdaSrc * src);

HRESULT gst_bdasrc_connect_filters(GstBdaSrc * src, IBaseFilter * filter_upstream,
    IBaseFilter * filter_downstream);
HRESULT gst_bdasrc_load_filter(GstBdaSrc * src, ICreateDevEnum * sys_dev_enum,
    REFCLSID clsid, IBaseFilter * upstream_filter, IBaseFilter ** downstream_filter);
BOOL gst_bdasrc_create_ts_capture(GstBdaSrc * bda_src,
    ICreateDevEnum * sys_dev_enum, IBaseFilterPtr &ts_capture);

#endif
