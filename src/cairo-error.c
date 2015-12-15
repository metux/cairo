/* -*- Mode: c; c-basic-offset: 4; indent-tabs-mode: t; tab-width: 8; -*- */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2002 University of Southern California
 * Copyright © 2005 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"
#include "cairo-private.h"

#include "cairo-compiler-private.h"
#include "cairo-error-private.h"

#include <assert.h>

/**
 * _cairo_error:
 * @status: a status value indicating an error, (eg. not
 * %CAIRO_STATUS_SUCCESS)
 *
 * Checks that status is an error status, but does nothing else.
 *
 * All assignments of an error status to any user-visible object
 * within the cairo application should result in a call to
 * _cairo_error().
 *
 * The purpose of this function is to allow the user to set a
 * breakpoint in _cairo_error() to generate a stack trace for when the
 * user causes cairo to detect an error.
 *
 * Return value: the error status.
 **/
cairo_status_t
_cairo_error (cairo_status_t status)
{
    CAIRO_ENSURE_UNIQUE;
    assert (_cairo_status_is_error (status));

    return status;
}

COMPILE_TIME_ASSERT ((int)CAIRO_INT_STATUS_LAST_STATUS == (int)CAIRO_STATUS_LAST_STATUS);

const char* cairo_status_text(cairo_status_t status)
{
    switch (status) {
	case CAIRO_STATUS_SUCCESS:			return "SUCCESS";
	case CAIRO_STATUS_NO_MEMORY:			return "NO_MEMORY";
	case CAIRO_STATUS_INVALID_RESTORE:		return "INVALID_RESTORE";
	case CAIRO_STATUS_INVALID_POP_GROUP:		return "INVALID_POP_GROUP";
	case CAIRO_STATUS_NO_CURRENT_POINT:		return "NO_CURRENT_POINT";
	case CAIRO_STATUS_INVALID_MATRIX:		return "INVALID_MATRIX";
	case CAIRO_STATUS_INVALID_STATUS:		return "INVALID_STATUS";
	case CAIRO_STATUS_NULL_POINTER:			return "NULL_POINTER";
	case CAIRO_STATUS_INVALID_STRING:		return "INVALID_STATUS";
	case CAIRO_STATUS_INVALID_PATH_DATA:		return "INVALID_PATH_DATA";
	case CAIRO_STATUS_READ_ERROR:			return "READ_ERROR";
	case CAIRO_STATUS_WRITE_ERROR:			return "WRITE_ERROR";
	case CAIRO_STATUS_SURFACE_FINISHED:		return "SURFACE_FINISHED";
	case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:	return "SURFACE_TYPE_MISMATCH";
	case CAIRO_STATUS_PATTERN_TYPE_MISMATCH:	return "PATTERN_TYPE_MISMATCH";
	case CAIRO_STATUS_INVALID_CONTENT:		return "INVALID_CONTENT";
	case CAIRO_STATUS_INVALID_FORMAT:		return "INVALID_FORMAT";
	case CAIRO_STATUS_INVALID_VISUAL:		return "INVALID_VISUAL";
	case CAIRO_STATUS_FILE_NOT_FOUND:		return "FILE_NOT_FOUND";
	case CAIRO_STATUS_INVALID_DASH:			return "INVALID_DASH";
	case CAIRO_STATUS_INVALID_DSC_COMMENT:		return "INVALID_DSC_COMMENT";
	case CAIRO_STATUS_INVALID_INDEX:		return "INVALID_INDEX";
	case CAIRO_STATUS_CLIP_NOT_REPRESENTABLE:	return "CLIP_NOT_REPRESENTABLE";
	case CAIRO_STATUS_TEMP_FILE_ERROR:		return "TEMP_FILE_ERROR";
	case CAIRO_STATUS_INVALID_STRIDE:		return "INVALID_STRIDE";
	case CAIRO_STATUS_FONT_TYPE_MISMATCH:		return "FONT_TYPE_MISMATCH";
	case CAIRO_STATUS_USER_FONT_IMMUTABLE:		return "USER_FONT_IMMUTABLE";
	case CAIRO_STATUS_USER_FONT_ERROR:		return "USER_FONT_ERROR";
	case CAIRO_STATUS_NEGATIVE_COUNT:		return "NEGATIVE_COUNT";
	case CAIRO_STATUS_INVALID_CLUSTERS:		return "INVALID_CLUSTERS";
	case CAIRO_STATUS_INVALID_SLANT:		return "INVALID_SLANT";
	case CAIRO_STATUS_INVALID_WEIGHT:		return "INVALID_WEIGHT";
	case CAIRO_STATUS_INVALID_SIZE:			return "INVALID_SIZE";
	case CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED:	return "USER_FONT_NOT_IMPLEMENTED";
	case CAIRO_STATUS_DEVICE_TYPE_MISMATCH:		return "DEVICE_TYPE_MISMATCH";
	case CAIRO_STATUS_DEVICE_ERROR:			return "DEVICE_ERROR";
	case CAIRO_STATUS_INVALID_MESH_CONSTRUCTION:	return "INVALID_MESH_CONSTRUCTION";
	case CAIRO_STATUS_DEVICE_FINISHED:		return "DEVICE_FINISHED";
	case CAIRO_STATUS_JBIG2_GLOBAL_MISSING:		return "JBIG2_GLOBAL_MISSING";
	case CAIRO_STATUS_LAST_STATUS:			return "LAST_STATUS";
    }

    static __thread char _buf[32];
    snprintf(_buf, sizeof(_buf), "%d", status);
    return _buf;
}
