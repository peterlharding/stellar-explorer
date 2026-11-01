/*
 * C ABI for the Rust core feasibility spike (ADR 0014). The Unreal side includes
 * this header and links the static library built by build.sh. Nothing C++ or STL
 * crosses this boundary; a thin C++ wrapper over these functions lives in the
 * Unreal module, not here.
 */
#ifndef STELLAR_CORE_H
#define STELLAR_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABI version of the library; must match the caller's expectation. */
uint32_t stc_abi_version(void);

/* Distance in parsecs from a parallax in milliarcseconds; NaN if parallax <= 0. */
double stc_distance_parsecs(double parallax_mas);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STELLAR_CORE_H */
