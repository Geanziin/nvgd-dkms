// SPDX-License-Identifier: GPL-2.0-only
/*
 * NVGD Tensor Core Speed-Select Unlock
 * Unlocks full FP16/BF16/TF32 -> FP32 accumulation tensor throughput (2x rate)
 * on supported NVIDIA Ampere (GA102/GA104/GA106/GA107) and Ada
 * (AD102/AD103/AD104/AD106/AD107) GPUs. Ada is supported by default only
 * for the RTX 4090 (AD102 0x2684); all other Ada IDs are experimental
 * (nvgd.tensor_unlock_experimental=1) and unvalidated on lab hardware.
 */
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/firmware.h>

#include "tensor_unlock.h"
#include "unlock_610.h"
#include "arch_610.h"
#include "gfw_eval.h"

extern int nvgd_tensor_unlock_enabled;
extern int nvgd_tensor_unlock_experimental;

/* RTX 4090 and supported desktop GA102/GA104/GA106/GA107 GPU functions.
 * Ampere IDs are listed here; Ada IDs come from the single source in
 * arch_610.h (GPU_UNLOCK_AD10X_SUPPORTED_IDS / _EXPERIMENTAL_IDS). */
static const unsigned short supported_ids[] = {
	/* GA102 */
	0x2200, 0x2203, 0x2204, 0x2205, 0x2206, 0x2207, 0x2208, 0x220a,
	0x220d, 0x2216, 0x222b, 0x222f,
	/* GA104 */
	0x2480, 0x2482, 0x2483, 0x2484, 0x2486, 0x2487, 0x2488, 0x2489,
	0x248a, 0x248c, 0x248d, 0x248e, 0x249c, 0x249d, 0x249f, 0x24a0,
	0x24a4, 0x24ac, 0x24ad, 0x24af, 0x24bf, 0x24c0, 0x24c7, 0x24c8,
	0x24c9, 0x24dc, 0x24dd, 0x24df, 0x24e0,
	/* GA103 */
	0x2414,
	/* GA106 (validated on RTX 3060 12GB 0x2504) */
	0x2501, 0x2503, 0x2504, 0x2505, 0x2507, 0x2508, 0x2509, 0x252f,
	0x2544,
	/* GA107 (validated on RTX 3050 6GB 0x2584) */
	0x2584,
};

/* Ada IDs, expanded from the single source in arch_610.h so admission
 * and AD102-family asset routing (unlock_610.c: ad102_family()) can
 * never desync. */
#define GU_AD10X_ID(id) id,
static const unsigned short supported_ada_ids[] = {
	GPU_UNLOCK_AD10X_SUPPORTED_IDS(GU_AD10X_ID)
};

static const unsigned short experimental_ids[] = {
	GPU_UNLOCK_AD10X_EXPERIMENTAL_IDS(GU_AD10X_ID)
};
#undef GU_AD10X_ID

static bool is_supported_gpu(struct pci_dev *pdev)
{
	unsigned int i;

	if (pdev->vendor != PCI_VENDOR_ID_NVIDIA)
		return false;

	for (i = 0; i < ARRAY_SIZE(supported_ids); i++) {
		if (pdev->device == supported_ids[i])
			return true;
	}

	for (i = 0; i < ARRAY_SIZE(supported_ada_ids); i++) {
		if (pdev->device == supported_ada_ids[i])
			return true;
	}

	if (nvgd_tensor_unlock_experimental) {
		for (i = 0; i < ARRAY_SIZE(experimental_ids); i++) {
			if (pdev->device == experimental_ids[i])
				return true;
		}
	}

	return false;
}

/* Known Ada ID that is experimental-gated, for the refusal log. */
static bool is_experimental_gpu(struct pci_dev *pdev)
{
	unsigned int i;

	if (pdev->vendor != PCI_VENDOR_ID_NVIDIA)
		return false;

	for (i = 0; i < ARRAY_SIZE(experimental_ids); i++) {
		if (pdev->device == experimental_ids[i])
			return true;
	}

	return false;
}

/* Read the four FEAT speed-select words (PLM, readout, SS0, SS1) at
 * BAR0 + 0x00823800. Shared by the probe early check and the post-resume
 * status report; read-only, safe to call while the driver is bound. */
static void read_feat_state(struct pci_dev *pdev, u32 *plm, u32 *ss0, u32 *ss1, u32 *readout)
{
	void __iomem *bar0;
	u16 cmd = 0;
	bool mem_enabled = false;

	*plm = *ss0 = *ss1 = *readout = 0;

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MEMORY)) {
		pci_write_config_word(pdev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
		mem_enabled = true;
	}

	bar0 = ioremap(pci_resource_start(pdev, 0) + 0x00823800, 0x100);
	if (bar0) {
		*plm = readl(bar0 + (GU_FEAT_PLM_OFFSET - 0x00823800));
		*readout = readl(bar0 + (GU_FEAT_READOUT_1_OFFSET - 0x00823800));
		*ss0 = readl(bar0 + (GU_FEAT_SS0_OFFSET - 0x00823800));
		*ss1 = readl(bar0 + (GU_FEAT_SS1_OFFSET - 0x00823800));
		iounmap(bar0);
	}

	if (mem_enabled)
		pci_write_config_word(pdev, PCI_COMMAND, cmd);
}

