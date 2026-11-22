/* SPDX-License-Identifier: MIT */
/* r615 family support: which chipsets the native 615.78.08 backend runs on
 * and where their firmware lives.
 *
 * Supported: GA107 (validated on hardware) and the AD10x family
 * (AD102/AD103/AD104/AD106/AD107). Chipset values from
 * nvkm/engine/device/base.c (nv177_chipset, nv192_chipset..nv197_chipset).
 *
 * AD10x shares one firmware set (nvidia/ad102/gsp/): the GSP-RM image and
 * ucodes are the same ga10x files as GA107 (R615 nv-firmware.h maps
 * NV_FIRMWARE_CHIP_FAMILY_AD10X to gsp_ga10x / ucodes_ga10x, and the
 * 615.78.08 installer ships no gsp_ad10x.bin), while booter_load,
 * booter_unload and bootloader are generated from the AD102 bindata.
 * The ad103/ad104/ad106/ad107 directories are symlinks to ad102 in
 * linux-firmware, so every AD10x chipset resolves to "ad102" here.
 *
 * The WPR/transport HAL is the TU102 one for all of these chips: the
 * 615 TU102 register HAL (message-queue registers 0x110c00+) is the same
 * in the TU102 and GA102 published headers, Ada has no separate GSP
 * register header (it reuses the GA102 GSP path), and
 * kgspPopulateWprMeta_TU102 is dispatched for TU102..AD107.
 */
#ifndef __NVKM_R615_FAMILY_H__
#define __NVKM_R615_FAMILY_H__

#ifdef __KERNEL__
#include <linux/types.h>
#endif
/* Userspace test harnesses (tests/rusd615_compat.h) provide u32/u64
 * themselves; the __KERNEL__ guard keeps this header includable there. */

static inline bool
r615_chipset_supported(u32 chipset)
{
	switch (chipset) {
	case 0x177: /* GA107 */
	case 0x192: /* AD102 */
	case 0x193: /* AD103 */
	case 0x194: /* AD104 */
	case 0x196: /* AD106 */
	case 0x197: /* AD107 */
		return true;
	default:
		return false;
	}
}

static inline bool
r615_chipset_is_ad10x(u32 chipset)
{
	switch (chipset) {
	case 0x192: /* AD102 */
	case 0x193: /* AD103 */
	case 0x194: /* AD104 */
	case 0x196: /* AD106 */
	case 0x197: /* AD107 */
		return true;
	default:
		return false;
	}
}

/* Firmware directory for a supported chipset: GA10x shares ga107,
 * AD10x shares ad102. Only call with a supported chipset. */
static inline const char *
r615_firmware_dir(u32 chipset)
{
	return r615_chipset_is_ad10x(chipset) ? "ad102" : "ga107";
}

#endif
