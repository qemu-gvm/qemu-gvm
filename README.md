This is a patched QEMU that can work with **Android Emulator Hypervisor Driver
for AMD Processors**. Both AMD and Intel processors are supported.

This is for evaluation purpose. The author tries to keep it up-to-date but it
is best effort only.

**Building Instructions:**

*   Download and install msys2 
*   Start C:\msys64\mingw64.exe
*   Updates (then close window and restart mingw64.exe): pacman -Syu
*   Updates: pacman -Su
*   Install basic packets: pacman -S base-devel mingw-w64-x86_64-toolchain git python
*   Install QEMU specific packets: pacman -S mingw-w64-x86_64-glib2 mingw-w64-x86_64-gtk3 mingw-w64-x86_64-SDL2
*   Get QEMU sources:
    *   git clone git://github.com/qemu-gvm/qemu-gvm.git
    *   cd qemu-gvm
    *   git submodule update --init ui/keycodemapdb
    *   git submodule update --init capstone
    *   git submodule update --init dtc
*   Build QEMU:
    *   ./configure --enable-gtk --enable-sdl --target-list=x86_64-softmmu --disable-capstone --prefix=$(pwd)/install_dir
    *   make install -C build
*   Run in install_dir ./qemu-system-x86_64 -accel gvm
