# Research Notes — Secure Data Wiping & Digital Forensics

Background research compiled for WipeSure (SIH 2025, Problem Statement 25070). This document
summarizes the forensic and technical context that motivated the design.

---

## 1. Why "Delete" Isn't Enough

| Action                      | What actually happens                                                                                                                                          | Recoverable?                          |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------- |
| **Delete**                  | OS removes the file's entry from the filesystem index (FAT table / NTFS MFT / ext journal) and marks the space free. The data blocks themselves are untouched. | Yes, until overwritten                |
| **Quick Format**            | Resets the filesystem's index/table structures. Data blocks remain.                                                                                            | Yes, until overwritten                |
| **Full Format**             | May overwrite sectors with zeros depending on OS/tool.                                                                                                         | Harder, depends on implementation     |
| **Secure Wipe** (overwrite) | Actual data bytes are overwritten one or more times.                                                                                                           | Effectively no, on conventional media |
| **Crypto-erase**            | Encryption key is destroyed; data becomes unrecoverable ciphertext.                                                                                            | No, unless the key leaked elsewhere   |

This is the core justification for the project: most "delete" and "format" operations only erase
the _index_, not the _data_, and the gap between those two is exactly what data-recovery and
forensic tools exploit.

---

## 2. Storage Media & Their Wiping Considerations

### Hard Disk Drives (HDD)

- Magnetic platters, data addressed directly — no controller-level remapping between logical and
  physical location.
- A straightforward overwrite (single or multi-pass) reliably destroys data because the logical
  address you write to _is_ the physical location.
- Secure deletion: multi-pass overwrite, degaussing, or physical destruction.

### Solid-State Drives (SSD)

- NAND flash with a **Flash Translation Layer (FTL)** and **wear-leveling**: the controller, not
  the OS, decides which physical cell a logical address maps to, and remaps it over time to
  spread wear.
- Consequence: overwriting "the same address" repeatedly does **not** guarantee every physical
  cell that ever held that data gets touched — remnants can persist in **over-provisioned** or
  remapped blocks invisible to the OS.
- **TRIM** tells the SSD which blocks are no longer in use so the controller can erase them during
  garbage collection — but TRIM is about performance/wear management, not a certified secure-erase
  guarantee.
- Correct approach: the drive's own **ATA Secure Erase** / **NVMe Sanitize/Format** command, which
  instructs the controller to erase all flash blocks including spare/over-provisioned area — or
  **crypto-erase** if the drive supports hardware encryption.

### USB Flash Drives / Memory Cards

- Also NAND flash, but typically simpler controllers than SSDs and usually **no TRIM support**.
- Because there's less aggressive remapping, a full-device overwrite is generally accepted as
  effective — this is the case `penwipe.c` targets.
- Still susceptible to wear and corruption; data can linger longer than on SSDs precisely because
  TRIM-style cleanup isn't happening.

---

## 3. NIST SP 800-88 — Media Sanitization Levels

| Level       | Definition                                                                                                          | Example technique                                         |
| ----------- | ------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------- |
| **Clear**   | Logical techniques applied via standard read/write commands, protecting against simple, non-invasive data recovery. | Single/multi-pass overwrite (`shred`, `dd`)               |
| **Purge**   | Techniques that render data infeasible to recover even with state-of-the-art laboratory methods.                    | ATA Secure Erase, NVMe Sanitize, degaussing, crypto-erase |
| **Destroy** | Physical destruction of the media.                                                                                  | Shredding, incineration, disintegration                   |

**`penwipe.c` implements Clear-level sanitization.** Purge-level support (hardware secure-erase
commands) was researched but not implemented in the working prototype.

---

## 4. File Carving (why "just deleting" is recoverable)

File carving recovers files from raw disk/memory **without relying on filesystem metadata**,
using known file signatures (header/footer byte patterns):

- JPEG: starts `FF D8`
- PDF: starts `%PDF`

This works even when the filesystem index is gone (after deletion or quick format), because the
raw data blocks are still physically present — carving tools just scan byte-by-byte for
recognizable patterns. This is exactly the attack a secure wipe needs to defeat: it's not enough
to erase the index, the underlying bytes have to be destroyed too.

**Recovery scenarios:**
| Scenario | Recoverable? |
|---|---|
| Normal delete (metadata only removed) | Yes, easily |
| Metadata overwritten, data blocks untouched | Yes, via carving |
| Crypto-erase only (key destroyed) | No — data is ciphertext |
| Crypto-erase + metadata overwrite | No |
| SSD over-provisioned/remapped blocks | Possibly, even after overwrite (lab-level only) |

---

## 5. OS-Level APIs for Secure Wiping (research reference)

### Linux

