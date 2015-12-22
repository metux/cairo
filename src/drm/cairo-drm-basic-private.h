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

#ifndef CAIRO_DRM_BASIC_PRIVATE_H
#define CAIRO_DRM_BASIC_PRIVATE_H

#include "cairoint.h"
#include "cairo-cache-private.h"
#include "cairo-compiler-private.h"
#include "cairo-drm-private.h"
#include "cairo-freelist-private.h"
#include "cairo-list-private.h"
#include "cairo-mutex-private.h"
#include "cairo-rtree-private.h"
#include "cairo-types-private.h"
#include "cairo-pattern-private.h"

#include "cairo-drm-intel-ioctl-private.h"

#define BASIC_TILING_DEFAULT I915_TILING_Y

#define BASIC_BO_CACHE_BUCKETS 12 /* cache surfaces up to 16 MiB */

typedef struct _basic_bo {
    cairo_drm_bo_t base;

    cairo_list_t link;
    cairo_list_t cache_list;

    uint32_t offset;
    uint32_t batch_read_domains;
    uint32_t batch_write_domain;

    uint32_t opaque0;
    uint32_t opaque1;

    uint32_t full_size;
    uint16_t stride;
    uint16_t _stride;
    uint32_t tiling :4;
    uint32_t _tiling :4;
    uint32_t purgeable :1;
    uint32_t busy :1;
    uint32_t cpu :1;

    struct drm_i915_gem_exec_object2 *exec;
} basic_bo_t;

#define BASIC_BATCH_SIZE (64*1024)
#define BASIC_VERTEX_BUFFER_SIZE (512*1024)
#define BASIC_MAX_RELOCS 2048

static inline void
basic_bo_mark_purgeable (basic_bo_t *bo)
{
    if (bo->base.name == 0)
	bo->purgeable = 1;
}

typedef struct _basic_vertex_buffer basic_vertex_buffer_t;

typedef void (*basic_vertex_buffer_new_func_t) (basic_vertex_buffer_t *vertex_buffer);
typedef void (*basic_vertex_buffer_start_rectangles_func_t) (basic_vertex_buffer_t *vertex_buffer,
							     uint32_t floats_per_vertex);
typedef void (*basic_vertex_buffer_flush_func_t) (basic_vertex_buffer_t *vertex_buffer);
typedef void (*basic_vertex_buffer_finish_func_t) (basic_vertex_buffer_t *vertex_buffer);

struct _basic_vertex_buffer {
    uint32_t vbo_batch; /* reloc position in batch, 0 -> not yet allocated */
    uint32_t vbo_offset;
    uint32_t vbo_used;

    uint32_t vertex_index;
    uint32_t vertex_count;

    uint32_t floats_per_vertex;
    uint32_t rectangle_size;

    basic_bo_t *last_vbo;
    uint32_t last_vbo_offset;
    uint32_t last_vbo_space;

    basic_vertex_buffer_new_func_t new;
    basic_vertex_buffer_start_rectangles_func_t start_rectangles;
    basic_vertex_buffer_flush_func_t flush;
    basic_vertex_buffer_finish_func_t finish;

    uint32_t base[BASIC_VERTEX_BUFFER_SIZE / sizeof (uint32_t)];
};

typedef struct _basic_batch basic_batch_t;

typedef void (*basic_batch_commit_func_t) (basic_batch_t *batch);
typedef void (*basic_batch_reset_func_t) (basic_batch_t *batch);

struct _basic_batch {
    size_t gtt_size;
    size_t gtt_avail_size;

    basic_batch_commit_func_t commit;
    basic_batch_reset_func_t reset;

    uint16_t exec_count;
    uint16_t reloc_count;
    uint16_t used;
    uint16_t header;

    basic_bo_t *target_bo[BASIC_MAX_RELOCS];
    struct drm_i915_gem_exec_object2 exec[BASIC_MAX_RELOCS];
    struct drm_i915_gem_relocation_entry reloc[BASIC_MAX_RELOCS];

    uint32_t base[BASIC_BATCH_SIZE / sizeof (uint32_t)];

    basic_vertex_buffer_t vertex_buffer;
};

typedef struct _basic_buffer {
    basic_bo_t *bo;
    uint32_t offset;
    cairo_format_t format;
    uint32_t map0, map1;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} basic_buffer_t;

