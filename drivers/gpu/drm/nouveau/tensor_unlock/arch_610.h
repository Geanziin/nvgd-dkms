/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GPU_UNLOCK_ARCH_610_H
#define GPU_UNLOCK_ARCH_610_H

enum gpu_unlock_booter_arch {
	GU_BOOTER_GA10X = 0,
	GU_BOOTER_AD10X = 1,
};

/* Ada (AD10x) PCI IDs the unlock knows.
 *
 * Single source of truth: admission in tensor_unlock.c
 * (supported_ada_ids[] / experimental_ids[]) and AD102-family asset
 * routing in unlock_610.c (ad102_family()) are both expanded from the
 * X-macros below, so the two can never desync and hand an Ada the GA102
 * payload through the unlock_assets() fallthrough. Add a new Ada ID
 * here, in the right macro, and both sides pick it up.
 *
 * X is invoked once per ID as X(constant). IDs in
 * GPU_UNLOCK_AD10X_SUPPORTED_IDS are admitted by default; IDs in
 * GPU_UNLOCK_AD10X_EXPERIMENTAL_IDS need
 * nvgd.tensor_unlock_experimental=1 (tensor_unlock.c).
 *
 * Product names verified against pci.ids (hwdata, vendor 10de):
 * several products carry TWO device IDs (different boards/steppings).
 * The _LAPTOP / _MOBILE suffix only distinguishes the two IDs of one
 * product; both IDs of a pair are the same NVIDIA product:
 *   0x2717 / 0x2757  AD103M RTX 4090 Laptop GPU
 *   0x2820 / 0x2860  AD106M RTX 4070 Max-Q / Mobile
 *   0x28a0 / 0x28e0  AD107M RTX 4060 Max-Q / Mobile
 *   0x28a1 / 0x28e1  AD107M RTX 4050 Max-Q / Mobile
 * "16GB" in GU_AD106_RTX_4060_TI_16GB is part of the NVIDIA product
 * name (pci.ids: "GeForce RTX 4060 Ti 16GB"), not a local label.
 * No ID here is validated on Ada hardware yet; every experimental ID
 * is unvalidated by definition, and the supported 4090 is likewise
 * unvalidated by the lab (see the validation matrix, PENDENTE DE
 * HARDWARE).
 */
#define GU_AD102_RTX_4090 0x2684u
#define GU_AD102_RTX_4090_D 0x2685u
#define GU_AD102_RTX_4070_TI_SUPER 0x2689u /* cut-down AD102, not the AD103 0x2705 */
#define GU_AD103_RTX_4080_SUPER 0x2702u
#define GU_AD103_RTX_4080 0x2704u
#define GU_AD103_RTX_4070_TI_SUPER 0x2705u
#define GU_AD103_RTX_4070 0x2709u
#define GU_AD103M_RTX_4090_LAPTOP 0x2717u
#define GU_AD103M_RTX_4090_MOBILE 0x2757u
#define GU_AD104_RTX_4070_TI 0x2782u
#define GU_AD104_RTX_4070_SUPER 0x2783u
#define GU_AD104_RTX_4070 0x2786u
#define GU_AD104_RTX_4060_TI 0x2788u /* AD104 cut-down */
#define GU_AD106_RTX_4060_TI 0x2803u
#define GU_AD106_RTX_4060_TI_16GB 0x2805u
#define GU_AD106_RTX_4060 0x2808u /* AD106, not the AD107 0x2882 */
#define GU_AD106M_RTX_4070_LAPTOP 0x2820u
#define GU_AD106M_RTX_4070_MOBILE 0x2860u
#define GU_AD107_RTX_4060 0x2882u
#define GU_AD107_RTX_4060_LAPTOP 0x28a0u
#define GU_AD107_RTX_4050_LAPTOP 0x28a1u
#define GU_AD107_RTX_4060_MOBILE 0x28e0u
#define GU_AD107_RTX_4050_MOBILE 0x28e1u

/* Ada IDs admitted by default (no opt-in). */
#define GPU_UNLOCK_AD10X_SUPPORTED_IDS(X) \
	X(GU_AD102_RTX_4090)

/* Ada IDs admitted only with nvgd.tensor_unlock_experimental=1. */
#define GPU_UNLOCK_AD10X_EXPERIMENTAL_IDS(X) \
	X(GU_AD102_RTX_4090_D) \
	X(GU_AD102_RTX_4070_TI_SUPER) \
	X(GU_AD103_RTX_4080_SUPER) \
	X(GU_AD103_RTX_4080) \
	X(GU_AD103_RTX_4070_TI_SUPER) \
	X(GU_AD103_RTX_4070) \
	X(GU_AD103M_RTX_4090_LAPTOP) \
	X(GU_AD103M_RTX_4090_MOBILE) \
	X(GU_AD104_RTX_4070_TI) \
	X(GU_AD104_RTX_4070_SUPER) \
	X(GU_AD104_RTX_4070) \
	X(GU_AD104_RTX_4060_TI) \
	X(GU_AD106_RTX_4060_TI) \
	X(GU_AD106_RTX_4060_TI_16GB) \
	X(GU_AD106_RTX_4060) \
	X(GU_AD106M_RTX_4070_LAPTOP) \
	X(GU_AD106M_RTX_4070_MOBILE) \
	X(GU_AD107_RTX_4060) \
	X(GU_AD107_RTX_4060_LAPTOP) \
	X(GU_AD107_RTX_4050_LAPTOP) \
	X(GU_AD107_RTX_4060_MOBILE) \
	X(GU_AD107_RTX_4050_MOBILE)

/* Every Ada ID the unlock knows, supported or experimental. */
#define GPU_UNLOCK_AD10X_IDS(X) \
	GPU_UNLOCK_AD10X_SUPPORTED_IDS(X) \
	GPU_UNLOCK_AD10X_EXPERIMENTAL_IDS(X)

#endif
