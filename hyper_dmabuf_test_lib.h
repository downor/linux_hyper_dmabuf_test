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

void init_exporter(int hyper_dmabuf_fd, int to_domid);

void init_importer(int hyper_dmabuf_fd, int from_domid);

int create_sample_bo(int width, int height, int bpp, const char *surf_name);

int verify_bo_data(int prime_fd, int width, int height, int bpp);

hyper_dmabuf_id_t export_bo_as_hbuf(int hyper_dmabuf_fd, int to_domid, int prime);

int import_bo_from_hbuf(int hyper_dmabuf_fd, hyper_dmabuf_id_t hid,
			int width, int height, int bpp, int *out_buf);

int fill_bo_from_hbuf(int hyper_dmabuf_fd, hyper_dmabuf_id_t hid,
		      int width, int height, int bpp, int pattern);

void false_ioctl_call(int hyper_dmabuf_fd, hyper_dmabuf_id_t false_hid);