typedef struct _basic_buffer_cache {
    int ref_count;
    basic_buffer_t buffer;
    cairo_rtree_t rtree;
    cairo_list_t link;
} basic_buffer_cache_t;

typedef struct _basic_glyph {
    cairo_rtree_node_t node;
    cairo_scaled_glyph_private_t base;
    cairo_scaled_glyph_t *glyph;
    basic_buffer_cache_t *cache;
    void **owner;
    float texcoord[3];
    int width, height;
} basic_glyph_t;

typedef struct _basic_scaled_font {
    cairo_scaled_font_private_t base;
} basic_scaled_font_t;

typedef struct _basic_gradient_cache {
    cairo_pattern_union_t pattern;
    basic_buffer_t buffer;
} basic_gradient_cache_t;
#define GRADIENT_CACHE_SIZE 16

typedef struct _basic_surface {
    cairo_drm_surface_t drm;

    cairo_cache_entry_t snapshot_cache_entry;
} basic_surface_t;

static inline basic_surface_t*
cairo_abstract_surface_cast_basic(cairo_surface_t* surface)
{
    return cairo_container_of(
	_cairo_abstract_surface_cast_drm(surface),
	basic_surface_t,
	drm);
}

typedef void (*basic_reset_context_func_t) (void *device);

typedef struct _basic_device {
    cairo_drm_device_t base;

    size_t gtt_max_size;
    size_t gtt_avail_size;

    cairo_freepool_t bo_pool;
    cairo_list_t bo_in_flight;

    cairo_mutex_t mutex;
    basic_batch_t batch;

    basic_buffer_cache_t glyph_cache[2];
    cairo_list_t fonts;

    struct {
	basic_gradient_cache_t cache[GRADIENT_CACHE_SIZE];
	unsigned int size;
    } gradient_cache;

    cairo_cache_t snapshot_cache;
    size_t snapshot_cache_max_size;

    basic_reset_context_func_t reset_context;

    cairo_status_t (*flush) (struct _basic_device *);
} basic_device_t;

static inline basic_device_t *
to_basic_device (cairo_device_t *base)
{
    return (basic_device_t *) base;
}

static inline const basic_device_t *
to_basic_device_const (const cairo_device_t *base)
{
    return (const basic_device_t *) base;
}

static inline basic_bo_t *
to_basic_bo (cairo_drm_bo_t *base)
{
    return (basic_bo_t *) base;
}

static inline basic_bo_t *
basic_bo_reference (basic_bo_t *bo)
{
    return to_basic_bo (cairo_drm_bo_reference (&bo->base));
}

cairo_private cairo_bool_t
basic_bo_madvise (basic_device_t *device, basic_bo_t *bo, int madv);

static cairo_always_inline void
basic_bo_destroy (basic_device_t *device, basic_bo_t *bo)
{
    cairo_drm_bo_destroy (&device->base.base, &bo->base);
}

static inline void
drm_basic_bo_in_flight_add (basic_device_t *device,
			basic_bo_t *bo)
{
    if (bo->base.name == 0 && bo->exec != NULL && cairo_list_is_empty (&bo->cache_list))
	cairo_list_add (&bo->cache_list, &device->bo_in_flight);
}

cairo_private int
basic_get (int fd, int param);

cairo_private cairo_bool_t
basic_info (int fd, uint64_t *gtt_size);

cairo_private cairo_status_t
basic_device_init (basic_device_t *device, int fd);

cairo_private void
_cairo_drm_basic_device_fini (basic_device_t *dev);

cairo_private basic_bo_t *
basic_bo_create (basic_device_t *dev,
	         uint32_t max_size,
	         uint32_t real_size,
	         cairo_bool_t gpu_target,
		 uint32_t tiling,
		 uint32_t stride);

cairo_private basic_bo_t *
basic_bo_create_for_name (basic_device_t *dev, uint32_t name);

cairo_private void
basic_bo_set_tiling (const basic_device_t *dev,
	             basic_bo_t *bo);

cairo_private cairo_bool_t
basic_bo_is_inactive (const basic_device_t *device,
		      basic_bo_t *bo);

cairo_private cairo_bool_t
basic_bo_wait (const basic_device_t *device, const basic_bo_t *bo);

cairo_private void
_cairo_drm_basic_bo_write (const cairo_drm_device_t *dev,
		cairo_drm_bo_t *bo,
		unsigned long offset,
		unsigned long size,
		const void *data);

