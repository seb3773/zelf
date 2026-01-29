Vendored UCL sources (NRV2B-99 compressor)

Origin:
  UCL (Ultimate Compression Library)
  Author: Markus F.X.J. Oberhumer
  http://www.oberhumer.com/opensource/ucl/

Why vendored here:
  zELF uses ucl_nrv2b_99_compress() in the packer to NRV-compress the stage-1 stub
  for the two-stage (stage0+stage1) decompressor mechanism.

License:
  This vendor snapshot is under the GNU GPL v2 (see COPYING.UCL).

Contents:
  - include headers: src/tools/ucl/vendor/include/ucl/
  - sources:         src/tools/ucl/vendor/src/
