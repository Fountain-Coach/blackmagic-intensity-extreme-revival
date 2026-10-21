# Experimental repair candidate for DeckLinkCMIO format discovery

**21 October 2026 — Static, standalone load, and bounded system-level A/B evidence. The experiment did not restore CoreMediaIO device enumeration.** The proven workaround remains to isolate the unmodified faulty plugin, retaining the working Blackmagic kernel driver and direct DeckLink API access. This experiment is an attempt to understand and ultimately correct that proprietary application-layer defect.

## Exact source of the candidate

The original Blackmagic Desktop Video 10.4 CoreMediaIO executable was inspected from an authorized private copy, but **no proprietary executable or decompiled source is distributed here**.

- Original universal Mach-O SHA-256: `6f1eda3ee6521dcb4c62df86f4f4e64e7e10a35dd47b39ee97d35472ca469da3`.
- Original extracted Intel 64-bit slice SHA-256: `3774f9acb44c9b4eb909fb4263b0c022aa21d63c550f2f859f5d01a9a78ad577`.
- Function: `DeckLinkOutputStream::AddAvailableFormatDescriptions()`, relative address `0x58d2` in the x86-64 slice.
- Analysis: private REA/Ghidra decompilation and independent `llvm-objdump` confirmation of the exact machine instruction bytes.

## Two suspected errors in the format loop

**1. Wrong pixel argument.** At relative offset `0x6352`, the original code contains `44 89 FA`, which is `movl %r15d, %edx`. The loop variable `r15d` is 0, 1, 2, 3. The code separately loads the intended FourCC format into `ebx`; the intended `IDeckLinkOutput::DoesSupportVideoMode` pixel argument is therefore not passed.

**2. Incorrect error branch.** At `0x635f`, the original code ORs the HRESULT with a returned support flag. At `0x6365`, it uses `JE` to skip unsupported descriptions only when the OR result is zero. This treats a **negative HRESULT error** as though a supported format was found.

### Measurements from the actual Intensity Extreme

All of the following were **read-only** native DeckLink queries on the legacy host, with the old CoreMediaIO plugin isolated:

| Requested format | SDK response | Supported output modes |
| --- | --- | --- |
| Integer 0 (unspecified pixel format) | `S_OK` | 16/16 |
| Integers 1, 2 or 3 | `0x80000003` | 0/16 each |
| `2vuy` | `S_OK` | 16/16 |
| `v210` | `S_OK` | 5/16 (standard definition) |
| FourCC `0x20` | `S_OK` | 0/16 |
| `R10k` | `0x80000003` | 0/16 |

The original loop's intended table contains `2vuy`, `v210`, `0x20`, `R10k`. Because the original code submits 0–3 instead, the last three iterations in every mode receive errors. The original condition then admits those negative error values to format-description construction. This is a **plausible cause of a CoreMediaIO discovery failure**. The exact connection to the App Store's invalid-object-release crash is not yet experimentally proven.

## Byte-exact, offline-only candidate

The [offline patch recipe](../scripts/offline_cmio_patch_candidate.py) performs only:

1. `0x6352`: `44 89 FA` → `89 DA 90` — pass the intended FourCC format in `edx`.
2. `0x6365`: `0F 84 42 01 00 00` → `0F 8E 42 01 00 00` — change JE to JLE, skipping unsupported values and negative HRESULT errors (as encoded by this function).

Only **four bytes** change in the Intel slice; the file size, other architectures and the rest of the bundle are not changed by the recipe. The tool rejects unrecognized SHA-256s, unknown Mach-O architectures, unexpected instruction preimages, in-place overwrites, OS installation paths and existing destinations.

Command for a private analysis copy (do **not** use it against the installed CoreMediaIO directory):

```sh
python3 scripts/offline_cmio_patch_candidate.py /path/to/private/original/DeckLinkCMIO --dry-run
python3 scripts/offline_cmio_patch_candidate.py /path/to/private/original/DeckLinkCMIO \
  --output /tmp/DeckLinkCMIO-offline-candidate
```

