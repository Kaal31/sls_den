# PS4 feasibility assessment — 2026-10-28

Assessment: porting Playden's launcher and metadata logic is plausible; native Windows-game execution
on PS4 is a separate substantial runtime project. No PS4 port or hardware test is included here.

## CPU and memory

Sony specifies an eight-core x86-64 AMD Jaguar CPU, 8 GB GDDR5 shared memory and a 1.84 TFLOPS GPU
for base PS4; PS4 Pro has the same CPU family and memory size with a 4.20 TFLOPS GPU.
The shared instruction architecture removes one potential translation requirement for compatible
x64 code, but does not establish OS ABI, instruction-extension, driver or game compatibility.
Available application memory is smaller than total system memory. It must be measured for the actual
homebrew environment. My inference: memory pressure, CPU overhead and graphics limits make older,
lighter games the sensible initial target; PS5 performance numbers cannot predict PS4 performance.
[Sony PS4 specifications](https://www.playstation.com/en-gb/ps4/tech-specs/)

## Wine runtime

Wine implements Windows APIs on supported POSIX systems; x86-64 hardware alone does not supply those
OS services. [Wine overview](https://www.winehq.org/about)

Playden delegates execution to prospero-win. Its Unix-side Wine build targets FreeBSD with PS5-specific
patches, including signal context, TLS, page-size handling, threads and server integration. Those
assumptions need an Orbis audit and new measurements; changing a platform define is insufficient.
Its graphics/input/audio integration also needs a PS4 backend, and 32-bit applications need a tested
WoW64/translation path. We have not established an existing usable native PS4 Wine kit.
[prospero-win Wine build notes](https://github.com/mpereiraesaa/prospero-win/blob/main/docs/WINE_PS5_BUILD.md)

## SDK, APIs and graphics

OpenOrbis supplies community PS4 headers, library stubs and build/package tools without Sony's official
SDK; its documentation explicitly notes gaps in discovered functions. It is a potential toolchain,
not proof that Playden's PS5 dependencies will compile or work. PS4-specific implementations are needed
for video output, controller/audio, user/system services, files and permissions, title metadata,
registration, launch and lifecycle. Playden's PS5 ShadowMount protocol, title paths and privileged
startup helpers must not be assumed to work unchanged.
[OpenOrbis toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)

prospero-win's documented pipeline uses DXVK with Vulkan/RADV and PS5 services for display, sound
and controls. Native PS4 needs a demonstrably functioning graphics and presentation path before
Direct3D compatibility can be promised. Sharing AMD ancestry does not establish driver/queue/memory
compatibility. My inference: this graphics work is a greater uncertainty than the launcher UI.
[prospero-win architecture](https://github.com/mpereiraesaa/prospero-win)

## Practical options

1. **Native launcher first:** reuse source parsing, fallback, Steam metadata and much of the C++ model;
   bring up a minimal OpenOrbis build and controller UI, then storage, networking and packaging.
2. **Runtime proof separately:** test a trivial Windows console program, then software drawing,
   input/audio, and finally one small graphics workload. Validate CPU features, memory and firmware
   support on real PS4 hardware before selecting game targets.
3. **PS4 Linux experiment:** community PS4 Linux kernels exist. My inference is that a compatible Linux
   setup plus Linux Wine/Mesa may reduce native ABI work, but kernel/model support, GPU acceleration,
   Vulkan and performance still need verification. This is a separate Linux target, without automatic
   native PS4 Home-screen title integration.
   [PS4 Linux kernel project](https://github.com/ps4boot/ps4-linux)

The desktop harness can help test platform-independent behavior throughout a port. It cannot prove
that Wine, PS4 APIs, graphics, game performance or the hardware work.
