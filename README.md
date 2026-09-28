# USB Tools - THCGaming

**USB Tools - THCGaming** is a customized Windows USB utility based on the open-source Rufus project.

Current development version: **0.1.0**

## V0.1 goals

- THCGaming application branding
- Branded executable: `THCGaming-USB-Tools.exe`
- Vietnamese as the default UI locale when no saved locale is present
- Preserve Rufus disk/ISO/formatting engine
- Disable the upstream Rufus application updater to prevent cross-product updates
- Keep upstream technical resources required by the Rufus engine
- Preserve GNU GPLv3 notices and upstream attribution

## Planned direction

The project will progressively add a simplified Basic Mode, an Advanced Mode, smart Windows/Linux presets, stronger target-drive safety checks, ISO history/library, checksum verification, backup/restore tools, and a THCGaming-specific update channel.

## Build

The upstream project supports Visual Studio and MinGW. The Visual Studio solution remains compatible with the Rufus project layout while producing the THCGaming-branded executable.

## Upstream

This project is derived from **Rufus**, created and maintained by Pete Batard and contributors:

https://github.com/pbatard/rufus

The disk-writing and boot-media implementation remains substantially based on Rufus. Upstream copyright notices are retained in source files.

## License

USB Tools - THCGaming is distributed under the **GNU General Public License v3.0 or later**, consistent with the upstream Rufus license.

See `LICENSE.txt` for the complete license text.

## Modification notice

USB Tools - THCGaming modifications © 2026 THCGaming.

Rufus copyright © 2011-2026 Pete Batard and contributors.