/* SS0/SS1 at maximum speed select, or PLM fully open.
 * All-ones in every word is the inaccessible-BAR signature (device powered
 * down, e.g. runtime-suspended), NOT an unlocked GPU: the unlock writes
 * SS0=0x88888888, SS1=0x8, never all-ones. Without this guard a suspended
 * GPU would read back as unlocked (PLM == ~0u). */
static bool feat_unlocked(u32 plm, u32 ss0, u32 ss1)
{
	if (plm == ~0u && ss0 == ~0u && ss1 == ~0u)
		return false;
	return (ss0 == GU_SS0_UNLOCK && (ss1 & 0xf) == GU_SS1_UNLOCK) || plm == ~0u;
}

int nvgd_tensor_unlock(struct pci_dev *pdev)
{
	struct gpu_unlock_610_shared shared = { 0 };
	u32 plm = 0, ss0 = 0, ss1 = 0, readout = 0;
	int ret;

	if (!nvgd_tensor_unlock_enabled)
		return 0;

	if (!is_supported_gpu(pdev)) {
		if (is_experimental_gpu(pdev))
			pci_info(pdev, "nvgd: tensor unlock not applied: device %#06x is experimental, enable with nvgd.tensor_unlock_experimental=1\n",
				 pdev->device);
		else
			pci_info(pdev, "nvgd: tensor unlock not supported on device %#06x, continuing with stock speed\n",
				 pdev->device);
		return 0;
	}

	/* Early check: is the GPU already unlocked? */
	read_feat_state(pdev, &plm, &ss0, &ss1, &readout);

	/* If SS0 and SS1 are already set to 0x88888888 and 0x8, or PLM is fully open,
	 * the GPU tensor cores are already at maximum speed select.
	 */
	if (feat_unlocked(plm, ss0, ss1)) {
		pci_info(pdev, "nvgd: tensor cores already unlocked (SS0=%#010x SS1=%#x readout=%#010x PLM=%#010x), full speed enabled\n",
			 ss0, ss1, readout, plm);
		return 0;
	}

	/* Check if shared GSP firmware exists before attempting unlock transaction */
	ret = gpu_unlock_610_load_shared(pdev, &shared);
	if (ret) {
		pci_info(pdev, "nvgd: tensor unlock firmware not available in /lib/firmware/gpu-unlock/610.43.03/ (%d), continuing with stock speed\n", ret);
		return 0;
	}

	pci_info(pdev, "nvgd: initiating tensor core speed-select unlock (current SS0=%#010x SS1=%#x readout=%#010x PLM=%#010x)...\n",
		 ss0, ss1, readout, plm);

	ret = gpu_unlock_610_unlock(pdev, &shared);
	gpu_unlock_610_release_shared(&shared);

	if (ret == -EALREADY) {
		pci_info(pdev, "nvgd: tensor cores already unlocked, skipping\n");
	} else if (ret) {
		pci_warn(pdev, "nvgd: tensor core unlock transaction returned %d, continuing with stock driver\n", ret);
	} else {
		pci_info(pdev, "nvgd: tensor cores unlocked successfully! (2x FP32 accumulate tensor rate active)\n");
	}

	return 0;
}

/* Post-resume unlock state report.
 *
 * Suspend/resume (D3hot) powers the chip down; whether the FEAT
 * speed-select state survives is hardware-dependent and UNVALIDATED on
 * any Ada GPU (PENDENTE DE HARDWARE). The unlock transaction is NOT
 * re-run here: it requires a cold, unbound GPU (gpu_unlock_610_unlock),
 * and re-running FLR + SEC2 under a bound, resumed driver is untested
 * and could wedge resume. So resume only REPORTS the state — a lost
 * unlock needs a full power cycle to come back (and, if the card was
 * re-initialised by a driver without the unlock in the meantime, the
 * -EBUSY "power off first" rule applies). Read-only: safe while the
 * driver is bound. */
void nvgd_tensor_unlock_status(struct pci_dev *pdev)
{
	u32 plm = 0, ss0 = 0, ss1 = 0, readout = 0;

	if (!nvgd_tensor_unlock_enabled || !is_supported_gpu(pdev))
		return;

	read_feat_state(pdev, &plm, &ss0, &ss1, &readout);
	if (feat_unlocked(plm, ss0, ss1))
		pci_info(pdev, "nvgd: tensor unlock still active after resume (SS0=%#010x SS1=%#x readout=%#010x PLM=%#010x)\n",
			 ss0, ss1, readout, plm);
	else
		pci_warn(pdev, "nvgd: tensor unlock LOST on suspend/resume (SS0=%#010x SS1=%#x readout=%#010x PLM=%#010x) — stock speed until next power cycle; the unlock is not re-applied at resume\n",
			 ss0, ss1, readout, plm);
}

/* Current FEAT speed-select state, for sysfs/telemetry (read-only BAR
 * reads; safe while the driver is bound). On a powered-down device the
 * BAR reads all-ones and feat_unlocked() reports false. */
void nvgd_tensor_unlock_read_state(struct pci_dev *pdev,
				   struct nvgd_tensor_unlock_state *state)
{
	state->supported = is_supported_gpu(pdev);
	read_feat_state(pdev, &state->plm, &state->ss0, &state->ss1, &state->readout);
	state->unlocked = feat_unlocked(state->plm, state->ss0, state->ss1);
}
