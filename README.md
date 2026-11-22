# nvgd-dkms: Next-Gen NVIDIA Kernel Driver

[![License](https://img.shields.io/badge/License-GPL%202.0%20%2F%20MIT-blue.svg)](LICENSE)
[![Kernel](https://img.shields.io/badge/Linux%20Kernel-%3E%3D%207.2-brightgreen.svg)](https://kernel.org)
[![GSP Version](https://img.shields.io/badge/GSP--RM-615.78.08-orange.svg)](https://github.com/NVIDIA/open-gpu-kernel-modules)

**nvgd** é um driver de kernel Linux avançado para GPUs NVIDIA baseado no Nouveau/NVKM, reestruturado para alto desempenho em jogos, computação e inteligência artificial nas arquiteturas **Turing**, **Ampere** e **Ada Lovelace** (GTX 16xx, RTX 20/30/40).

---

## 🚀 Principais Recursos

### 1. Desbloqueio Nativo dos Tensor Cores (Tensor Core Speed-Select Unlock)
Remove a restrição artificial de fábrica da NVIDIA (Speed-Select) em chips de consumo através do SEC2 Falcon V67 booter integrado diretamente na inicialização do driver:
- **Throughput Dobrado (2× Rate, inferência de registrador)** em operações de acumulação FP32 (`HMMA` e `IMMA`: FP16 → FP32, BF16 → FP32 e TF32 → FP32). O "2×" é inferido dos registradores (SS0 todo em 0x8 = taxa máxima), não throughput medido: não existe no lab nenhum benchmark HMMA/IMMA antes/depois, em nenhuma arquitetura.
- **Sem Regressão**: INT8, dp4a e ponto flutuante escalar permanecem estáveis na taxa máxima.
- **Detecção por registradores**: se a placa já estiver desbloqueada, o probe detecta e pula. Comportamento em boot/reload: warm boot com estado persistido pula; com estado resetado o unlock re-executa (seguro, WPR2 baixo); reload após inicialização por um driver sem desbloqueio recusa com `-EBUSY` ("power off first") até um power-off completo; reload após unlock pula. Suspend/resume pode perder o estado (não validado em Ada) e o unlock NÃO é reaplicado no resume.
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

### 3. Resultados de Performance (RTX 3050 6GB GA107)
- **Vulkan Nativo (NVK/NAK)**: `vkmark` — corridas armazenadas: 2026-10-22 mediana **14076** (14076 / 13401 / 14090); 2026-10-23 mediana **~15510** (15510 / 15549 / 15497). Logs: `src/mesa/docs/benchmarks/vulkan-2026-10-22/README.md`, `nvgd/run-{1,2,3}.log`. **Essas corridas antecedem o desbloqueio** (commit bedd688fd, 27/out; usaram `nvgd-dkms 1.0.0-20`) e **não evidenciam efeito do desbloqueio**. O valor "18004" divulgado antes não aparece em nenhum log e foi removido.
- **OpenGL 4.6 (nvc0/NAK)**: sem resultado de `glmark2` armazenado no lab — o "9067" divulgado antes não tem log e foi removido.
- **Emulação Android (Redroid Container)**: sem log ou artefato no lab — o "240 FPS" divulgado antes foi removido.

---

## 🛠️ GPUs Suportadas

O desbloqueio de Tensor Cores é aplicado às placas abaixo. **Validado** aqui
significa: readout de registradores registrado no lab. **Implementado, não
validado** significa: o código existe e roteia a placa, mas nenhum artefato
de validação em hardware existe no lab.

- **Ampere — validadas**:
  - GeForce RTX 3050 6GB (GA107 `10de:2584`) — readout FEAT/SS0/SS1 no lab.
  - GeForce RTX 3060 12GB (GA106 `10de:2504`) — carimbo de validação no
    código e na mensagem do commit bedd688fd; sem log no lab.
- **Ampere — implementadas, não validadas**:
  - Chips desktop GA102 / GA104 (no código, sem artefato de validação).
  - GA103 `10de:2414` (mobile, no código).
- **Ada Lovelace — implementada, não validada**:
  - GeForce RTX 4090 (AD102 `10de:2684`) — em `supported_ids[]`, ativa por
    padrão, mas **nenhum** log de boot, readout ou benchmark em hardware
    Ada existe no lab. Não chamar de validada até existir o artefato.

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

Ou diretamente no log do kernel (persistente; o anel do `dmesg` rola e fica
vazio dias após o boot):
```bash
journalctl -k | grep -i "tensor"
```

Saída esperada:
```text
nvgd 0000:03:00.0: nvgd: initiating tensor core speed-select unlock (current SS0=0x50404070 SS1=0x6 readout=0x00400080 PLM=0xffffff8f)...
nvgd 0000:03:00.0: nvgd: tensor cores unlocked successfully! (2x FP32 accumulate tensor rate active)
```
