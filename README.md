# WipeSure — Secure Data Wiping for Trustworthy IT Asset Recycling

Smart India Hackathon 2025 · Problem Statement 25070 · Ministry of Mines (JNARDDC)
Team WipeSure · Shortlisted at institution level

## Problem
India generates 1.75M+ tonnes of e-waste annually. A major driver: people and organizations
hoard old devices out of fear that deleted data can be recovered, instead of recycling them.
Existing wiping tools are complex, expensive, or don't prove erasure happened.

## What this repo contains
A working CLI tool (`src/penwipe.c`) that performs secure, NIST SP 800-88 Clear-level data
sanitization on Linux for removable USB drives:
- Detects connected removable drives
- Securely overwrites the entire device (shred, 3-pass + zero-fill; falls back to dd zero-fill)
- Reformats the device (FAT32 or exFAT) via parted + mkfs
- Verifies the wipe by inspecting raw sectors

## Status
| Feature | Status |
|---|---|
| Linux CLI wipe/format/verify | ✅ Implemented & tested on physical USB |
| Windows / Android support | 📐 Designed only |
| Digitally signed wipe certificates | 📐 Designed only |
| Web portal (`ui-prototype/`) | 🎨 Front-end demo, not connected to backend |

## Build & Run
\`\`\`bash
gcc -O2 -o penwipe src/penwipe.c
sudo ./penwipe list
sudo ./penwipe /dev/sdX wipe
sudo ./penwipe /dev/sdX format fat32
sudo ./penwipe /dev/sdX verify
\`\`\`
⚠️ Replace `/dev/sdX` with your actual device — this is irreversible. Use `list` first to
confirm the correct device. Test safely with a loop device — see docs/.

## Demo
- CLI run: see `demo/cli_start.png`, `demo/cli_end.png`
- Architecture: see `docs/architecture_diagram.png`
- Prototype video: [link]
- UI mockup (not wired to backend): [Lovable link]

## Standards referenced
- NIST SP 800-88 Rev. 1 — Guidelines for Media Sanitization

## Roadmap
- Windows (DeviceIoControl / IOCTL_DISK_SECURE_ERASE) and Android backend
- Digitally signed PDF/JSON wipe certificates
- ATA Secure Erase / NVMe Sanitize support for Purge-level SSD wiping
- Wire the front-end portal to the backend via a REST API

## Team
[names]
