# Vendored NovarisOS headers

`syscall64.h` is copied unchanged from
[NovarisOS](https://github.com/NaBrO-pixel/NovarisOS) `include/syscall64.h`
at commit `7656729`. `make check-sysnum` uses it when this repo is built on
its own, so the check works without a NovarisOS checkout.

To refresh it from a NovarisOS checkout:

```bash
make update-syscall64 NOVARIS_DIR=/path/to/NovarisOS
```
