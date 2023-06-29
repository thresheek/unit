# wasmtime

include $(dir $(abspath $(lastword $(MAKEFILE_LIST))))/version
WASMTIME_URL := https://github.com/bytecodealliance/wasmtime/releases/download/v$(WASMTIME_VERSION)/wasmtime-v$(WASMTIME_VERSION)-src.tar.gz

PKGS += wasmtime

ifeq ($(shell which cargo >/dev/null 2>&1 || echo FAIL),)
CARGO = cargo
else
CARGO = $(error Cargo (Rust package manager) not found)
endif

$(TARBALLS)/wasmtime-v$(WASMTIME_VERSION)-src.tar.gz:
	$(call download_pkg,$(WASMTIME_URL),wasmtime)

.sum-wasmtime: wasmtime-v$(WASMTIME_VERSION)-src.tar.gz

wasmtime: wasmtime-v$(WASMTIME_VERSION)-src.tar.gz .sum-wasmtime
	$(UNPACK)
	$(MOVE)

.wasmtime: wasmtime
	cd $< && $(CARGO) build --release --manifest-path crates/c-api/Cargo.toml
	cp $</crates/c-api/wasm-c-api/include/wasm.h $</crates/c-api/include/
	touch $@
