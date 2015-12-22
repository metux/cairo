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

#include <inttypes.h>		// broken xf86drmMode.h
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-basic-private.h"

#include "cairo-error-private.h"
#include "cairo-image-surface-private.h"

#define _DEBUG(text, ...)	\
    { fprintf(stderr, "[drm/basic] " text "\n", ##__VA_ARGS__); }

// FIXME: double-copy
void
_cairo_drm_basic_bo_read (const cairo_drm_basic_device_t *device,
			  cairo_drm_basic_bo_t *bo,
			  unsigned long offset,
			  unsigned long size,
			  void *data)
{
    _DEBUG("_cairo_drm_basic_bo_read()");

    assert (device);
    assert (bo);
    assert (data);

    uint8_t *ptr = device->base.bo.map (&(device->base), &(bo->base));

    if (ptr == NULL) {
	_DEBUG("_cairo_drm_basic_bo_read(): failed to map");
	return;
    }

    memcpy (data, ptr + offset, size);
    _cairo_drm_bo_unmap (&(bo->base));

    VG (VALGRIND_MAKE_MEM_DEFINED (data, size));
}

void *
_cairo_drm_basic_bo_map (const cairo_drm_device_t *drm_dev,
			 cairo_drm_bo_t *drm_bo)
{
    _DEBUG("_cairo_drm_basic_bo_map()");

    const cairo_drm_basic_device_t* device = _cairo_drm_device_cast_basic_const (drm_dev);
    cairo_drm_basic_bo_t *bo = _cairo_drm_bo_cast_basic (drm_bo);

    assert(drm_dev);
    assert(drm_bo);
    assert(device);
    assert(bo);
    assert(bo->mode_dev);

    if (bo->base.mapped != NULL) {
	_DEBUG("WARN: bo is already mapped");
	return bo->base.mapped;
    }

    /* perform actual memory mapping */
    bo->base.mapped = mmap(0, bo->mode_dev->size, PROT_READ | PROT_WRITE, MAP_SHARED,
	        device->base.fd, bo->mode_dev->map_offset);
    if (bo->base.mapped  == MAP_FAILED) {
	_DEBUG("cannot mmap dumb buffer (%d): %m", errno);
	return NULL;
    }

    /* clear the framebuffer to 0 */
    _DEBUG("bo_map() clearing framebuffer at: %ld => %d", (long)bo->base.mapped, bo->base.size);
    memset(bo->base.mapped, 0x66, bo->base.size);

    return bo->base.mapped;
}

static int modeset_find_crtc(cairo_drm_basic_device_t *device,
			     drmModeRes *res,
			     drmModeConnector *conn,
			     struct modeset_dev *dev)
{
    drmModeEncoder *enc = NULL;

    /* first try the currently conected encoder+crtc */
    if (conn->encoder_id)
	enc = drmModeGetEncoder(device->base.fd, conn->encoder_id);

    if (enc) {
	if (enc->crtc_id) {

	    struct modeset_dev *iter;
	    int have_crtc = 1;

	    for (iter = device->mode_list; iter; iter = iter->next) {
		if (iter->crtc == enc->crtc_id) {
		    have_crtc = 0;
		    break;
		}
	    }

	    if (have_crtc) {
		drmModeFreeEncoder(enc);
		dev->crtc = enc->crtc_id;
		return 1;
	    }
	}

	drmModeFreeEncoder(enc);
    }

    /* If the connector is not currently bound to an encoder or if the
     * encoder+crtc is already used by another connector (actually unlikely
     * but lets be safe), iterate all other available encoders to find a
     * matching CRTC. */
    int i;
    for (i = 0; i < conn->count_encoders; ++i) {
	enc = drmModeGetEncoder(device->base.fd, conn->encoders[i]);
	if (!enc) {
	    _DEBUG("cannot retrieve encoder %u:%u (%d): %m",
		i, conn->encoders[i], errno);
	    continue;
	}

	/* iterate all global CRTCs */
	int j;
	for (j = 0; j < res->count_crtcs; ++j) {
	    /* check whether this CRTC works with the encoder */
	    if (!(enc->possible_crtcs & (1 << j)))
		continue;

	    /* check that no other device already uses this CRTC */
	    struct modeset_dev *iter;
	    int have_crtc = 1;
	    for (iter = device->mode_list; iter; iter = iter->next) {
		if (iter->crtc == res->crtcs[j]) {
		    have_crtc = 0;
		    break;
		}
	    }

	    /* we have found a CRTC, so save it and return */
	    if (have_crtc) {
		drmModeFreeEncoder(enc);
		dev->crtc = res->crtcs[j];
		return 1;
	    }
	}

	drmModeFreeEncoder(enc);
    }

    _DEBUG("cannot find suitable CRTC for connector %u", conn->connector_id);
    return 0;
}

static int modeset_create_fb(cairo_drm_basic_device_t *device,
			     struct modeset_dev *dev)
{
    struct drm_mode_map_dumb mreq = { 0 };
    int ret;

    /* create dumb buffer */
    struct drm_mode_create_dumb creq = {
	.width = dev->mode.hdisplay,
	.height = dev->mode.vdisplay,
	.bpp = 32
    };

    ret = drmIoctl(device->base.fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
    if (ret < 0) {
	_DEBUG("cannot create dumb buffer (%d): %m", errno);
	return -errno;
    }
    dev->stride = creq.pitch;
    dev->size = creq.size;
    dev->handle = creq.handle;

    /* create framebuffer object for the dumb-buffer */
    ret = drmModeAddFB(device->base.fd,
		       dev->mode.hdisplay,
		       dev->mode.vdisplay,
		       24,
		       32,
		       dev->stride,
		       dev->handle,
		       &dev->fb);
    if (ret) {
	_DEBUG("cannot create framebuffer (%d): %m", errno);
	ret = -errno;
	goto err_destroy;
    }

    /* prepare buffer for memory mapping */
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = dev->handle;
    ret = drmIoctl(device->base.fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (ret) {
	_DEBUG("cannot map dumb buffer (%d): %m", errno);
	ret = -errno;
	goto err_fb;
    }

    dev->map_offset = mreq.offset;

    return 0;

err_fb:
    drmModeRmFB(device->base.fd, dev->fb);

err_destroy:
    {
	struct drm_mode_destroy_dumb dreq = { .handle = dev->handle };
	drmIoctl(device->base.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	return ret;
    }
}

static int modeset_setup_dev(cairo_drm_basic_device_t *device,
			     drmModeRes *res,
			     drmModeConnector *conn,
			     struct modeset_dev *dev)
{
    int ret;

    /* check if a monitor is connected */
    if (conn->connection != DRM_MODE_CONNECTED) {
	_DEBUG("ignoring unused connector %u", conn->connector_id);
	return -ENOENT;
    }

    /* check if there is at least one valid mode */
    if (conn->count_modes == 0) {
	_DEBUG("no valid mode for connector %u", conn->connector_id);
	return -EFAULT;
    }

    /* copy the mode information into our device structure */
    dev->mode = conn->modes[0];

    _DEBUG("mode for connector %u is %ux%u",
	conn->connector_id, dev->mode.hdisplay, dev->mode.vdisplay);

    /* find a crtc for this connector */
    if (!modeset_find_crtc(device, res, conn, dev)) {
	_DEBUG("no valid crtc for connector %u",
	    conn->connector_id);
	return -ENOENT;
    }

    /* create a framebuffer for this CRTC */
    ret = modeset_create_fb(device, dev);
    if (ret) {
	_DEBUG("cannot create framebuffer for connector %u",
	    conn->connector_id);
	return ret;
    }

    return 0;
}

static int modeset_prepare(cairo_drm_basic_device_t *device)
{
    drmModeRes *res;
    drmModeConnector *conn;
    struct modeset_dev *dev;
    int ret;

    device->mode_list = NULL;

    /* retrieve resources */
    res = drmModeGetResources(device->base.fd);
    if (!res) {
	_DEBUG("cannot retrieve DRM resources (%d): %m", errno);
	return -errno;
    }

    /* iterate all connectors */
    int i;
    for (i = 0; i < res->count_connectors; ++i) {
	/* get information for each connector */
	conn = drmModeGetConnector(device->base.fd, res->connectors[i]);
	if (!conn) {
	    _DEBUG("cannot retrieve DRM connector %u:%u (%d): %m",
		i, res->connectors[i], errno);
	    continue;
	}

	/* create a device structure */
	dev = malloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	dev->conn = conn->connector_id;

	/* call helper function to prepare this connector */
	ret = modeset_setup_dev(device, res, conn, dev);
	if (ret) {
	    if (ret != -ENOENT) {
		errno = -ret;
		_DEBUG("cannot setup device for connector %u:%u (%d): %m",
		    i, res->connectors[i], errno);
	    }
	    free(dev);
	    drmModeFreeConnector(conn);
	    continue;
	}

	/* free connector data and link device into global list */
	drmModeFreeConnector(conn);
	dev->next = device->mode_list;
	device->mode_list = dev;
    }

    /* free resources again */
    drmModeFreeResources(res);
    return 0;
}

cairo_drm_bo_t *
_cairo_drm_basic_bo_create (cairo_drm_basic_device_t *device,
			    cairo_format_t format,
			    uint32_t width,
			    uint32_t height)
{
    cairo_drm_basic_bo_t *bo = _cairo_drm_bo_cast_basic (_cairo_drm_bo_from_pool (&device->base));

    assert(bo);

    _DEBUG("_cairo_drm_basic_bo_create() format=%d width=%d height=%d", format, width, height);

    // FIXME: just picking the first one
    struct modeset_dev *mode_dev = bo->mode_dev = device->mode_list;

    // FIXME: need to handle dimensions!
    bo->base.handle = mode_dev->handle;
    bo->base.size   = mode_dev->size;

    _DEBUG("fb id: %u", mode_dev->fb);

    CAIRO_REFERENCE_COUNT_INIT (&bo->base.ref_count, 1);
    return &bo->base;
}

cairo_surface_t *
_cairo_drm_basic_bo_get_image (const cairo_drm_device_t *drm_dev,
			       cairo_drm_bo_t *drm_bo,
			       const cairo_drm_surface_t *surface)
{
    cairo_image_surface_t *image;
    uint8_t *dst;
    int size, row;

    _DEBUG("drm_basic_bo_get_image()");

    const cairo_drm_basic_device_t *device = _cairo_drm_device_cast_basic_const (drm_dev);
    cairo_drm_basic_bo_t *bo = _cairo_drm_bo_cast_basic (drm_bo);

    image = _cairo_surface_cast_image (
	cairo_image_surface_create (surface->format,
				    surface->width,
				    surface->height));
    if (unlikely (image->base.status))
	return &image->base;

    if (image->stride == surface->stride) {
	size = surface->stride * surface->height;
	_cairo_drm_basic_bo_read (device, bo, 0, size, image->data);
    } else {
	int offset;

	size = surface->width;
	if (surface->format != CAIRO_FORMAT_A8)
	    size *= 4;

	offset = 0;
	row = surface->height;
	dst = image->data;
	while (row--) {
	    _cairo_drm_basic_bo_read (device, bo, offset, size, dst);
	    offset += surface->stride;
	    dst += image->stride;
	}
    }

    return &image->base;
}

cairo_status_t
_cairo_drm_basic_device_init (cairo_drm_basic_device_t *device,
			      int fd)
{
    _cairo_freepool_init (&device->base.bo_pool, sizeof (cairo_drm_basic_bo_t));

    device->base.bo.release			= _cairo_drm_bo_release;
    device->base.bo.map				= _cairo_drm_basic_bo_map;
    device->base.bo.get_image			= _cairo_drm_basic_bo_get_image;

    device->base.surface.create			= _cairo_drm_basic_surface_create;
    device->base.surface.create_for_name	= _cairo_drm_basic_surface_create_for_name;
    device->base.surface.flink			= _cairo_drm_surface_flink;

    device->base.device.destroy			= _cairo_drm_basic_device_destroy;

    device->base.fd = fd;

    /* prepare all connectors and CRTCs */
    int ret;
    if ((ret = modeset_prepare(device)))
    {
	_DEBUG("modeset_prepare() failed: %s", strerror(errno));
	return CAIRO_STATUS_DEVICE_ERROR;
    }

    /* perform actual modesetting on each found connector+CRTC */
    struct modeset_dev *iter;
    for (iter = device->mode_list; iter; iter = iter->next) {
	ret = drmModeSetCrtc(
		device->base.fd,
		iter->crtc,
		iter->fb,
		0,
		0,
		&iter->conn,
		1,
		&iter->mode);

	if (ret)
	    _DEBUG("cannot set CRTC for connector %u (%d): %m",
		iter->conn, errno);
    }

    return CAIRO_STATUS_SUCCESS;
}
