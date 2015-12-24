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
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Chris Wilson.
 *
 * Contributors(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#ifndef CAIRO_DRM_PRIVATE_H
#define CAIRO_DRM_PRIVATE_H

#include "cairo-drm.h"

#include "cairo-device-private.h"
#include "cairo-reference-count-private.h"
#include "cairo-surface-private.h"
#include "cairo-freelist-type-private.h"
#include "cairo-freelist-private.h"

#include <sys/types.h> /* dev_t */

typedef struct _cairo_drm_device cairo_drm_device_t;
typedef struct _cairo_drm_bo cairo_drm_bo_t;
typedef struct _cairo_drm_surface cairo_drm_surface_t;

typedef cairo_drm_device_t *
(*cairo_drm_device_create_func_t) (int fd,
				   dev_t dev,
				   int vendor_id,
				   int chip_id);

typedef cairo_int_status_t
(*cairo_drm_device_flush_func_t) (cairo_drm_device_t *device);

typedef cairo_int_status_t
(*cairo_drm_device_throttle_func_t) (cairo_drm_device_t *device);

typedef void
(*cairo_drm_device_destroy_func_t) (void *data);

typedef cairo_surface_t *
(*cairo_drm_surface_create_func_t) (cairo_drm_device_t *device,
				    cairo_format_t format,
				    int width, int height);

typedef cairo_surface_t *
(*cairo_drm_surface_create_for_name_func_t) (cairo_drm_device_t *device,
				             unsigned int name,
					     cairo_format_t format,
					     int width, int height, int stride);

typedef cairo_surface_t *
(*cairo_drm_surface_create_from_cacheable_image_func_t)
    (cairo_drm_device_t *device, cairo_surface_t *image);

typedef cairo_int_status_t
(*cairo_drm_surface_flink_func_t) (void *surface);

typedef cairo_status_t
(*cairo_drm_surface_enable_scan_out_func_t) (void *surface);

typedef cairo_surface_t *
(*cairo_drm_surface_map_to_image_func_t) (void *surface);

typedef struct _cairo_drm_bo_backend {
    void (*release) (cairo_drm_device_t *device, cairo_drm_bo_t *bo);
    void *(*map)    (const cairo_drm_device_t *device, cairo_drm_bo_t *bo);
    cairo_surface_t *(*get_image) (const cairo_drm_device_t *device, cairo_drm_bo_t *bo, const cairo_drm_surface_t *surface);
} cairo_drm_bo_backend_t;

typedef struct _cairo_drm_device_backend {
    cairo_drm_device_flush_func_t flush;
    cairo_drm_device_throttle_func_t throttle;
    cairo_drm_device_destroy_func_t destroy;
} cairo_drm_device_backend_t;

typedef struct _cairo_drm_surface_backend {
    cairo_drm_surface_create_func_t create;
    cairo_drm_surface_create_for_name_func_t create_for_name;
    cairo_drm_surface_create_from_cacheable_image_func_t create_from_cacheable_image;
    cairo_drm_surface_flink_func_t flink;
    cairo_drm_surface_enable_scan_out_func_t enable_scan_out;
    cairo_drm_surface_map_to_image_func_t map_to_image;
} cairo_drm_surface_backend_t;

struct _cairo_drm_bo {
    cairo_reference_count_t ref_count;
    uint32_t name;
    uint32_t handle;
    uint32_t size;
    void*    mapped;
};

struct _cairo_drm_device {
    cairo_device_t base;

    int vendor_id;
    int chip_id;
    dev_t id;
    int fd;

    int max_surface_size;

    cairo_drm_bo_backend_t bo;
    cairo_drm_surface_backend_t surface;
    cairo_drm_device_backend_t device;
    cairo_freepool_t bo_pool;

    cairo_drm_device_t *next, *prev;
};

struct _cairo_drm_surface {
    cairo_surface_t base;

    cairo_drm_bo_t *bo;

    cairo_format_t format;
    int width, height, stride;

    cairo_surface_t *fallback;
    uint32_t map_count;
};

/* cast from abstract void* pointer */
static inline cairo_drm_device_t*
_cairo_abstract_device_cast_drm(cairo_device_t* dev)
{
    return cairo_container_of(
	dev,
	cairo_drm_device_t,
	base);
}

static inline cairo_drm_surface_t*
_cairo_abstract_surface_cast_drm(cairo_surface_t* surface)
{
    return cairo_container_of(surface, cairo_drm_surface_t, base);
}

static inline cairo_drm_surface_t*
_cairo_surface_cast_drm (cairo_surface_t* surface)
{
    return cairo_container_of (surface, cairo_drm_surface_t, base);
}

static inline const cairo_drm_surface_t*
_cairo_surface_cast_drm_const (const cairo_surface_t* surface)
{
    return cairo_container_of (surface, const cairo_drm_surface_t, base);
}

static inline cairo_drm_device_t*
_cairo_device_cast_drm(cairo_device_t *device)
{
    return cairo_container_of (device, cairo_drm_device_t, base);
}

static inline const cairo_drm_device_t*
_cairo_device_cast_drm_const(const cairo_device_t *device)
{
    return cairo_container_of (device, const cairo_drm_device_t, base);
}

cairo_private void
_cairo_drm_bo_unmap (cairo_drm_bo_t *bo);

static inline cairo_drm_bo_t *
cairo_drm_bo_reference (cairo_drm_bo_t *bo)
{
    _cairo_reference_count_inc (&bo->ref_count);
    return bo;
}

static inline cairo_drm_device_t *
_cairo_drm_surface_get_device (cairo_drm_surface_t *surface)
{
    return _cairo_device_cast_drm(surface->base.device);
}

static inline cairo_always_inline cairo_drm_bo_t *
_cairo_drm_bo_from_pool (cairo_drm_device_t *device)
{
    cairo_drm_bo_t *bo = _cairo_freepool_alloc (&device->bo_pool);

    if (unlikely(bo == NULL))
	return NULL;
    bo->mapped = NULL;

    return bo;
}

static cairo_always_inline void
cairo_drm_bo_destroy (cairo_device_t *abstract_device,
		      cairo_drm_bo_t *bo)
{
    _cairo_drm_bo_unmap (bo);

    if (_cairo_reference_count_dec_and_test (&bo->ref_count)) {
	cairo_drm_device_t *device = (cairo_drm_device_t *) abstract_device;
	device->bo.release (device, bo);
    }
}

cairo_private cairo_status_t
_cairo_drm_bo_open_for_name (const cairo_drm_device_t *dev,
			     cairo_drm_bo_t *bo,
			     uint32_t name);

cairo_private cairo_status_t
_cairo_drm_bo_flink (const cairo_drm_device_t *dev,
		     cairo_drm_bo_t *bo);

cairo_private void
_cairo_drm_bo_close (const cairo_drm_device_t *dev,
		     cairo_drm_bo_t *bo);

cairo_private void
_cairo_drm_surface_init (cairo_drm_surface_t *surface,
			 cairo_format_t format,
			 int width, int height);

cairo_private cairo_status_t
_cairo_drm_surface_finish (void *surface);

cairo_private void
_cairo_drm_surface_get_font_options (void                  *abstract_surface,
				     cairo_font_options_t  *options);

cairo_private cairo_bool_t
_cairo_drm_surface_get_extents (void *abstract_surface,
			        cairo_rectangle_int_t *rectangle);

cairo_private cairo_int_status_t
_cairo_drm_surface_flink (void *abstract_surface);

static inline cairo_drm_device_t *
_cairo_drm_device_create_in_error (cairo_status_t status)
{
    return (cairo_drm_device_t *) _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);
}

