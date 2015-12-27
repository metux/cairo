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

#ifndef CAIRO_DRM_RADEON_PRIVATE_H
#define CAIRO_DRM_RADEON_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"
#include "cairo-drm-private.h"
#include "cairo-freelist-private.h"

typedef struct _radeon_bo {
    cairo_drm_bo_t base;

} radeon_bo_t;

typedef struct _radeon_device {
    cairo_drm_device_t base;

    uint64_t vram_limit;
    uint64_t gart_limit;
} radeon_device_t;

typedef struct _radeon_surface {
    cairo_drm_surface_t base;
} radeon_surface_t;

/* cast void* to radeon_device_t* */
static inline radeon_surface_t *
_cairo_abstract_surface_cast_radeon (cairo_surface_t *surface)
{
    return cairo_container_of (
	_cairo_abstract_surface_cast_drm (surface),
	radeon_surface_t,
	base);
}

/* cast cairo_drm_device_t* to radeon_device_t* */
static inline radeon_surface_t *
_cairo_drm_surface_cast_radeon (cairo_drm_surface_t *surface)
{
    return cairo_container_of (surface, radeon_surface_t, base);
}

/* cast const cairo_drm_device_t* to const radeon_device_t* */
static inline const radeon_surface_t *
_cairo_drm_surface_cast_radeon_const (const cairo_drm_surface_t *surface)
{
    return cairo_container_of (surface, const radeon_surface_t, base);
}

/* cast cairo_device_t to radeon_device_t */
static inline radeon_surface_t *
_cairo_surface_cast_radeon (cairo_surface_t *surface)
{
    return _cairo_drm_surface_cast_radeon (
	_cairo_surface_cast_drm (surface));
}

/* cast const cairo_device_t* to const radeon_device_t* */
static inline const radeon_surface_t *
_cairo_surface_cast_radeon_cast (const cairo_surface_t *surface)
{
    return _cairo_drm_surface_cast_radeon_const (
	_cairo_surface_cast_drm_const (surface));
}

static inline radeon_device_t *
_cairo_drm_device_cast_radeon (cairo_drm_device_t *device)
{
    return cairo_container_of (device, radeon_device_t, base);
}

static inline const radeon_device_t *
_cairo_drm_device_cast_radeon_const (const cairo_drm_device_t *device)
{
    return cairo_container_of (device, const radeon_device_t, base);
}

static inline radeon_device_t *
_cairo_device_cast_radeon (cairo_device_t *device)
{
    return _cairo_drm_device_cast_radeon (
	_cairo_device_cast_drm (device));
}

static inline const radeon_device_t *
_cairo_device_cast_radeon_const (const cairo_device_t *device)
{
    return _cairo_drm_device_cast_radeon_const (
	_cairo_device_cast_drm_const (device));
}

static inline radeon_bo_t *
_cairo_drm_bo_cast_radeon (cairo_drm_bo_t *bo)
{
    return cairo_container_of (bo, radeon_bo_t, base);
}

static inline const radeon_bo_t *
_cairo_drm_bo_cast_radeon_const (const cairo_drm_bo_t *bo)
{
    return cairo_container_of (bo, const radeon_bo_t, base);
}

/* get radeon device from radeon surface */
static inline radeon_device_t *
_cairo_radeon_surface_get_device (const radeon_surface_t *surface)
{
    return _cairo_device_cast_radeon (surface->base.base.device);
}

/* get radeon bo from radeon surface */
static inline radeon_bo_t *
_cairo_radeon_surface_get_bo (const radeon_surface_t *surface)
{
    return _cairo_drm_bo_cast_radeon (surface->base.bo);
}

cairo_private cairo_status_t
radeon_device_init (radeon_device_t *device, int fd);

cairo_private void
radeon_device_fini (radeon_device_t *device);

cairo_private cairo_bool_t
radeon_info (int fd,
             uint64_t *gart_size,
	     uint64_t *vram_size);

cairo_private void
radeon_bo_write (const radeon_device_t *dev,
	         radeon_bo_t *bo,
		 unsigned long offset,
		 unsigned long size,
		 const void *data);

cairo_private void
radeon_bo_read (const radeon_device_t *dev,
	        radeon_bo_t *bo,
	        unsigned long offset,
		unsigned long size,
		void *data);

cairo_private void
radeon_bo_wait (const radeon_device_t *dev, radeon_bo_t *bo);

cairo_private void *
_cairo_drm_radeon_bo_map (const cairo_drm_device_t *dev, cairo_drm_bo_t *bo);

static inline void *
radeon_bo_map (const radeon_device_t *dev, radeon_bo_t *bo)
{
    return dev->base.bo.map (&(dev->base), &(bo->base));
}

cairo_private cairo_drm_bo_t *
radeon_bo_create (radeon_device_t *dev,
		  uint32_t size,
		  uint32_t initial_domain);

cairo_private cairo_surface_t *
_cairo_drm_radeon_bo_get_image (const cairo_drm_device_t *device,
				cairo_drm_bo_t *bo,
				const cairo_drm_surface_t *surface);

#endif /* CAIRO_DRM_RADEON_PRIVATE_H */
