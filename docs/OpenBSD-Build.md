# Building SolveSpace on OpenBSD

This document provides instructions for building SolveSpace on OpenBSD, including setup in an emulated environment if needed.

## Direct Installation on OpenBSD

If you're running OpenBSD natively, you can use the provided build scripts:

```sh
# Install dependencies
.github/scripts/install-openbsd.sh

# Build SolveSpace
.github/scripts/build-openbsd.sh
```

Note that on OpenBSD, the produced executables are not filesystem location independent and must be installed before use. The installation path is `/usr/local/bin/solvespace` for the GUI and `/usr/local/bin/solvespace-cli` for the command-line interface.

## Building in an Emulated Environment

If you need to build and test on OpenBSD in an emulated environment, follow these steps:

### Setting up QEMU for OpenBSD

1. Install QEMU on your host system:
   ```sh
   # On Ubuntu/Debian
   sudo apt-get install qemu-system-x86
   ```

2. Download OpenBSD installation image from https://www.openbsd.org/faq/faq4.html#Download

3. Create a virtual disk:
   ```sh
   qemu-img create -f qcow2 openbsd.qcow2 20G
   ```

4. Start the VM with the installation image:
   ```sh
   qemu-system-x86_64 -m 2048 -smp 2 -hda openbsd.qcow2 -cdrom /path/to/install*.iso -boot d
   ```

5. Follow the OpenBSD installation process

### Building SolveSpace in the VM

1. Clone the SolveSpace repository:
   ```sh
   git clone https://github.com/solvespace/solvespace
   cd solvespace
   ```

2. Run the installation script:
   ```sh
   .github/scripts/install-openbsd.sh
   ```

3. Build SolveSpace:
   ```sh
   .github/scripts/build-openbsd.sh
   ```

4. Install the built executables:
   ```sh
   doas make install
   ```

### Transferring Files Between Host and VM

To transfer the built binaries or test the application:

1. Set up SSH in the VM and use SCP to transfer files
2. Alternatively, set up a shared folder between the host and VM:
   ```sh
   qemu-system-x86_64 -m 2048 -smp 2 -hda openbsd.qcow2 -virtfs local,path=/path/to/shared/folder,mount_tag=host0,security_model=mapped,id=host0
   ```

   Then in OpenBSD:
   ```sh
   mkdir -p /mnt/shared
   mount_9p host0 /mnt/shared
   ```

## Testing

After building and installing SolveSpace, you can run it with:

```sh
/usr/local/bin/solvespace
```

For the command-line interface:

```sh
/usr/local/bin/solvespace-cli
```

## Troubleshooting

- If you encounter library loading issues, ensure all dependencies are properly installed
- For graphics-related issues, make sure X11 is properly configured in your OpenBSD environment
- When running in QEMU, ensure you have 3D acceleration enabled if available:
  ```sh
  qemu-system-x86_64 -m 2048 -smp 2 -hda openbsd.qcow2 -vga virtio -display gtk,gl=on
  ```
