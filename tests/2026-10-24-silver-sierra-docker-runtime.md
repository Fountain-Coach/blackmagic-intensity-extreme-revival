# Sierra-hosted Linux Docker and Fountain Video acceptance — 2026-10-24

## Decision and unchanged physical ownership

Silver Macmini5,3 boots Sierra 10.12.6; Blackmagic Intensity Extreme remains on Sierra's Thunderbolt 1 through the Comdis THUNDERDOCK2 (PCI 0xBDBD:0xA124). Existing original Desktop Video 10.4 KEXT stays loaded, independent KEXT unloaded. Nothing here proves working physical HDMI capture, a Linux driver owning the PCI function, hardware passthrough, or a repaired legacy frame lifetime.

User chose explicitly: **do not install Ubuntu as silver's host operating system**. Linux/Docker runs as a guest while Sierra owns the physical device.

## Actual hardware and reproducible offline VM

- Machine: 2011 Macmini5,3, Intel Core i7, 16 GiB RAM, Sierra 10.12.6; Apple Hypervisor support sysctl kern.hv_support=1.
- Original Data boot drive retained. Fresh second drive contains preexisting user files and was neither erased nor repartitioned.
- Original xhyve source machyve/xhyve commit `dfbe09b9db0ef9384c993db8e72fb3e96f376e7b` compiled successfully with Sierra Xcode 9.2; executable operates through Apple's built-in Hypervisor and vmnet frameworks. No third-party host kernel module was installed.
- Old TinyCore 3.16.6 kernel booted in VM and exposed interactive serial shell; guest shut down cleanly.
- Official Alpine 3.24.3 virtual ISO SHA-256 `cdc1bc0efc64d8d611bf3733f54868172a545888dfd726f518bf279bcb0ff1af`, verified before delivery. Linux 6.18.58-0-virt booted interactively from read-only virtual ISO; two guest vCPUs and 2GiB RAM. No host disk passed through.
- Dedicated Fresh path `/Volumes/Fresh/FountainVideoDockerLab/` contains Alpine ISO and new virtual guest file `docker-guest.raw`, 12GiB, allocated as a user-owned file (not an OS partition). Guest sees virtual disk `/dev/vda` and ISO `/dev/sr0`. Formatted ONLY `/dev/vda` as ext4 inside guest and mounted at `/mnt/fc-docker-state`.
- Official Docker 29.9.2 Linux x86_64 static archive downloaded from download.docker.com, SHA-256 `9ac225999fd5e863f7694c728ef23d480616b1e5639328fa309befd4243d69eb`. Binaries copied from offline virtual CD onto the ext4 guest disk. No Docker Desktop, Sierra OS change or Internet access from guest.
- Docker Engine 29.9.2 started with `vfs` driver, `--bridge=none --iptables=false --ip6tables=false --ip-forward=false --ip-masq=false`, cgroup2 mounted in guest. Offline `hello-world:latest` imported from trusted Ubuntu Docker export; `docker run --rm --network none hello-world:latest` returned **Hello from Docker!**.
- Guest was fully shut down and rebooted from Alpine read-only media; ext4 guest disk retained Docker binaries, engine state and the hello-world image; Docker 29.9.2 restarted with root `/mnt/fc-docker-state/engine`. This witnesses persistence independent of Sierra's filesystems.

## Actual Fountain Video scratch service in Docker

- The independently built `fountain-video-scratch-scratch:latest` Linux image, already working on separate Ubuntu host (port 8373) was exported there, transferred via offline ISO, and imported on the silver Linux guest.
- The image was started in the guest as container `fc-scratch-guest`, `--network none`, read-only root filesystem, capabilities dropped, no-new-privileges, 256 MiB cap, UID 1000, token file private and bound read-only from guest ext4. The guest has no network adapter, so the server is strictly inaccessible from Sierra or the LAN at this step.
- Inside this real running container, HTTP `GET http://127.0.0.1:8273/api/scratch/state` succeeded, reporting zero frames and `hardware_hdmi_capture_accepted=false`.
- `python3 -m unittest simulator.tests.test_scratch_server -q` ran **18/18 tests OK** inside the actual container. Tests include HTTP authenticated synthetic JPEG ingest, buffer eviction, source provenance and reading a stored frame. There was **no real HDMI frame**.
- The original Ubuntu host's production-like bare-metal scratch server on port 8273 and separately tested Docker staging server on loopback 8373 remain separate; no ports stolen or changed on Ubuntu.

## Network boundary — independently tested blocker

- xhyve's `-s 2:0,virtio-net` vmnet device was refused when run as unprivileged Sierra account: **Could not create vmnet interface, permission denied or no entitlement?**
- This host has no passwordless sudo and `security find-identity -v -p codesigning` reported zero valid signing identities.
- A copied xhyve binary ad-hoc signed with a `com.apple.vm.networking` entitlement passed static codesign verification but **failed at runtime**, macOS `EXC_CRASH (Code Signature Invalid)`, termination namespace CODESIGNING. This is NOT a viable network solution.
- We did **not** disable SIP, grant an improvised root service, install a system-wide networking module, or weaken existing security controls.
- Enabling Sierra-to-guest delivery requires properly authorized vmnet networking, a supported VM setup with an equivalent guest networking mechanism, or a separate explicit data gateway. Running the Docker service in an isolated guest **does not** transfer PCI/DMA/IRQ ownership from the Sierra kernel.
- Official references: https://github.com/machyve/xhyve (vmnet signing) and https://developer.apple.com/documentation/vmnet; Docker static binary docs https://docs.docker.com/engine/install/binaries.

## Safety and lifecycle

- `scripts/silver-video-docker-guest.sh --check` validates the correct Sierra host and complete media without starting a VM. `--run` explicitly enters offline guest console; refuses a second concurrent xhyve instance. No autostart or background task is installed.
- Boot console is root without a password on the entirely offline install/test medium. Never expose this console or grant public guest networking as configured.
- For Docker on subsequent boots, guest operator mounts `/dev/vda` at `/mnt/fc-docker-state`, then launches `sh /mnt/fc-docker-state/start-daemon.sh` (script mounted from guest disk) and uses `PATH=/mnt/fc-docker-state/docker-bin:$PATH`.
- Exit with guest `poweroff` from serial, then verify xhyve exits; keep original Sierra as boot OS.
- The actual capture acceptance remains: a physically unprotected external HDMI source, confirmed receiver lock, reproducible valid (not `no source`) pixels and timestamps, safe vendor-frame lifetime or independent hardware driver, and delivered frame in Docker. *These physical gates are not yet satisfied.*
