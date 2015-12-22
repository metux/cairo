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

#ifndef CAIRO_DRM_BASIC_PRIVATE_H
#define CAIRO_DRM_BASIC_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"
#include "cairo-drm-private.h"
#include "cairo-freelist-private.h"

struct modeset_dev {
    struct modeset_dev *next;

    uint32_t stride;
    uint32_t size;
    uint32_t handle;

    uint32_t map_offset;

    drmModeModeInfo mode;
    uint32_t fb;
    uint32_t conn;
    uint32_t crtc;
    drmModeCrtc *saved_crtc;
};

typedef struct _cairo_drm_basic_bo {
    cairo_drm_bo_t base;
    struct modeset_dev *mode_dev;
} cairo_drm_basic_bo_t;

typedef struct _cairo_drm_basic_device {
    cairo_drm_device_t base;

    struct modeset_dev *mode_list;

    uint32_t fb_id;
} cairo_drm_basic_device_t;

typedef struct _cairo_drm_basic_surface {
    cairo_drm_surface_t base;
} cairo_drm_basic_surface_t;

static inline cairo_drm_basic_surface_t *
_cairo_abstract_surface_cast_basic (cairo_surface_t *surface)
{
    return cairo_container_of (
	_cairo_abstract_surface_cast_drm (surface),
	cairo_drm_basic_surface_t,
	base);
}

/* cast cairo_drm_device_t* to cairo_drm_basic_device_t* */
static inline cairo_drm_basic_surface_t *
_cairo_drm_surface_cast_basic (cairo_drm_surface_t *surface)
{
    return cairo_container_of (surface, cairo_drm_basic_surface_t, base);
}

/* cast cairo_device_t to cairo_drm_basic_device_t */
static inline cairo_drm_basic_surface_t *
_cairo_surface_cast_basic (cairo_surface_t *surface)
{
    return _cairo_drm_surface_cast_basic (
	_cairo_surface_cast_drm (surface));
}

static inline cairo_drm_basic_device_t *
_cairo_drm_device_cast_basic (cairo_drm_device_t *device)
{
    return cairo_container_of (device, cairo_drm_basic_device_t, base);
}

static inline const cairo_drm_basic_device_t *
_cairo_drm_device_cast_basic_const (const cairo_drm_device_t *device)
{
    return cairo_container_of (device, const cairo_drm_basic_device_t, base);
}

static inline cairo_drm_basic_device_t *
_cairo_device_cast_basic (cairo_device_t *device)
{
    return _cairo_drm_device_cast_basic (
	_cairo_device_cast_drm (device));
}

static inline const cairo_drm_basic_device_t *
_cairo_device_cast_basic_const (const cairo_device_t *device)
{
    return _cairo_drm_device_cast_basic_const (
	_cairo_device_cast_drm_const (device));
}

static inline cairo_drm_basic_bo_t *
_cairo_drm_bo_cast_basic (cairo_drm_bo_t *bo)
{
    return cairo_container_of (bo, cairo_drm_basic_bo_t, base);
}

static inline const cairo_drm_basic_bo_t *
_cairo_drm_bo_cast_basic_const (const cairo_drm_bo_t *bo)
{
    return cairo_container_of (bo, const cairo_drm_basic_bo_t, base);
}

/* get basic device from basic surface */
static inline cairo_drm_basic_device_t *
_cairo_basic_surface_get_device (const cairo_drm_basic_surface_t *surface)
{
    return _cairo_device_cast_basic (surface->base.base.device);
}

/* get basic bo from basic surface */
static inline cairo_drm_basic_bo_t *
_cairo_basic_surface_get_bo (const cairo_drm_basic_surface_t *surface)
{
    return _cairo_drm_bo_cast_basic (surface->base.bo);
}

cairo_private cairo_status_t
_cairo_drm_basic_device_init (cairo_drm_basic_device_t *device, int fd);

cairo_private void
_cairo_drm_basic_device_fini (cairo_drm_basic_device_t *device);

cairo_private void
_cairo_drm_basic_bo_read (const cairo_drm_basic_device_t *dev,
	        cairo_drm_basic_bo_t *bo,
	        unsigned long offset,
		unsigned long size,
		void *data);

cairo_private void *
_cairo_drm_basic_bo_map (const cairo_drm_device_t *dev, cairo_drm_bo_t *bo);

cairo_private cairo_surface_t *
_cairo_drm_basic_surface_create_for_name (cairo_drm_device_t *device,
					  unsigned int name,
					  cairo_format_t format,
					  int width,
					  int height,
					  int stride);

cairo_private void
_cairo_drm_basic_device_destroy (void *data);

cairo_private cairo_surface_t *
_cairo_drm_basic_surface_create (cairo_drm_device_t *device,
				 cairo_format_t format,
				 int width,
				 int height);

cairo_private cairo_drm_bo_t *
_cairo_drm_basic_bo_create (cairo_drm_basic_device_t *dev,
			    cairo_format_t format,
			    uint32_t width,
			    uint32_t height);

cairo_private cairo_surface_t *
_cairo_drm_basic_bo_get_image (const cairo_drm_device_t *device,
				cairo_drm_bo_t *bo,
				const cairo_drm_surface_t *surface);

#endif /* CAIRO_DRM_BASIC_PRIVATE_H */
