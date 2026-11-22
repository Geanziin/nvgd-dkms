/*
 * Copyright 2023 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#include "priv.h"

static const struct nvkm_gsp_func
ad102_gsp = {
	.flcn = &ga102_gsp_flcn,
	.fwsec = &ga102_gsp_fwsec,

	.sig_section = ".fwsignature_ad10x",

	.booter.ctor = ga102_gsp_booter_ctor,

	.fwsec_sb.ctor = tu102_gsp_fwsec_sb_ctor,
	.fwsec_sb.dtor = tu102_gsp_fwsec_sb_dtor,

	.dtor = r535_gsp_dtor,
	.oneinit = tu102_gsp_oneinit,
	.init = tu102_gsp_init,
	.fini = tu102_gsp_fini,
	.reset = ga102_gsp_reset,

	.rm.gpu = &ad10x_gpu,
};

static const struct nvkm_gsp_func
ad102_gsp_r615 = {
	.flcn = &ga102_gsp_flcn,
	.fwsec = &ga102_gsp_fwsec,

	.sig_section = ".fwsignature_ad10x",

	.booter.ctor = ga102_gsp_booter_ctor,

	.fwsec_sb.ctor = tu102_gsp_fwsec_sb_ctor,
	.fwsec_sb.dtor = tu102_gsp_fwsec_sb_dtor,

	.dtor = r615_gsp_dtor,
	.oneinit = r615_gsp_oneinit,
	.init = r615_gsp_init,
	.fini = r615_gsp_fini,
	.reset = ga102_gsp_reset,

	.rm.gpu = &ad10x_gpu,
};

static struct nvkm_gsp_fwif
ad102_gsps[] = {
	/* 615 requires its native lifecycle and framed MSGQ/GMCAPI transport.
	 * Single 615 entry: the 570-path hybrid (candidate 10) was removed so a
	 * version pin can only ever select the native backend. 615 is opt-in
	 * (nvkm_gsp_fwif_order skips it unless pinned); the r615 backend on
	 * Ada additionally fails closed at r615_host_new while the AD10x
	 * sec2-bl has no staged evidence (see firmware-ad102-615.78.08-01).
	 */
	{  2, r615_gsp_load, &ad102_gsp_r615, &r615_rm_ga102, "615.78.08" },
	{ 1, tu102_gsp_load, &ad102_gsp, &r570_rm_ga102, "570.144" },
	{ 0, tu102_gsp_load, &ad102_gsp, &r535_rm_ga102, "535.113.01" },
	{}
};

int
ad102_gsp_new(struct nvkm_device *device, enum nvkm_subdev_type type, int inst,
	      struct nvkm_gsp **pgsp)
{
	struct nvkm_gsp_fwif selection[ARRAY_SIZE(ad102_gsps)];
	int ret;

	ret = nvkm_gsp_fwif_order(selection, ARRAY_SIZE(selection), ad102_gsps);
	if (ret)
		return ret;
	return nvkm_gsp_new_(selection, device, type, inst, pgsp);
}

NVKM_GSP_FIRMWARE_BOOTER(ad102, 535.113.01);
NVKM_GSP_FIRMWARE_BOOTER(ad103, 535.113.01);
NVKM_GSP_FIRMWARE_BOOTER(ad104, 535.113.01);
NVKM_GSP_FIRMWARE_BOOTER(ad106, 535.113.01);
NVKM_GSP_FIRMWARE_BOOTER(ad107, 535.113.01);

NVKM_GSP_FIRMWARE_BOOTER(ad102, 570.144);
NVKM_GSP_FIRMWARE_BOOTER(ad103, 570.144);
NVKM_GSP_FIRMWARE_BOOTER(ad104, 570.144);
NVKM_GSP_FIRMWARE_BOOTER(ad106, 570.144);
NVKM_GSP_FIRMWARE_BOOTER(ad107, 570.144);

NVKM_GSP_FIRMWARE_BOOTER(ad102, 615.78.08);
NVKM_GSP_FIRMWARE_BOOTER(ad103, 615.78.08);
NVKM_GSP_FIRMWARE_BOOTER(ad104, 615.78.08);
NVKM_GSP_FIRMWARE_BOOTER(ad106, 615.78.08);
NVKM_GSP_FIRMWARE_BOOTER(ad107, 615.78.08);
