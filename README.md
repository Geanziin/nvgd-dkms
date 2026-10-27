# nvgd-dkms: Next-Gen NVIDIA Kernel Driver

[![License](https://img.shields.io/badge/License-GPL%202.0%20%2F%20MIT-blue.svg)](LICENSE)
[![Kernel](https://img.shields.io/badge/Linux%20Kernel-%3E%3D%207.2-brightgreen.svg)](https://kernel.org)
[![GSP Version](https://img.shields.io/badge/GSP--RM-615.78.08-orange.svg)](https://github.com/NVIDIA/open-gpu-kernel-modules)

**nvgd** é um driver de kernel Linux avançado para GPUs NVIDIA baseado no Nouveau/NVKM, reestruturado para alto desempenho em jogos, computação e inteligência artificial nas arquiteturas **Turing**, **Ampere** e **Ada Lovelace** (GTX 16xx, RTX 20/30/40).

---

## 🚀 Principais Recursos

### 1. Desbloqueio Nativo dos Tensor Cores (Tensor Core Speed-Select Unlock)
Remove a restrição artificial de fábrica da NVIDIA (Speed-Select) em chips de consumo através do SEC2 Falcon V67 booter integrado diretamente na inicialização do driver:
- **Throughput Dobrado (2× Rate)** em operações de acumulação FP32 (`HMMA` e `IMMA`: FP16 → FP32, BF16 → FP32 e TF32 → FP32).
- **Sem Regressão**: INT8, dp4a e ponto flutuante escalar permanecem estáveis na taxa máxima.
- **Idempotência Instantânea**: Detecção imediata por registradores de hardware se a placa já estiver desbloqueada (zero overhead em warm boots/reloads).
- **Fallback Suave**: Caso os microcódigos de desbloqueio em `/usr/lib/firmware/gpu-unlock/610.43.03/` não estejam presentes, o driver inicializa o GSP normalmente sem falhar o probe.

#### Evidência de Hardware (Registradores no Silício GA107 `10de:2584`):
| Registrador | Nome | Antes (Travado) | Depois (Desbloqueado) |
|---|---|---|---|
| `0x00823804` | `FEAT_OVR_PLM` | `0xffffff8f` (Travado) | `0xffffffff` (Destravado) |
| `0x00823818` | `FEAT_READOUT_1` | `0x00400080` (50% Throttle) | `0x00000000` (Taxa Máxima) |
| `0x0082381c` | `FEAT_OVR_SM_SPEED_SELECT` | `0x50404070` | `0x88888888` |
| `0x00823820` | `FEAT_OVR_SM_SPEED_SELECT_1` | `0x00000006` | `0x00000008` |

### 2. GSP-RM 615 com P-State P0 Boost
- Inicialização direta do firmware GSP `615.78.08` com `sec2-bl-615.78.08.bin`.
- Chaveamento forçado para estado de energia de alta performance (P0).
- Redução de latência de RPC e telemetria de clocks/energia em tempo real via `hwmon`.

### 3. Resultados de Performance Validados (RTX 3050 6GB GA107)
- **Vulkan Nativo (NVK/NAK)**: `vkmark Score: 18004` (picos de 22.934 FPS em geometria 3D e 20.555 FPS em filtros de convolução 2D).
- **OpenGL 4.6 Nativo (nvc0/NAK)**: `glmark2 Score: 9067` (13.577 FPS com VBO).
- **Emulação Android (Redroid Container)**: **240 FPS** contínuos via render node nativo.

---

## 🛠️ GPUs Suportadas

O desbloqueio de Tensor Cores é aplicado exclusivamente às placas validadas:
- **Ampere**:
  - GeForce RTX 3050 6GB (GA107 `10de:2584`)
  - GeForce RTX 3060 12GB (GA106 `10de:2504`)
  - Chips desktop GA102 / GA104
- **Ada Lovelace**:
  - GeForce RTX 4090 (AD102 `10de:2684`)

Chips Ada experimentais podem ser habilitados opcionalmente via parâmetro de módulo.

---

## ⚙️ Parâmetros do Módulo (`/etc/modprobe.d/nvgd.conf`)

```ini
# Habilita o desbloqueio de Tensor Cores para GPUs suportadas (padrão: 1 = ativo)
options nvgd tensor_unlock=1

# Habilita desbloqueio experimental para chips Ada não testados (padrão: 0 = desativado)
options nvgd tensor_unlock_experimental=0
```

---

## 📦 Instalação

### Arch Linux / CachyOS (Repositório Oficial nvgd)

Adicione ao `/etc/pacman.conf` antes dos outros repositórios:
```ini
[nvgd]
SigLevel = Optional TrustAll
Server = https://geanziin.github.io/nvgd-repo/$arch
```

Instale via pacman:
```bash
sudo pacman -Syu nvgd-dkms
```

### Gentoo Linux (Overlay Portage)

Configure o repositório em `/etc/portage/repos.conf/nvgd.conf`:
```ini
[nvgd]
location = /var/db/repos/nvgd
sync-type = git
sync-uri = https://github.com/Geanziin/nvgd-repo.git
auto-sync = yes
```

Sincronize e compile:
```bash
emaint sync -r nvgd
emerge --ask x11-drivers/nvgd-dkms::nvgd
```

---

## 🔍 Diagnóstico e Verificação

Após o boot, verifique o status do driver com:
```bash
nvgd-status
```

Ou diretamente no log do kernel:
```bash
dmesg | grep -i "tensor"
```

Saída esperada:
```text
nvgd 0000:03:00.0: nvgd: initiating tensor core speed-select unlock (current SS0=0x50404070 SS1=0x6 readout=0x00400080 PLM=0xffffff8f)...
nvgd 0000:03:00.0: nvgd: tensor cores unlocked successfully! (2x FP32 accumulate tensor rate active)
```