cairo_private cairo_drm_device_t *
_cairo_drm_device_init (cairo_drm_device_t *device,
			int fd,
			dev_t devid,
			int vendor_id,
			int chip_id,
			int max_surface_size);

cairo_private void
_cairo_drm_device_fini (cairo_drm_device_t *device);

cairo_private cairo_surface_t *
_cairo_drm_surface_map_to_image (void *abstract_surface);

/* default / dumb framebuffer backend ops */

cairo_private cairo_int_status_t
_cairo_drm_dumb_surface_paint (void			*abstract_surface,
			       cairo_operator_t		op,
			       const cairo_pattern_t	*source,
			       const cairo_clip_t	*clip);

cairo_private cairo_int_status_t
_cairo_drm_dumb_surface_mask (void			*abstract_surface,
			      cairo_operator_t		op,
			      const cairo_pattern_t	*source,
			      const cairo_pattern_t	*mask,
			      const cairo_clip_t	*clip);

cairo_private cairo_int_status_t
_cairo_drm_dumb_surface_stroke (void				*abstract_surface,
				cairo_operator_t		op,
				const cairo_pattern_t		*source,
				const cairo_path_fixed_t	*path,
				const cairo_stroke_style_t	*stroke_style,
				const cairo_matrix_t		*ctm,
				const cairo_matrix_t		*ctm_inverse,
				double				tolerance,
				cairo_antialias_t		antialias,
				const cairo_clip_t		*clip);

cairo_private cairo_int_status_t
_cairo_drm_dumb_surface_fill (void			*abstract_surface,
			      cairo_operator_t		op,
			      const cairo_pattern_t	*source,
			      const cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		fill_rule,
			      double			tolerance,
			      cairo_antialias_t		antialias,
			      const cairo_clip_t	*clip);

cairo_private cairo_int_status_t
_cairo_drm_dumb_surface_glyphs (void			*abstract_surface,
				cairo_operator_t	op,
				const cairo_pattern_t	*source,
				cairo_glyph_t		*glyphs,
				int			num_glyphs,
				cairo_scaled_font_t	*scaled_font,
				const cairo_clip_t	*clip);

cairo_private cairo_status_t
_cairo_drm_surface_acquire_source_image (void *abstract_surface,
					 cairo_image_surface_t **image_out,
					 void **image_extra);

/* h/w specific backends */

cairo_private cairo_drm_device_t *
_cairo_drm_intel_device_create (int fd, dev_t dev, int vendor_id, int chip_id);

cairo_private cairo_drm_device_t *
_cairo_drm_i915_device_create (int fd, dev_t dev, int vendor_id, int chip_id);

cairo_private cairo_drm_device_t *
_cairo_drm_i965_device_create (int fd, dev_t dev, int vendor_id, int chip_id);

cairo_private cairo_drm_device_t *
_cairo_drm_radeon_device_create (int fd, dev_t dev, int vendor_id, int chip_id);

#if CAIRO_HAS_GALLIUM_SURFACE
cairo_private cairo_drm_device_t *
_cairo_drm_gallium_device_create (int fd, dev_t dev);
#endif

slim_hidden_proto (cairo_drm_device_default);
slim_hidden_proto (cairo_drm_device_get);
slim_hidden_proto (cairo_drm_device_get_for_fd);

slim_hidden_proto (cairo_drm_surface_create_for_name);

cairo_private cairo_bool_t
_cairo_drm_size_is_valid (cairo_device_t *abstract_device,
			  int width, int height);

#endif /* CAIRO_DRM_PRIVATE_H */
