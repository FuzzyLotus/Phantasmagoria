# Phantasmagoria
### Spectral Shimmer Delay for the Electrosmith Daisy Seed / PedalPCB Terrarium

Phantasmagoria is a complex, lush spectral delay effect implemented in C++ for the [Daisy Seed](https://electro-smith.com/products/daisy-seed) microcontroller running on the [PedalPCB Terrarium](https://www.pedalpcb.com/product/terrarium/) guitar pedal platform.

It combines spectral delay, reverse delay, pitch shifting, shimmer, freeze, and reverb into a single atmospheric effect pedal.

---

## Features

- **Spectral Delay** — multi-tap echo chamber reverb with carefully tuned tap timing
- **Reverse Delay** — sweeping reverse buffer with fixed LFO parameters for consistent feel
- **Pitch Shifting** — real-time pitch shift via grain-based processing
- **Shimmer** — octave-up pitch shift blended into the reverb tail (SW4)
- **Freeze** — hold audio in the buffer indefinitely; hold to accumulate new layers
- **Reverb** — 48kHz echo chamber with taps at 83 / 151 / 227 / 311 ms

---

## Controls

| Control | Function |
|---|---|
| Knob 1 | Mix (dry/wet) |
| Knob 2 | Decay / Reverb time |
| Knob 3 | Pitch shift amount |
| Knob 4 | Reverse delay mix |
| Knob 5 | Shimmer level |
| Knob 6 | Input gain |
| SW1 | Reverse delay on/off |
| SW2 | Pitch shift on/off |
| SW3 | Reverb on/off |
| SW4 | Shimmer mode (octave up) |
| FS1 | Bypass (independent of freeze) |
| FS2 | Freeze — tap to hold, hold to accumulate layers |

> **Note:** Knob assignments may vary slightly depending on your build. Check the source comments in `phantasmagoria.cpp` for the definitive mapping.

---

## Hardware Requirements

- [Electrosmith Daisy Seed](https://electro-smith.com/products/daisy-seed)
- [PedalPCB Terrarium](https://www.pedalpcb.com/product/terrarium/) (or compatible Daisy Seed pedalboard)
- ARM DFU-capable USB connection for flashing

---

## Building from Source

### 1. Clone the DaisyCloudSeed repository and its submodules

```bash
git clone https://github.com/GuitarML/DaisyCloudSeed
cd DaisyCloudSeed
git submodule update --init --recursive
```

### 2. Build the required libraries

```bash
make -C libdaisy
make -C DaisySP
```

### 3. Clone Phantasmagoria into the petal folder

```bash
cd petal
git clone https://github.com/FuzzyLotus/Phantasmagoria
cd Phantasmagoria
```

### 4. Build and flash

```bash
make
make program-dfu
```

> Connect your Daisy Seed via USB and put it into DFU mode (hold BOOT, tap RESET, release BOOT) before running `make program-dfu`.

---

## Toolchain

You need the ARM GCC embedded toolchain. On most Linux systems:

```bash
# Fedora/RHEL
sudo dnf install arm-none-eabi-gcc arm-none-eabi-newlib

# Ubuntu/Debian
sudo apt install gcc-arm-none-eabi
```

Or download directly from [ARM's website](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).

---

## Project Structure

```
Phantasmagoria/
├── phantasmagoria.cpp   # Main DSP source
├── terrarium.h          # Terrarium hardware abstraction (included locally)
├── Makefile
└── README.md
```

`terrarium.h` is included directly in this repo so you don't need to rely on the Terrarium submodule separately.

---

## Contributing

Pull requests are welcome! If you build on this project, please share your work openly under the same license.

- Fork the repo
- Create a branch: `git checkout -b my-feature`
- Commit your changes: `git commit -m "Add my feature"`
- Push and open a Pull Request

Please test on real hardware before submitting.

---

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.

You are free to use, modify, and share this project. You may **not** use it in closed-source or commercial products. Any derivatives must also be released under GPL-3.0.

---

## Credits

## Credits

Built on the [GuitarML DaisyCloudSeed](https://github.com/GuitarML/DaisyCloudSeed) build environment, which bundles [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP).

The reverb lineage traces back to [ValdemarOrn/CloudSeed](https://github.com/ValdemarOrn/CloudSeed), the original open source algorithmic reverb plugin that inspired this entire ecosystem.
