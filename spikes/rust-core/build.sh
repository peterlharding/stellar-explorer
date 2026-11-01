#!/bin/sh
# Build the Rust core spike as a static library for one target.
# Run the Windows build on Windows; the macOS and visionOS builds on the Mac.
#   ./build.sh win           x86_64-pc-windows-msvc      (stable)
#   ./build.sh mac           aarch64-apple-darwin        (stable)
#   ./build.sh visionos      aarch64-apple-visionos      (nightly + build-std)
#   ./build.sh visionos-sim  aarch64-apple-visionos-sim  (nightly + build-std)
#
# visionOS is a Tier 3 Rust target: there is no prebuilt std, so it is compiled
# from source with a nightly toolchain and the rust-src component:
#   rustup toolchain install nightly
#   rustup component add rust-src --toolchain nightly
set -eu
case "${1:-}" in
win)
	cargo build --release --target x86_64-pc-windows-msvc
	;;
mac)
	cargo build --release --target aarch64-apple-darwin
	;;
visionos)
	cargo +nightly build --release -Z build-std=std,panic_abort --target aarch64-apple-visionos
	;;
visionos-sim)
	cargo +nightly build --release -Z build-std=std,panic_abort --target aarch64-apple-visionos-sim
	;;
*)
	echo "usage: $0 {win|mac|visionos|visionos-sim}" >&2
	exit 2
	;;
esac
