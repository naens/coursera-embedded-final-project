# Linked Projects for Submodules

This document lists the external projects that should be added as Git submodules to this repository.

## Projects to Add as Submodules

### 1. 8086tiny Emulator

**Repository:** https://github.com/adriancable/8086tiny

**Description:** Official repository for 8086tiny: a tiny PC emulator/virtual machine

**Purpose:** This minimalistic 8086 emulator will be adapted to run on the Raspberry Pi Zero and execute CCP/M-86. It will need modifications for:
- Adjusting the number of rows and columns for the SPI display
- Adding poweroff functionality
- Integration with the custom display and keyboard drivers

**License:** MIT License

**How to add as submodule:**
```bash
git submodule add https://github.com/adriancable/8086tiny.git 8086tiny
```

### 2. CCP/M-86 Source Code

**Repository:** https://gitlab.com/ccpm-86/ccpm

**Description:** Concurrent CP/M-86 source code that will be adapted to run on the modified 8086tiny emulator

**Purpose:** The operating system that will run on the emulated 8086 environment. Modifications needed include:
- Adapting the XIOS module for the custom display
- Adjusting status line for the SPI display format
- Modifying character output for the screen
- Rebuilding the CCPM.SYS file with modifications

**Note:** This is hosted on GitLab, not GitHub.

**How to add as submodule:**
```bash
git submodule add https://gitlab.com/ccpm-86/ccpm.git ccpm
```

## Additional Information

**Note:** The wiki mentions a "CCP/M-86 QEMU Build Project" which is part of the same repository (https://gitlab.com/ccpm-86/ccpm) as the main CCP/M-86 source code. This project contains both the source code and QEMU build configurations that will be adapted for this embedded Linux implementation.

For more details about how these projects integrate into the overall system architecture, see:
- [Project Overview](https://github.com/naens/coursera-embedded-final-project/wiki/Project-Overview)
- [Issue #10: Add the 8086tiny emulator](https://github.com/naens/coursera-embedded-final-project/issues/10)
- [Issue #11: CCP/M-86](https://github.com/naens/coursera-embedded-final-project/issues/11)

## Repository Information

| Project | Platform | URL | Stars |
|---------|----------|-----|-------|
| 8086tiny | GitHub | https://github.com/adriancable/8086tiny | 1000+ |
| CCP/M-86 | GitLab | https://gitlab.com/ccpm-86/ccpm | N/A |

Both projects are open source and will be adapted for use on the Raspberry Pi Zero platform with custom SPI display and XT keyboard drivers.
