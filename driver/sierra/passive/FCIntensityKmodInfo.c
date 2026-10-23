/*
 * Kernel module metadata for the inert Sierra IOKit driver.
 * Matches CFBundleIdentifier exactly; the C++ runtime owns startup/shutdown.
 * No custom hardware initialization occurs at module load.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#include <mach/kmod.h>
extern kmod_start_func_t _start;
extern kmod_stop_func_t _stop;
KMOD_EXPLICIT_DECL(coach.fountain.intensity.PCIDiagnostic, "0.1.0", _start, _stop)
kmod_start_func_t *_realmain = 0;
kmod_stop_func_t *_antimain = 0;