cairo_private void
_cairo_drm_basic_bo_read (const cairo_drm_device_t *dev,
	       cairo_drm_bo_t *bo,
	       unsigned long offset,
	       unsigned long size,
	       void *data);

cairo_private void *
_cairo_drm_basic_bo_map (const cairo_drm_device_t *dev, cairo_drm_bo_t *bo);

static inline void *
basic_bo_map (const basic_device_t *dev, basic_bo_t *bo)
{
    return dev->base.bo_map (&(dev->base), &(bo->base));
}

cairo_private cairo_status_t
basic_bo_init (const basic_device_t *dev,
	       basic_bo_t *bo,
	       uint32_t size,
	       uint32_t initial_domain);

cairo_private cairo_status_t
basic_bo_init_for_name (const basic_device_t *dev,
			basic_bo_t *bo,
			uint32_t size,
			uint32_t name);

cairo_private cairo_surface_t *
basic_bo_get_image (const basic_device_t *device,
		    basic_bo_t *bo,
		    const cairo_drm_surface_t *surface);

cairo_private cairo_status_t
basic_bo_put_image (basic_device_t *dev,
		    basic_bo_t *bo,
		    cairo_image_surface_t *src,
		    int src_x, int src_y,
		    int width, int height,
		    int dst_x, int dst_y);

cairo_private void
drm_basic_surface_init (basic_surface_t *surface,
		    const cairo_surface_backend_t *backend,
		    cairo_drm_device_t *device,
		    cairo_format_t format,
		    int width, int height);

cairo_private cairo_status_t
basic_buffer_cache_init (basic_buffer_cache_t *cache,
		        basic_device_t *device,
			cairo_format_t format,
			int width, int height);

cairo_private cairo_status_t
basic_gradient_render (basic_device_t *device,
		       const cairo_gradient_pattern_t *pattern,
		       basic_buffer_t *buffer);

cairo_private cairo_int_status_t
basic_get_glyph (basic_device_t *device,
		 cairo_scaled_font_t *scaled_font,
		 cairo_scaled_glyph_t *scaled_glyph);

cairo_private void
basic_scaled_glyph_fini (cairo_scaled_glyph_private_t *scaled_glyph_private,
			 cairo_scaled_glyph_t         *scaled_glyph,
			 cairo_scaled_font_t          *scaled_font);

cairo_private void
basic_scaled_font_fini (cairo_scaled_font_t *scaled_font);

cairo_private void
basic_glyph_cache_unpin (basic_device_t *device);

static inline basic_glyph_t *
basic_glyph_pin (basic_glyph_t *glyph)
{
    cairo_rtree_node_t *node = &glyph->node;
    if (unlikely (node->pinned == 0))
	return _cairo_rtree_pin (&glyph->cache->rtree, node);
    return glyph;
}

cairo_private cairo_status_t
basic_snapshot_cache_insert (basic_device_t *device,
			     basic_surface_t *surface);

cairo_private void
basic_surface_detach_snapshot (cairo_surface_t *abstract_surface);

cairo_private void
basic_snapshot_cache_thaw (basic_device_t *device);

cairo_private void
basic_throttle (basic_device_t *device);

cairo_private cairo_status_t
drm_basic_surface_acquire_source_image (void *abstract_surface,
				    cairo_image_surface_t **image_out,
				    void **image_extra);

cairo_private void
drm_basic_surface_release_source_image (void *abstract_surface,
				    cairo_image_surface_t *image,
				    void *image_extra);
cairo_private cairo_surface_t *
drm_basic_surface_map_to_image (void *abstract_surface);

cairo_private cairo_status_t
drm_basic_surface_flush (void *abstract_surface,
		     unsigned flags);

cairo_private cairo_status_t
drm_basic_surface_finish (void *abstract_surface);

cairo_private void
basic_dump_batchbuffer (const void *batch,
			uint32_t length,
			int devid);

static inline uint32_t cairo_const
MS3_tiling (uint32_t tiling)
{
    switch (tiling) {
    default:
    case I915_TILING_NONE: return 0;
    case I915_TILING_X: return MS3_TILED_SURFACE;
    case I915_TILING_Y: return MS3_TILED_SURFACE | MS3_TILE_WALK;
    }
}

static inline float cairo_const
texcoord_2d_16 (double x, double y)
{
    union {
	uint32_t ui;
	float f;
    } u;
    u.ui = (_cairo_half_from_float (y) << 16) | _cairo_half_from_float (x);
    return u.f;
}

