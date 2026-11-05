# Lubuntu VM Details

_Last updated: 2026-10-12_

## Virtual Machine

| Component | Details |
|---|---|
| Operating system | Lubuntu 26.04.1 LTS |
| Desktop environment | LXQt |
| Display session | X11 |
| Hypervisor | VMware Workstation Pro 25H2 |
| RAM | 3 GB |
| CPU | 2 cores |
| Virtual disk | 25 GB |
| Terminal | QTerminal 2.3.0 |
| Browser | Brave |

## Appearance and Input

| Component | Current configuration |
|---|---|
| LXQt theme | `graphite` |
| Qt widget style | `kvantum-dark` |
| Kvantum theme | `GraphiteDark` |
| GTK 3 / GTK 2 theme | `Graphite-Dark` |
| Terminal color scheme | `GreenOnBlack` |
| Icon theme | `Papirus` (`Papirus-Dark` installed) |
| Keyboard layout | English (US), `setxkbmap -layout us` |

## Development Tools

| Tool | Version |
|---|---|
| G++ | 15.2.0 |
| CMake | 4.2.3 |
| Ninja | 1.13.2 |
| Python | 3.14.4 |
| Git | 2.53.0 |

## ns-3 Status

| Item | Status |
|---|---|
| ns-3 dependencies | Installed |
| ns-3 build | **In progress** |
| ns-3 version | TBD (confirm: release tarball vs `ns-3-dev` git clone) |
| Source location | TBD (confirm: `~/ns3` or other) |
| Configure flags | TBD (target: `--enable-examples --enable-tests`) |

## Build Constraints

- 3 GB RAM + 2 cores: use `./ns3 build -j2` (or `-j1` if OOM / `cc1plus killed`).
- 25 GB disk: debug build with examples+tests needs ~5–8 GB. Check `df -h /` after build.

## Verify After Build

```bash
cd <ns-3 dir>
./ns3 show config | grep -Ei 'examples|tests|build profile'
./ns3 run hello-simulator
./test.py -s traffic-control
```
