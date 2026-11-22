/* SPDX-License-Identifier: MIT */
#ifndef __NVGD_TENSOR_UNLOCK_H__
#define __NVGD_TENSOR_UNLOCK_H__

struct pci_dev;

int nvgd_tensor_unlock(struct pci_dev *pdev);

/* Read-only post-resume unlock state report (logs whether the unlock
 * survived suspend; never re-applies the transaction). */
void nvgd_tensor_unlock_status(struct pci_dev *pdev);

/* Current FEAT speed-select state, for sysfs/telemetry (read-only BAR
 * reads; safe while the driver is bound). */
struct nvgd_tensor_unlock_state {
	bool supported; /* device is in the supported/experimental lists */
	bool unlocked;  /* SS0/SS1 at maximum speed select, or PLM fully open */
	u32 plm, ss0, ss1, readout;
};

void nvgd_tensor_unlock_read_state(struct pci_dev *pdev,
				   struct nvgd_tensor_unlock_state *state);

#endif /* __NVGD_TENSOR_UNLOCK_H__ */
