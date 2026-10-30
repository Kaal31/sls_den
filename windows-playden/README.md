# Windows host compilation probe

The Windows branch isolates the first Playden simulation compilation fixes from the PS5 branch. The probe compiles the actual simulation translation unit with MSVC to expose subsequent portability blockers. This is not yet an end-to-end Windows GUI target.

The first Windows probe cleared the POSIX header issue and reached an MSVC C1061 nested-block compiler limit. The next probe uses clang-cl with the Windows/MSVC ABI to continue discovering portability failures.
