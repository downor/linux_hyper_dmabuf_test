/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdrm/intel_bufmgr.h>
#include <xf86drm.h>
#include <linux/ioctl.h>
#include "hyper_dmabuf.h"

void init_exporter(int hyper_dmabuf_fd, int to_domid)
{
        struct ioctl_hyper_dmabuf_tx_ch_setup msg = {0};

	msg.remote_domain = to_domid;

	if(ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_TX_CH_SETUP, &msg)) {
		printf("%s: ioctl failed\n", __func__);
		return;
	}

	printf("%s: ioctl successful\n", __func__);
}

void init_importer(int hyper_dmabuf_fd, int from_domid)
{
        struct ioctl_hyper_dmabuf_rx_ch_setup msg = {0};

	msg.source_domain = from_domid;

	if(ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_RX_CH_SETUP, &msg)) {
		printf("%s: ioctl failed\n", __func__);
		return;
	}
	printf("%s: ioctl successful\n", __func__);
}

int create_sample_bo(int width, int height, int bpp, const char *surf_name)
{
	struct drm_prime_handle prime;
	ssize_t size = width * height * bpp/8;
	int drm_fd;
	int *data;
	int i, ret;

	drm_fd = drmOpen("i915", NULL);
	if (drm_fd < 0) {
		printf("Failed to open DRM device\n");
		return -1;
	}

	dri_bufmgr *bufmgr = intel_bufmgr_gem_init(drm_fd, 0x80000);

	drm_intel_bo *bo = drm_intel_bo_alloc_for_render(bufmgr, surf_name, size, 0);

	drm_intel_bo_map(bo, 1);

	/* Put counting patterns in the image */
	data = (int*)bo->virtual;
	for (i = 0; i < size/sizeof(int); i++) {
		data[i] = i;
	}

	drm_intel_bo_unmap(bo);

	memset(&prime, 0, sizeof(prime));
	prime.handle = bo->handle;

	ret = ioctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &prime);
	if (ret) {
		printf("Failed to export primeFD\n");
		return -1;
	}

	drmClose(drm_fd);

	return prime.fd;
}

int verify_bo_data(int prime_fd, int width, int height, int bpp)
{
	int drm_fd, i;
	drm_intel_bo *bo;
	int *data;
	ssize_t size = width * height * bpp/8;
	drm_fd = drmOpen("i915", NULL);
	if (drm_fd < 0) {
		printf("Failed to open drm device\n");
		return -1;
	}

	dri_bufmgr *bufmgr = intel_bufmgr_gem_init(drm_fd, 0x80000);

	bo = drm_intel_bo_gem_create_from_prime(bufmgr, prime_fd, size);

	if (!bo) {
		printf("failed to map bo\n");
		return -1;
	}

	drm_intel_gem_bo_map_gtt(bo);

	if (!bo->virtual) {
		printf("failed to map bo in aperture\n");
		return -1;
	}

	data = (int*)bo->virtual;
	for (i = 0; i < size/sizeof(int); i++) {
		if (data[i] != i) {
			printf("Buffer content changed\n");
			return 1;
		}
	}

	printf("Buffer content not changed\n");

	return 0;
}

hyper_dmabuf_id_t export_bo_as_hbuf(int hyper_dmabuf_fd, int to_domid, int prime)
{
        struct ioctl_hyper_dmabuf_export_remote msg = {0};

	msg.remote_domain = to_domid;
	msg.dmabuf_fd = prime;

	if(ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_EXPORT_REMOTE, &msg)) {
		printf("%s: ioctl failed\n", __func__);
		return (hyper_dmabuf_id_t){-1, {0, 0, 0}};
	}

	printf("%s: ioctl successful\n", __func__);

	return msg.hid;
}

int import_bo_from_hbuf(int hyper_dmabuf_fd, hyper_dmabuf_id_t hid,
			int width, int height, int bpp, int *out_buf)
{
        struct ioctl_hyper_dmabuf_export_fd msg = {0};
	int drm_fd;
	drm_intel_bo *bo;
	ssize_t size = width * height * bpp/8;

	msg.hid = hid;

	if(ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_EXPORT_FD, &msg)) {
		printf("%s: ioctl failed\n", __func__);
		return -1;
	}

	printf("%s: ioctl successful\n", __func__);

	drm_fd = drmOpen("i915", NULL);
	if (drm_fd < 0) {
		printf("Failed to open drm device\n");
		return -1;
	}

	dri_bufmgr *bufmgr = intel_bufmgr_gem_init(drm_fd, 0x80000);

	bo = drm_intel_bo_gem_create_from_prime(bufmgr, msg.fd, size);

	if (!bo) {
		printf("failed to map bo\n");
		return -1;
	}

	drm_intel_gem_bo_map_gtt(bo);

	if (!bo->virtual) {
		printf("failed to map bo in aperture\n");
		return -1;
	}

	/* Get data from start of buffer and print them out */
	memcpy(out_buf, (int*)bo->virtual, size);
	drm_intel_gem_bo_unmap_gtt(bo);

	return 0;
}

int fill_bo_from_hbuf(int hyper_dmabuf_fd, hyper_dmabuf_id_t hid,
		      int width, int height, int bpp, char pattern)
{
        struct ioctl_hyper_dmabuf_export_fd msg = {0};
	int drm_fd;
	drm_intel_bo *bo;
	size_t size = width * height * bpp/8;

	msg.hid = hid;

	if(ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_EXPORT_FD, &msg)) {
		printf("%s: ioctl failed\n", __func__);
		return -1;
	}

	printf("%s: ioctl successful\n", __func__);

	drm_fd = drmOpen("i915", NULL);
	if (drm_fd < 0) {
		printf("Failed to open drm device\n");
		return -1;
	}

	dri_bufmgr *bufmgr = intel_bufmgr_gem_init(drm_fd, 0x80000);

	bo = drm_intel_bo_gem_create_from_prime(bufmgr, msg.fd, size);

	if (!bo) {
		printf("failed to map bo\n");
		return -1;
	}

	drm_intel_gem_bo_map_gtt(bo);

	if (!bo->virtual) {
		printf("failed to map bo in aperture\n");
		return -1;
	}

	/* Get data from start of buffer and print them out */
	memset(bo->virtual, pattern, size);
	drm_intel_gem_bo_unmap_gtt(bo);

	return 0;
}

void false_ioctl_call(int hyper_dmabuf_fd, hyper_dmabuf_id_t false_hid)
{
	struct ioctl_hyper_dmabuf_export_fd import_msg = {0};
	struct ioctl_hyper_dmabuf_export_remote export_msg = {0};
	struct ioctl_hyper_dmabuf_unexport unexport_msg = {0};
	struct ioctl_hyper_dmabuf_query query_msg = {0};

	import_msg.hid = false_hid;

	export_msg.remote_domain = rand();
	export_msg.dmabuf_fd = rand();

	unexport_msg.hid = false_hid;
	unexport_msg.delay_ms = 100;

	query_msg.hid = false_hid;
	query_msg.item = rand();

	ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_EXPORT_REMOTE, &export_msg);

	ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_EXPORT_FD, &import_msg);

	ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_UNEXPORT, &unexport_msg);

	ioctl(hyper_dmabuf_fd, IOCTL_HYPER_DMABUF_QUERY, &query_msg);
}
