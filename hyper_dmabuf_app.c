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
#include <signal.h>
#include "hyper_dmabuf.h"
#include "hyper_dmabuf_test_lib.h"

#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1080
#define IMAGE_BPP 4 /* BYTE PER PIXEL */

enum app_type {
	EXPORTER = 0x100,
	IMPORTER = 0x110,
	CORRUPT = 0x120,
	FALSE_IOCTL  = 0x130,
	INVALID = 0x0
};

void print_help(enum app_type type)
{
	printf("Usage:\n\n");
	switch (type) {
		case EXPORTER:
			printf("./hyper_dmabuf_app exp <domid of importer0> <domid of importer1> ...\n");
			printf("ex) ./hyper_dmabuf_app exp 0 2 (exporting fullHD image to dom0 and dom2 as a hyper_dmabuf)\n");
			break;

		case IMPORTER:
			printf("./hyper_dmabuf_app imp <domid of exporter> <hid.id> <hid.key[0]> <hid.key[1]> <hid.key[2]>\n");
			printf("ex) ./hyper_dmabuf_app imp 1 8 392874597 3982547 76908456 (run importing test on hyper_dmabuf)\n");
			break;

		case CORRUPT:
			printf("./hyper_dmabuf_app corrupt <domid of exporter> <hid.id> <hid.key[0]> <hid.key[1]> <hid.key[2]>\n");
			printf("ex) ./hyper_dmabuf_app corrupt 1 8 392874597 3982547 76908456 (run corruption test on hyper_dmabuf)\n");
			break;

		case FALSE_IOCTL:
			printf("./hyper_dmabuf_app false_ioctl <num of iterations>\n");
			printf("ex) ./hyper_dmabuf_app false_ioctl 100 (run false ioctl test on hyper_dmabuf device)\n");
			break;

		default:
			printf("Syntax: ./hyper_dmabuf_app <type of application> <arguments>\n\n");

			printf("./hyper_dmabuf_app exp <domid of importer0> <domid of importer1> ...\n");
			printf("ex) ./hyper_dmabuf_app exp 0 2 (exporting fullHD image to dom0 and dom2 as a hyper_dmabuf)\n\n");

			printf("./hyper_dmabuf_app imp <domid of exporter> <hid.id> <hid.key[0]> <hid.key[1]> <hid.key[2]>\n");
			printf("ex) ./hyper_dmabuf_app imp 1 8 392874597 3982547 76908456 (run importing test on hyper_dmabuf)\n\n");

			printf("./hyper_dmabuf_app corrupt <domid of exporter> <hid.id> <hid.key[0]> <hid.key[1]> <hid.key[2]>\n");
			printf("ex) ./hyper_dmabuf_app corrupt 1 8 392874597 3982547 76908456 (run corruption test on hyper_dmabuf)\n\n");

			printf("./hyper_dmabuf_app false_ioctl <num of iterations>\n");
			printf("ex) ./hyper_dmabuf_app false_ioctl 100 (run false ioctl test on hyper_dmabuf device\n\n");
	}
}

static int running = 1;

int exporter_routine(int *importers, int n_importers)
{
	int i;
	int h_fd;
	int importer_id;
	int prime;
	hyper_dmabuf_id_t hid;

	h_fd = open("/dev/xen/hyper_dmabuf", O_RDWR);

	if (h_fd < 0) {
		printf("Failed to open hyper_dmabuf device\n");
		return -1;
	}

	/* create image to export */
	prime = create_sample_bo(IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BPP*8, "hyper_dmabuf_test_image");

	if (prime < 0) {
		printf("Failed to create an image\n");
		close(h_fd);
		return -1;
	}

	/* set up expoter->importer comm channels */
	for (i = 0; i < n_importers; i++) {
		importer_id = *(importers+i);
		init_exporter(h_fd, importer_id);
		hid = export_bo_as_hbuf(h_fd, importer_id, prime);

		if (hid.id < 0) {
			printf("Failed to export a buffer to %d\n", importer_id);
			close(h_fd);
			return -1;
		}

		printf("TO domain:id == %d: hyper_dmabuf_id of exported buffer: <id:%d key0:%d key1:%d key2:%d>\n",
			importer_id, hid.id, hid.rng_key[0], hid.rng_key[1], hid.rng_key[2]);
	}

	while(running)
		sleep(1);

	verify_bo_data(prime, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BPP*8);

	close(h_fd);
	return 0;
}

