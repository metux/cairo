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
#include "cairo-drm-radeon-private.h"

#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

/* Basic stub surface for radeon chipsets */

#define MAX_SIZE 2048

static cairo_surface_t *
radeon_surface_create_similar (void			*abstract_surface,
			      cairo_content_t		 content,
			      int			 width,
			      int			 height)
{
    return cairo_image_surface_create (_cairo_format_from_content (content),
				       width, height);
}

static cairo_status_t
radeon_surface_finish (void *abstract_surface)
{
    radeon_surface_t *surface = abstract_surface;

    return _cairo_drm_surface_finish (&surface->base);
}

static void
radeon_surface_release_source_image (void *abstract_surface,
				     cairo_image_surface_t *image,
				     void *image_extra)
{
    cairo_surface_destroy (&image->base);
}

static cairo_status_t
radeon_surface_flush (void *abstract_surface,
		      unsigned flags)
{
    radeon_surface_t *surface = _cairo_surface_cast_radeon(abstract_surface);
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->base.fallback == NULL)
	return CAIRO_STATUS_SUCCESS;

    /* kill any outstanding maps */
    cairo_surface_finish (surface->base.fallback);

    status = cairo_surface_status (surface->base.fallback);
    cairo_surface_destroy (surface->base.fallback);
    surface->base.fallback = NULL;

    _cairo_drm_bo_unmap (surface->base.bo);

    return status;
}

static const cairo_surface_backend_t radeon_surface_backend = {
    .type			= CAIRO_SURFACE_TYPE_DRM,
    .create_context		= _cairo_default_context_create,
    .create_similar		= radeon_surface_create_similar,
    .finish			= radeon_surface_finish,
    .acquire_source_image	= _cairo_drm_surface_acquire_source_image,
    .release_source_image	= radeon_surface_release_source_image,
    .get_extents		= _cairo_drm_surface_get_extents,
    .get_font_options		= _cairo_drm_surface_get_font_options,
    .flush			= radeon_surface_flush,
    .paint			= _cairo_drm_dumb_surface_paint,
    .mask			= _cairo_drm_dumb_surface_mask,
    .stroke			= _cairo_drm_dumb_surface_stroke,
    .fill			= _cairo_drm_dumb_surface_fill,
    .show_glyphs		= _cairo_drm_dumb_surface_glyphs,
};

static void
radeon_surface_init (radeon_surface_t *surface,
		     cairo_drm_device_t *device,
		     cairo_format_t format,
		     int width, int height)
{
    _cairo_surface_init (&surface->base.base,
			 &radeon_surface_backend,
			 &device->base,
			 _cairo_content_from_format (format),
			FALSE);
    _cairo_drm_surface_init (&surface->base, format, width, height);
}

static cairo_surface_t *
radeon_surface_create_internal (cairo_drm_device_t *device,
				cairo_format_t format,
				int width, int height)
{
    radeon_surface_t *surface;
    cairo_status_t status;

    surface = malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, device, format, width, height);

    if (width && height) {
	surface->base.stride =
	    cairo_format_stride_for_width (surface->base.format, width);

	surface->base.bo = radeon_bo_create (_cairo_drm_device_cast_radeon (device),
					     surface->base.stride * height,
					     RADEON_GEM_DOMAIN_GTT);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static cairo_surface_t *
radeon_surface_create (cairo_drm_device_t *device,
		       cairo_format_t format,
		       int width, int height)
{
    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    return radeon_surface_create_internal (device, format, width, height);
}

static cairo_surface_t *
radeon_surface_create_for_name (cairo_drm_device_t *device,
			      unsigned int name,
			      cairo_format_t format,
			      int width, int height, int stride)
{
    radeon_surface_t *surface;
    cairo_status_t status;

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB30:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    if (stride < cairo_format_stride_for_width (format, width))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    surface = malloc (sizeof (radeon_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    radeon_surface_init (surface, device, format, width, height);

    if (width && height) {
	surface->base.stride = stride;

	surface->base.bo = radeon_bo_create_for_name (_cairo_drm_device_cast_radeon(device),
						      name);

	if (unlikely (surface->base.bo == NULL)) {
	    status = _cairo_drm_surface_finish (&surface->base);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
    }

    return &surface->base.base;
}

static void
radeon_device_destroy (void *data)
{
    radeon_device_t *device = _cairo_device_cast_radeon (data);

    _cairo_drm_device_fini (&device->base);

    free (data);
}

cairo_drm_device_t *
_cairo_drm_radeon_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    radeon_device_t *device;
    uint64_t gart_size, vram_size;
    cairo_status_t status;

    if (! radeon_info (fd, &gart_size, &vram_size))
	return NULL;

    device = malloc (sizeof (radeon_device_t));
    if (device == NULL)
	return _cairo_drm_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = radeon_device_init (device, fd);
    if (unlikely (status)) {
	free (device);
	return _cairo_drm_device_create_in_error (status);
    }

    device->base.surface.create = radeon_surface_create;
    device->base.surface.create_for_name = radeon_surface_create_for_name;
    device->base.surface.create_from_cacheable_image = NULL;
    device->base.surface.flink = _cairo_drm_surface_flink;
    device->base.surface.enable_scan_out = NULL;

    device->base.device.flush = NULL;
    device->base.device.throttle = NULL;
    device->base.device.destroy = radeon_device_destroy;

    device->vram_limit = vram_size;
    device->gart_limit = gart_size;

    return _cairo_drm_device_init (&device->base, fd, dev, vendor_id, chip_id, MAX_SIZE);
}
