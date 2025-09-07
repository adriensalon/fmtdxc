# fmtdxc

Binary project format (.dxcc) meant to be converted to/from other DAW formats. Projects are wrapped into `fmtdxc::project_container` data structures with diff/merge capabilities for versioning and multiplayer. This provides functionnality for importing and exporting containers as C++17 data structures. Documentation for the data structures can be found in the [fmtdxc/fmtdxc.hpp](include/fmtdxc/fmtdxc.hpp) header.

Requires [cereal](https://github.com/USCiLab/cereal) headers path to be defined as `CEREAL_INCLUDE_DIR` from CMake.

### Usage

Use `void fmtdxc::import_container(std::istream&, fmtdxc::project_container&, fmtals::version&)` to import a project container and retrieve the dxcc version it was created with.

Use `void fmtdxc::export_container(std::ostream&, const fmtdxc::project_container&, const fmtdxc::version&)` to export a project container for a specified dxcc version.
