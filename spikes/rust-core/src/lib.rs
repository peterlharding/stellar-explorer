//! Feasibility spike: a minimal Rust static library with a C ABI, to prove a Rust
//! core can link into the Unreal project and cook on all three targets (Windows x64,
//! macOS arm64, visionOS) — the visionOS build being the real test. See README.md.
//!
//! This is not the real core; it exists only to be linked and called across the
//! boundary that ADR 0014 defines.

/// ABI version of this library, so the caller can check it matches its header.
#[no_mangle]
pub extern "C" fn stc_abi_version() -> u32 {
    1
}

/// Distance in parsecs from a parallax in milliarcseconds — the phase-1 reference
/// maths, used here as a non-trivial smoke test. Returns NaN for a non-positive
/// parallax rather than panicking across the FFI boundary.
#[no_mangle]
pub extern "C" fn stc_distance_parsecs(parallax_mas: f64) -> f64 {
    if parallax_mas > 0.0 {
        1000.0 / parallax_mas
    } else {
        f64::NAN
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parallax_of_100_mas_is_10_parsecs() {
        assert!((stc_distance_parsecs(100.0) - 10.0).abs() < 1e-9);
    }

    #[test]
    fn non_positive_parallax_is_nan() {
        assert!(stc_distance_parsecs(0.0).is_nan());
        assert!(stc_distance_parsecs(-1.0).is_nan());
    }
}