| Call                                      | Purpose                                                            |
| ----------------------------------------- | ------------------------------------------------------------------ |
| `open()` / `write()` / `pwrite()`         | Open a device or file and overwrite with zero/random data          |
| `ioctl()` with `BLKSECDISCARD`            | ATA Secure Erase at the block-device level                         |
| `ioctl()` with `BLKDISCARD`               | TRIM/discard blocks (SSD)                                          |
| `ioctl()` with `BLKZEROOUT`               | Zero-fill a specified range                                        |
| `fallocate()` with `FALLOC_FL_PUNCH_HOLE` | Deallocate file blocks (file-level TRIM)                           |
| `fsync()`                                 | Ensure overwrites are flushed to physical media, not just OS cache |
| `dm-crypt` / LUKS key destruction         | Crypto-erase via the kernel crypto API                             |

CLI equivalents: `shred`, `dd if=/dev/zero`/`if=/dev/urandom`, `blkdiscard`, `cryptsetup luksErase`,
`hdparm --security-erase` (SATA secure erase), `nvme format -s 1` (NVMe sanitize).

`penwipe.c` uses the CLI layer (`shred`, `dd`, `parted`, `mkfs`) rather than calling these syscalls
directly — a more advanced version would call `ioctl()` (`BLKSECDISCARD`/`BLKZEROOUT`) directly for
finer control and to avoid going through a shell.

### Windows (WinAPI) — for reference, not implemented

| Call                                                           | Purpose                                                              |
| -------------------------------------------------------------- | -------------------------------------------------------------------- |
| `CreateFile`                                                   | Open a handle to a file or raw physical drive (`\\.\PhysicalDriveN`) |
| `WriteFile`                                                    | Overwrite file content or raw sectors                                |
| `DeviceIoControl` + `IOCTL_DISK_SECURE_ERASE`                  | ATA Secure Erase, if supported by drive firmware                     |
| `DeviceIoControl` + `IOCTL_STORAGE_MANAGE_DATA_SET_ATTRIBUTES` | TRIM for SSDs                                                        |
| `FSCTL_SET_ZERO_DATA`                                          | Zero a range of a file (handles slack space)                         |
| BitLocker management APIs                                      | Crypto-erase by destroying the encryption key                        |

Manual equivalent (DiskPart): `clean` → `create partition primary` → `format fs=ntfs quick` —
note "quick" format here doesn't overwrite data; `clean` removes partition info, not the data
itself, unless paired with an explicit wipe step.

---

## 6. Verification

- **Hash check**: compute a hash (e.g., `sha256sum /dev/sdX`) before/after to compare state.
- **Raw inspection**: `dd` a few sectors off the raw device and pipe to `hexdump -C` to visually
  confirm an all-zero (or expected) pattern — this is what `penwipe.c`'s `verify_device()` does.
- **Forensic confirmation** (not implemented here): running a tool like Autopsy/FTK Imager against
  the wiped device to confirm nothing is recoverable.

---

## 7. Existing Tools — Comparison Context

| Tool                                  | Technique                                                   | Strength                                     | Limitation                                                  |
| ------------------------------------- | ----------------------------------------------------------- | -------------------------------------------- | ----------------------------------------------------------- |
| **DBAN** (Darik's Boot and Nuke)      | Multi-pass random overwrite                                 | Free, open-source, effective on HDDs         | No verification report; ineffective on SSDs (wear-leveling) |
| **Blancco Drive Eraser**              | Certified overwrite algorithms (DoD 5220.22-M, NIST 800-88) | Enterprise-trusted, audit-ready certificates | Commercial/expensive                                        |
| **CCleaner / Windows Format Utility** | Basic overwrite/format                                      | Simple, accessible                           | Weak security assurance, no compliance reporting            |

Forensic/data-recovery tools referenced during research (to understand what a wipe needs to defeat):
Autopsy/Sleuth Kit, FTK Imager, EnCase, X-Ways Forensics, R-Studio, TestDisk/PhotoRec, Recuva.
These were studied to understand recovery techniques (file-system metadata parsing, raw carving,
journal/log analysis) — not used or integrated in this project.

---

## 8. Key References

- NIST Special Publication 800-88 Rev. 1, _Guidelines for Media Sanitization_ —
  https://nvlpubs.nist.gov/nistpubs/specialpublications/nist.sp.800-88r1.pdf
- NIST SP 800-88 Rev. 2 (Initial Public Draft) — https://doi.org/10.6028/NIST.SP.800-88r2.ipd
- Diesburg, S. M., & Wang, A.-I. A. (2010). _A survey of confidential data storage and deletion
  methods._ ACM Computing Surveys, 43(1), 1–37. https://doi.org/10.1145/1824795.1824797
- Chen, S.-H., & Huang, K.-H. (2023). _Leveraging journaling file system for prompt secure
  deletion on interlaced recording drives._ IEEE Transactions on Emerging Topics in Computing,
  11(3), 619–634. https://doi.org/10.1109/tetc.2022.3226620
- Sandwal, S. K., Jakhar, R., & Styszko, K. (2025). _E-waste challenges in India: Environmental
  and human health impacts._ Applied Sciences, 15(8), 4350. https://doi.org/10.3390/app15084350
