# Changelog

## [Feb/2026] - 2026-02-23

### Added
- Makefile for easier compilation of all server and client binaries
- .gitignore to prevent accidental commits of compiled binaries and temp files
- CHANGELOG.md to track feature branch changes

### Notes
- Use `make` to compile all binaries at once
- Use `make clean` to remove all compiled binaries
- Individual targets available: `make S1`, `make S2`, `make S3`, `make S4`, `make s25client`