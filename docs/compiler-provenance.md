# Compiler Provenance

## Current Conclusion

Halo Xbox debug build 2276 identifies itself as:

`halobeta xbox 01.10.12.2276 Oct 12 2001 16:07:48`

Exact compiler build and optimization flags are not proven. VC71 (MSVC
13.10.3077) is the closest available comparison toolchain used by
`vc71_verify.py`; it is not established as the compiler that produced the XBE.
Visual Studio .NET 2003 postdates this binary.

Custom register-argument conventions are confirmed in machine code. MSVC LTCG
can generate such conventions on x86, making `/GL` plus `/LTCG` plausible, but
the convention alone does not prove those flags or a compiler version.

## Evidence Ladder

Exact proof requires an authenticated original artifact carrying tool identity:

1. Original `.obj`, `.lib`, PDB, linker map, build log, project file, or response
   file with CodeView compiler records, command lines, or linker directives.
2. An unstripped executable or symbol package containing CodeView records.
3. Matching compiler fingerprints across many functions built from known source.
   This identifies a likely family and flag set, not exact provenance.
4. Final-XBE patterns such as custom conventions, x87 scheduling, CRT helpers,
   jump tables, and prologues. These are supporting evidence only.

XBE library-version records identify linked Xbox/XDK libraries, not necessarily
the compiler used for Bungie's game translation units.

## Practical Investigation

1. Search available symbol packages, archives, discs, and build trees for PDB,
   object, library, map, incremental-link, response, and project files from 2276.
2. Inspect XBE and symbol files for CodeView `NBxx`/`RSDS` records, compiler
   banners, linker directives, and PDB paths.
3. Record XBE library-version entries and compare them with dated Xbox XDK
   releases. Treat this as an XDK-date bound, not direct compiler proof.
4. Assemble historical candidate toolchains available around October 2001,
   including Xbox XDK compiler builds, VC6, and VC7 prereleases. Keep VC71 as a
   useful later control.
5. Build a fingerprint corpus from known-source functions such as vendored zlib
   and CRT routines. Compile with candidate optimization and LTCG settings.
6. Compare exact instruction bytes and relocations, not only mnemonic LCS.
   Weight helper selection, exception metadata, name decoration, x87 ordering,
   switch tables, frame layout, and cross-TU inlining.
7. Require one candidate to explain multiple objects. One matching function is
   fingerprint evidence, not provenance proof.

Until original tool-bearing evidence is found, reports should say "VC71
comparison score" and "compiler-family match", never "same compiler".
