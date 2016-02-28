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

/**
 * Returns a description for the specified BDA / DirectShow error code.
 */
std::string bda_err_to_str (HRESULT hr);

/**
 * Returns a friendly name for the specified BDA tuner device.
 */
std::string bda_get_tuner_name (IMoniker * tuner_moniker);

/**
 * Determines input device type from tuner's capabilities. FIXME: One device
 * can probably support multiple input types (e.g. DVB-T and DVB-C), we don't
 * currently support that.
 */
GstBdaInputType gst_bdasrc_get_input_type (GstBdaSrc * src);

const char *gst_bdasrc_get_input_type_name (GstBdaInputType input_type);

/**
 * Maps our input type to BDA network type.
 */
BOOL gst_bdasrc_get_network_type (GstBdaInputType input_type,
    CLSID & network_type);

/**
 * Returns a BDA tuning space according to input device type. If a matching
 * tuning space is found in system tuning spaces, it is returned. Otherwise
 * a new one is created.
 * @return TRUE if tuning space was found or created successfully
 */
BOOL gst_bdasrc_create_tuning_space (GstBdaSrc * src,
    ITuningSpacePtr & tuning_space);

/**
 * Initializes a tune request according to input device type. FIXME: Use a more
 * generic interface than IDVBTuneRequest.
 * @return TRUE if tune request was initialized successfully
 */
BOOL gst_bdasrc_init_tune_request (GstBdaSrc * src,
    IDVBTuneRequestPtr & tune_request);

/**
 * Connects the specified DirectShow filters. Enumerates filter pins and
 * tries to connect any unconnected pins (upstream output pin -> downstream
 * input pin).
 * @return S_OK if filters were connected successfully, otherwise an error
 */
HRESULT gst_bdasrc_connect_filters (GstBdaSrc * src,
    IBaseFilter * filter_upstream, IBaseFilter * filter_downstream);

/**
 * Creates a filter with the specified CLSID and connects it to the specified
 * upstream filter.
 * @return S_OK if filter was created successfully, otherwise an error
 */
HRESULT gst_bdasrc_load_filter (GstBdaSrc * src, ICreateDevEnum * sys_dev_enum,
    REFCLSID clsid, IBaseFilter * upstream_filter,
    IBaseFilter ** downstream_filter);

/**
 * Creates a transport stream capture filter and connects it to our
 * GstBdaGrabber.
 * @return TRUE if capture filter was created successfully
 */
BOOL gst_bdasrc_create_ts_capture (GstBdaSrc * bda_src,
    ICreateDevEnum * sys_dev_enum, IBaseFilterPtr & ts_capture);

#endif