#define PCI_CHIP_I810			0x7121
#define PCI_CHIP_I810_DC100		0x7123
#define PCI_CHIP_I810_E			0x7125
#define PCI_CHIP_I815			0x1132

#define PCI_CHIP_I830_M			0x3577
#define PCI_CHIP_845_G			0x2562
#define PCI_CHIP_I855_GM		0x3582
#define PCI_CHIP_I865_G			0x2572

#define PCI_CHIP_I915_G			0x2582
#define PCI_CHIP_E7221_G		0x258A
#define PCI_CHIP_I915_GM		0x2592
#define PCI_CHIP_I945_G			0x2772
#define PCI_CHIP_I945_GM		0x27A2
#define PCI_CHIP_I945_GME		0x27AE

#define PCI_CHIP_Q35_G			0x29B2
#define PCI_CHIP_G33_G			0x29C2
#define PCI_CHIP_Q33_G			0x29D2

#define PCI_CHIP_IGD_GM			0xA011
#define PCI_CHIP_IGD_G			0xA001

#define IS_IGDGM(devid)	(devid == PCI_CHIP_IGD_GM)
#define IS_IGDG(devid)	(devid == PCI_CHIP_IGD_G)
#define IS_IGD(devid) (IS_IGDG(devid) || IS_IGDGM(devid))

#define PCI_CHIP_I965_G			0x29A2
#define PCI_CHIP_I965_Q			0x2992
#define PCI_CHIP_I965_G_1		0x2982
#define PCI_CHIP_I946_GZ		0x2972
#define PCI_CHIP_I965_GM                0x2A02
#define PCI_CHIP_I965_GME               0x2A12

#define PCI_CHIP_GM45_GM                0x2A42

#define PCI_CHIP_IGD_E_G                0x2E02
#define PCI_CHIP_Q45_G                  0x2E12
#define PCI_CHIP_G45_G                  0x2E22
#define PCI_CHIP_G41_G                  0x2E32

#define PCI_CHIP_ILD_G                  0x0042
#define PCI_CHIP_ILM_G                  0x0046

#define IS_MOBILE(devid)	(devid == PCI_CHIP_I855_GM || \
				 devid == PCI_CHIP_I915_GM || \
				 devid == PCI_CHIP_I945_GM || \
				 devid == PCI_CHIP_I945_GME || \
				 devid == PCI_CHIP_I965_GM || \
				 devid == PCI_CHIP_I965_GME || \
				 devid == PCI_CHIP_GM45_GM || IS_IGD(devid))

#define IS_G45(devid)           (devid == PCI_CHIP_IGD_E_G || \
                                 devid == PCI_CHIP_Q45_G || \
                                 devid == PCI_CHIP_G45_G || \
                                 devid == PCI_CHIP_G41_G)
#define IS_GM45(devid)          (devid == PCI_CHIP_GM45_GM)
#define IS_G4X(devid)		(IS_G45(devid) || IS_GM45(devid))

#define IS_ILD(devid)           (devid == PCI_CHIP_ILD_G)
#define IS_ILM(devid)           (devid == PCI_CHIP_ILM_G)
#define IS_IRONLAKE(devid)      (IS_ILD(devid) || IS_ILM(devid))

#define IS_915(devid)		(devid == PCI_CHIP_I915_G || \
				 devid == PCI_CHIP_E7221_G || \
				 devid == PCI_CHIP_I915_GM)

#define IS_945(devid)		(devid == PCI_CHIP_I945_G || \
				 devid == PCI_CHIP_I945_GM || \
				 devid == PCI_CHIP_I945_GME || \
				 devid == PCI_CHIP_G33_G || \
				 devid == PCI_CHIP_Q33_G || \
				 devid == PCI_CHIP_Q35_G || IS_IGD(devid))

#define IS_965(devid)		(devid == PCI_CHIP_I965_G || \
				 devid == PCI_CHIP_I965_Q || \
				 devid == PCI_CHIP_I965_G_1 || \
				 devid == PCI_CHIP_I965_GM || \
				 devid == PCI_CHIP_I965_GME || \
				 devid == PCI_CHIP_I946_GZ || \
				 IS_G4X(devid) || \
				 IS_IRONLAKE(devid))

#define IS_9XX(devid)		(IS_915(devid) || \
				 IS_945(devid) || \
				 IS_965(devid))


#endif /* CAIRO_DRM_BASIC_PRIVATE_H */