int importer_routine(int exporter, hyper_dmabuf_id_t hid)
{
	int i;
	int h_fd;
	int *image_buf, *temp;
	int ret;

	h_fd = open("/dev/xen/hyper_dmabuf", O_RDWR);

	if (h_fd < 0) {
		printf("Failed to open hyper_dmabuf device\n");
		return -1;
	}

	init_importer(h_fd, exporter);

	image_buf = (int*)malloc(IMAGE_HEIGHT * IMAGE_WIDTH * IMAGE_BPP);

	if (!image_buf) {
		printf("Failed to allocate buffer for imported image\n");
		close(h_fd);
		return -1;
	}

	/* import image */
	ret = import_bo_from_hbuf(h_fd, hid, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BPP*8, image_buf);

	close(h_fd);

	if (ret < 0) {
		printf("Failed to import an image\n");
		return -1;
	}

	temp = image_buf;

	/* comparing */
	for (i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_BPP/4; i++)
	{
		if (*temp != i) {
			printf("Data mismatch found at the offset of 0x%x, expected:0x%x, actual:0x%x\n",
				i, i, *temp);
			free(image_buf);
			return -1;
		}
		temp++;
	}

	free(image_buf);
	return 0;
}

int corrupt_attacker_routine(int exporter, hyper_dmabuf_id_t hid)
{
	int h_fd;
	int ret;

	h_fd = open("/dev/xen/hyper_dmabuf", O_RDWR);

	if (h_fd < 0) {
		printf("Failed to open hyper_dmabuf device\n");
		return -1;
	}

	init_importer(h_fd, exporter);

	/* fill imported image with 0xa5 */
	ret = fill_bo_from_hbuf(h_fd, hid, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_BPP*8, 0xa5);

	close(h_fd);

	if (ret < 0) {
		printf("Failed to corrupt imported image\n");
		return -1;
	}

	return 0;
}

int false_ioctl_attacker_routine(int iteration)
{
	int h_fd;
	hyper_dmabuf_id_t hid;

	h_fd = open("/dev/xen/hyper_dmabuf", O_RDWR);

	if (h_fd < 0) {
		printf("Failed to open hyper_dmabuf device\n");
		return -1;
	}

	while(iteration--) {
		hid.id = rand();
		hid.rng_key[0] = rand();
		hid.rng_key[1] = rand();
		hid.rng_key[2] = rand();

		/* ch setup ioctls with random domid */
		init_exporter(h_fd, rand());
		init_importer(h_fd, rand());

		/* import/export ioctls with random hid */
		false_ioctl_call(h_fd, hid);
	}

	close(h_fd);

	return 0;
}

static void
signal_int(int signum)
{
	running = 0;
}

int main(int argc, char** argv)
{
	enum app_type type = 0;
	int *imp_domid;
	int min_args;
	int i, ret;
	hyper_dmabuf_id_t hid;
	struct sigaction sigint;

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);

	if (strcmp(argv[1], "exp") == 0) {
		type = EXPORTER;
		min_args = 3;
	} else if (strcmp(argv[1], "imp") == 0) {
		type = IMPORTER;
		min_args = 7;
	} else if (strcmp(argv[1], "corrupt") == 0) {
		type = CORRUPT;
		min_args = 7;
	} else if (strcmp(argv[1], "false_ioctl") == 0) {
		type = FALSE_IOCTL;
		min_args = 2;
	}

	if ( type==0 || (argc < min_args)) {
		print_help(type);
		return -1;
	}

	switch (type) {
		/* exporter test */
	case EXPORTER:
		imp_domid = (int*)malloc(sizeof(*imp_domid)*(argc-2));

		for (i = 0 ; i < (argc-2); i++) {
			imp_domid[i] = strtol(argv[i+2], NULL, 10);
		}
			/* exporting samne image to domains */
		ret = exporter_routine(imp_domid, argc-2);
		break;

	case IMPORTER:
		hid.id = strtol(argv[3], NULL, 10);
		hid.rng_key[0] = strtol(argv[4], NULL, 10);
		hid.rng_key[1] = strtol(argv[5], NULL, 10);
		hid.rng_key[2] = strtol(argv[6], NULL, 10);
		ret = importer_routine(strtol(argv[2], NULL, 10), hid);
		break;

	case CORRUPT:
		hid.id = strtol(argv[3], NULL, 10);
		hid.rng_key[0] = strtol(argv[4], NULL, 10);
		hid.rng_key[1] = strtol(argv[5], NULL, 10);
		hid.rng_key[2] = strtol(argv[6], NULL, 10);
		ret = corrupt_attacker_routine(strtol(argv[2], NULL, 10), hid);
		break;

	case FALSE_IOCTL:
		ret = false_ioctl_attacker_routine(strtol(argv[2], NULL, 10));
		break;

	default:
		/* can't get here */
		break;
	}

	return ret;
}


