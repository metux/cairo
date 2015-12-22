/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-basic-private.h"

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"

#include "cairo-image-surface-private.h"

/* Basic generic/stub surface for basic framebuffer */

#define MAX_SIZE 2048

static cairo_surface_t *
drm_basic_surface_create_similar (void			*abstract_surface,
			      cairo_content_t	 content,
			      int			 width,
			      int			 height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

cairo_status_t
drm_basic_surface_finish (void *abstract_surface)
{
    basic_surface_t *surface = cairo_abstract_surface_cast_basic(abstract_surface);

    drm_basic_bo_in_flight_add (to_basic_device (surface->drm.base.device),
			    to_basic_bo (surface->drm.bo));
    return _cairo_drm_surface_finish (&surface->drm);
}

cairo_status_t
drm_basic_surface_flush (void *abstract_surface, unsigned flags)
{
    basic_surface_t *surface = cairo_abstract_surface_cast_basic(abstract_surface);
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->drm.fallback == NULL)
	return CAIRO_STATUS_SUCCESS;

    /* kill any outstanding maps */
    cairo_surface_finish (surface->drm.fallback);

    status = cairo_surface_status (surface->drm.fallback);
    cairo_surface_destroy (surface->drm.fallback);
    surface->drm.fallback = NULL;

    return status;
}

static const cairo_surface_backend_t basic_surface_backend = {
    .type			= CAIRO_SURFACE_TYPE_DRM,
    .create_context		= _cairo_default_context_create,
    .create_similar		= drm_basic_surface_create_similar,
    .finish			= drm_basic_surface_finish,
    .acquire_source_image	= _cairo_drm_surface_acquire_source_image,
    .release_source_image	= _cairo_drm_surface_release_source_image,
    .get_extents		= _cairo_drm_surface_get_extents,
    .get_font_options		= _cairo_drm_surface_get_font_options,
    .flush			= drm_basic_surface_flush,
    .paint			= _cairo_drm_dumb_surface_paint,
    .mask			= _cairo_drm_dumb_surface_mask,
    .stroke			= _cairo_drm_dumb_surface_stroke,
    .fill			= _cairo_drm_dumb_surface_fill,
    .show_glyphs		= _cairo_drm_dumb_surface_glyphs,
};

void
drm_basic_surface_init (basic_surface_t *surface,
		    const cairo_surface_backend_t *backend,
		    cairo_drm_device_t *device,
		    cairo_format_t format,
		    int width, int height)
{
    _cairo_surface_init (&surface->drm.base,
			 backend,
			 &device->base,
			 _cairo_content_from_format (format),
			 FALSE);

    _cairo_drm_surface_init (&surface->drm, format, width, height);

    surface->snapshot_cache_entry.hash = 0;
}

static cairo_surface_t *
basic_surface_create (cairo_drm_device_t *device,
		      cairo_format_t format,
		      int width, int height)
{
    basic_surface_t *surface;
    cairo_status_t status;

    surface = malloc (sizeof (basic_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    drm_basic_surface_init (surface, &basic_surface_backend, device,
			format, width, height);

    if (width && height) {
	/* Vol I, p134: size restrictions for textures */
	width  = (width  + 3) & -4;
	height = (height + 1) & -2;
	surface->drm.stride =
	    cairo_format_stride_for_width (surface->drm.format, width);
	surface->drm.bo = &basic_bo_create (to_basic_device (&device->base),
					    surface->drm.stride * height,
					    surface->drm.stride * height,
					    TRUE, I915_TILING_NONE, surface->drm.stride)->base;
	if (surface->drm.bo == NULL) {
	    status = _cairo_drm_surface_finish (&surface->drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->drm.base;
}

static cairo_surface_t *
basic_surface_create_for_name (cairo_drm_device_t *device,
			       unsigned int name,
			       cairo_format_t format,
			       int width, int height, int stride)
{
    basic_surface_t *surface;
    cairo_status_t status;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_A8:
	break;
    }

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    surface = malloc (sizeof (basic_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    drm_basic_surface_init (surface, &basic_surface_backend,
			device, format, width, height);

    if (width && height) {
	surface->drm.stride = stride;

	surface->drm.bo = &basic_bo_create_for_name (to_basic_device (&device->base),
						      name)->base;
	if (unlikely (surface->drm.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error
						   (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->drm.base;
}

static cairo_status_t
basic_surface_enable_scan_out (void *abstract_surface)
{
    basic_surface_t *surface = cairo_abstract_surface_cast_basic(abstract_surface);

    if (unlikely (surface->drm.bo == NULL))
	return _cairo_error (CAIRO_STATUS_INVALID_SIZE);

    to_basic_bo (surface->drm.bo)->tiling = I915_TILING_X;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
basic_device_throttle (cairo_drm_device_t *device)
{
//    basic_throttle (to_basic_device (&device->base));
    return CAIRO_STATUS_SUCCESS;
}

static void
basic_device_destroy (void *data)
{
    basic_device_t *device = data;

    _cairo_drm_basic_device_fini (device);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_basic_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    basic_device_t *device;
    cairo_status_t status;

    device = malloc (sizeof (basic_device_t));
    if (unlikely (device == NULL))
	return (cairo_drm_device_t *) _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = basic_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return (cairo_drm_device_t *) _cairo_device_create_in_error (status);
    }

    device->base.surface.create = basic_surface_create;
    device->base.surface.create_for_name = basic_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = basic_surface_enable_scan_out;

    device->base.surface.map_to_image = _cairo_drm_surface_map_to_image;

    device->base.device.flush = NULL;
    device->base.device.throttle = basic_device_throttle;
    device->base.device.destroy = basic_device_destroy;

    fprintf(stderr, "[drm/basic] initialzing drm device\n");

    return _cairo_drm_device_init (&device->base,
				   fd, dev,
				   vendor_id, chip_id,
				   MAX_SIZE);
}
