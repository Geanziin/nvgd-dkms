// SPDX-License-Identifier: GPL-2.0-only
/*
 * NVGD Tensor Core Speed-Select Unlock
 * Unlocks full FP16/BF16/TF32 -> FP32 accumulation tensor throughput (2x rate)
 * on supported NVIDIA Ampere (GA102/GA104/GA106/GA107) and Ada (AD102) GPUs.
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

/* RTX 4090 and supported desktop GA102/GA104/GA106/GA107 GPU functions. */
static const unsigned short supported_ids[] = {
	/* AD102: GeForce RTX 4090 */
	0x2684,
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

static const unsigned short experimental_ids[] = {
	GU_AD102_RTX_4090_D,
	GU_AD102_RTX_4070_TI_SUPER,
	GU_AD103_RTX_4080_SUPER,
	GU_AD103_RTX_4080,
	GU_AD103_RTX_4070_TI_SUPER,
	GU_AD103_RTX_4070,
	GU_AD103M_RTX_4090_MOBILE,
	GU_AD104_RTX_4070_TI,
	GU_AD104_RTX_4070_SUPER,
	GU_AD104_RTX_4070,
	GU_AD106_RTX_4060_TI,
	GU_AD106_RTX_4060_TI_16GB,
	GU_AD107_RTX_4060,
	GU_AD107_RTX_4060_LAPTOP,
	GU_AD107_RTX_4060_MOBILE,
	GU_AD107_RTX_4050_MOBILE,
};

static bool is_supported_gpu(struct pci_dev *pdev)
{
	unsigned int i;

	if (pdev->vendor != PCI_VENDOR_ID_NVIDIA)
		return false;

	for (i = 0; i < ARRAY_SIZE(supported_ids); i++) {
		if (pdev->device == supported_ids[i])
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

int nvgd_tensor_unlock(struct pci_dev *pdev)
{
	struct gpu_unlock_610_shared shared = { 0 };
	void __iomem *bar0;
	u32 plm = 0, ss0 = 0, ss1 = 0, readout = 0;
	u16 cmd = 0;
	bool mem_enabled = false;
	int ret;

	if (!nvgd_tensor_unlock_enabled)
		return 0;

	if (!is_supported_gpu(pdev))
		return 0;

	/* Early check: Is the GPU already unlocked?
	 * We briefly map the feature registers at BAR0 + 0x00823800 and inspect SS0 and SS1.
	 */
	pci_read_config_word(pdev, PCI_COMMAND, &cmd);
	if (!(cmd & PCI_COMMAND_MEMORY)) {
		pci_write_config_word(pdev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
		mem_enabled = true;
	}

	bar0 = ioremap(pci_resource_start(pdev, 0) + 0x00823800, 0x100);
	if (bar0) {
		plm = readl(bar0 + (GU_FEAT_PLM_OFFSET - 0x00823800));
		readout = readl(bar0 + (GU_FEAT_READOUT_1_OFFSET - 0x00823800));
		ss0 = readl(bar0 + (GU_FEAT_SS0_OFFSET - 0x00823800));
		ss1 = readl(bar0 + (GU_FEAT_SS1_OFFSET - 0x00823800));
		iounmap(bar0);
	}

	if (mem_enabled)
		pci_write_config_word(pdev, PCI_COMMAND, cmd);

	/* If SS0 and SS1 are already set to 0x88888888 and 0x8, or PLM is fully open,
	 * the GPU tensor cores are already at maximum speed select.
	 */
	if ((ss0 == GU_SS0_UNLOCK && (ss1 & 0xf) == GU_SS1_UNLOCK) || plm == ~0u) {
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
