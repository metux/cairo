/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2015 Enrico Weigelt, metux IT consult
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
 */

#include <inttypes.h>		/* workaround for broken radeon_drm.h */
#include <stddef.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drmMode.h>
#include <libdrm/radeon_drm.h>

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-basic-private.h"

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

#define _DEBUG(text, ...)       \
    { fprintf(stderr, "[drm/basic] " text "\n", ##__VA_ARGS__); }

#define MAX_SIZE 2048

static cairo_surface_t *
_cairo_drm_basic_surface_create_similar (void		*abstract_surface,
			      cairo_content_t		 content,
			      int			 width,
			      int			 height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width,
				       height);
}

static const cairo_surface_backend_t _cairo_drm_basic_surface_backend = {
    .type			= CAIRO_SURFACE_TYPE_DRM,
    .create_context		= _cairo_default_context_create,
    .create_similar		= _cairo_drm_basic_surface_create_similar,
    .finish			= _cairo_drm_surface_finish,
    .acquire_source_image	= _cairo_drm_surface_acquire_source_image,
    .release_source_image	= _cairo_drm_surface_release_source_image,
    .get_extents		= _cairo_drm_surface_get_extents,
    .get_font_options		= _cairo_drm_surface_get_font_options,
    .flush			= _cairo_drm_surface_flush,
    .paint			= _cairo_drm_surface_paint,
    .mask			= _cairo_drm_surface_mask,
    .stroke			= _cairo_drm_surface_stroke,
    .fill			= _cairo_drm_surface_fill,
    .show_glyphs		= _cairo_drm_surface_glyphs,
};

static cairo_surface_t *
_cairo_drm_basic_surface_create_for_bo (cairo_drm_device_t *device,
					cairo_drm_bo_t *drm_bo,
					cairo_format_t format)
{
    if (unlikely (drm_bo == NULL)) {
	_DEBUG("_cairo_drm_basic_surface_create_for_bo() NULL bo");
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    switch (format) {
	case CAIRO_FORMAT_ARGB32:
	case CAIRO_FORMAT_RGB24:
	    break;
	case CAIRO_FORMAT_INVALID:
	case CAIRO_FORMAT_A1:
	case CAIRO_FORMAT_RGB16_565:
	case CAIRO_FORMAT_RGB30:
	case CAIRO_FORMAT_A8:
	    _DEBUG("_cairo_drm_basic_surface_create_for_bo() unsupported color format: %d", format);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    cairo_drm_basic_bo_t *basic_bo = _cairo_drm_bo_cast_basic(drm_bo);
    assert(basic_bo != NULL);

    _DEBUG("_cairo_drm_basic_surface_create_for_bo() width=%d height=%d",
	basic_bo->mode_dev->mode.hdisplay,
	basic_bo->mode_dev->mode.vdisplay);

    cairo_drm_basic_surface_t *surface = calloc (1, sizeof (cairo_drm_basic_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    _cairo_surface_init (&surface->base.base,
			 &_cairo_drm_basic_surface_backend,
			 &device->base,
			 _cairo_content_from_format (format),
			 FALSE);

    _cairo_drm_surface_init (&surface->base,
			     format,
			     basic_bo->mode_dev->mode.hdisplay,
			     basic_bo->mode_dev->mode.vdisplay);

    surface->base.bo = drm_bo;
    surface->base.stride = basic_bo->mode_dev->stride;

    return &surface->base.base;
}

cairo_surface_t *
_cairo_drm_basic_surface_create (cairo_drm_device_t *device,
				 cairo_format_t format,
				 int width,
				 int height)
{
    _DEBUG("_cairo_drm_basic_surface_create() %dx%d", width, height);

    // the dimensions given here are just preferred - actual one may differ
    cairo_drm_bo_t *drm_bo = _cairo_drm_basic_bo_create (
	_cairo_drm_device_cast_basic (device),
	format,
	width,
	height);

    return _cairo_drm_basic_surface_create_for_bo(device, drm_bo, format);
}

cairo_surface_t *
_cairo_drm_basic_surface_create_for_name (cairo_drm_device_t *device,
					  unsigned int name,
					  cairo_format_t format,
					  int width,
					  int height,
					  int stride)
{
    _DEBUG("_cairo_drm_basic_surface_create_for_name()");

    // FIXME: completely untested !

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    // FIXME: should call a basic-device specific bo ?
    cairo_drm_bo_t *drm_bo = _cairo_drm_bo_create_for_name (device, name);

    return _cairo_drm_basic_surface_create_for_bo(device, drm_bo, format);
}

void
_cairo_drm_basic_device_destroy (void *data)
{
    cairo_drm_basic_device_t *device = _cairo_device_cast_basic (data);

    _cairo_drm_device_fini (&device->base);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_basic_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    cairo_drm_basic_device_t *device;
    cairo_status_t status;

    device = calloc (1, sizeof (cairo_drm_basic_device_t));
    if (device == NULL)
	return _cairo_drm_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = _cairo_drm_basic_device_init (device, fd);
    if (unlikely (status)) {
	_DEBUG("device init failed");
	free (device);
	return _cairo_drm_device_create_in_error (status);
    }

    return _cairo_drm_device_init (&device->base, fd, dev, vendor_id, chip_id, MAX_SIZE);
}

uint32_t
_cairo_drm_surface_get_crtc_id (cairo_drm_surface_t *abstract_surface)
{
    cairo_drm_basic_surface_t *surface = _cairo_drm_surface_cast_basic (abstract_surface);
    cairo_drm_basic_bo_t *bo = _cairo_drm_bo_cast_basic (surface->base.bo);
    return bo->mode_dev->crtc;
}