**Offline validation already completed:** six synthetic-input unit tests passed; the tested original universal binary passed all hash/preimage checks; the candidate output differed by exactly four bytes at universal file offsets `0x7352`, `0x7353`, `0x7354`, `0x7366`. LLVM confirmed the new `movl %ebx,%edx` and `jle` instructions.

**Scope of the offline patch recipe:** It writes an unsigned candidate file only; changing executable bytes invalidates the vendor signature. A separate ad-hoc signed test bundle subsequently passed a local load check (below), but CoreMediaIO device discovery, physical capture and a permanent replacement remain unverified. Do not interpret byte-exact patching or successful bundle loading as a driver repair.

## Runtime experiments completed — same day

**Standalone test bundle.** A copy of the original bundle was placed in a temporary, non-discoverable user-owned directory on the Sierra host, with only the four verified patch bytes changed. The vendor signature correctly failed after modification. The **test copy only** was ad-hoc signed with Sierra's `codesign`. Its ad-hoc signature verified successfully, and a small independent CoreFoundation process could `CFBundleLoadExecutable` and unload it without crashing. None of these operations modified the preserved original.

**Plugin factory test.** A self-authored [factory probe](../src/cmio-factory-probe.c) used CoreFoundation to instantiate the plugin factory from the preserved vendor-original bundle and, separately, from the ad-hoc signed candidate. Both returned `phase=create_plugin status=ok` and `phase=create_instance status=ok`. This is a nonprivileged factory initialization result, not CoreMediaIO device registration. The tests ran in separate short-lived processes without any system-wide installation.

**Per-account discovery test.** The signed test bundle was copied temporarily under the account's own CoreMediaIO/DAL directory; a fresh CoreMediaIO enumeration still reported zero devices. The copy was removed after the test. This did not establish whether the user-level location is supported by Sierra.

**Administrator-approved system-level A/B.** A guarded, time-bounded script temporarily installed the signed patch copy into the normal system-wide DAL directory, ran a fresh CoreMediaIO enumeration, and launched App Store. Its complete observed result:

```text
stage=preflight hashes=ok signature=ok
stage=install_test status=verified
step=read_devices_size
size_status=0 bytes=0
step=read_devices
devices_status=0 bytes=0 count=0
stage=cmio_query exit=0
stage=appstore_launch exit=0
stage=appstore_survived value=true
stage=appstore_crashes_before value=5
stage=appstore_crashes_after value=5
stage=remove_test status=0
stage=preserved_original hash=6f1eda3ee6521dcb4c62df86f4f4e64e7e10a35dd47b39ee97d35472ca469da3
stage=final_original_isolated value=true
```

**Interpretation:** App Store did not crash during the short test, but **no Blackmagic CoreMediaIO device was enumerated**. This **does not prove the patch fixed the original crash**: the modified ad-hoc signed plugin may not have been loaded into the system media process, or another initialization requirement may be unmet. Nor does it prove the corrections are harmless or sufficient once full registration occurs. Do not install the patched copy permanently. The script restored the proven isolation, the original vendor bundle remains intact, and direct DeckLink enumeration still works.

## Next staged acceptance

1. Establish why the candidate does not enumerate a video device in macOS despite passing the signature/load checks (for example, confirm actual plugin loading, system signature validation and CoreMediaIO factory initialization).
2. Keep the current working App Store isolation as the stable configuration while diagnosing the separate core video problem.
3. Prefer a source-owned CoreMediaIO DAL bridge to the confirmed direct DeckLink API if the proprietary plugin cannot be safely repaired. A later replacement must have explicit end-to-end discovery tests, not merely binary loading.
4. Only reattempt a privileged temporary test after a new independent proof or instrumentation improves the prospect of meaningful evidence. Do not permanently install the experimental patched binary.

This candidate is specific to the inspected binary version, not a general driver update and not a current-macOS compatibility promise.
