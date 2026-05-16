Fuse for Windows
================

This is a binary distribution of Fuse (the Free Unix Spectrum Emulator) for
Windows. For an introduction to Fuse, see README.txt. For more detailed
documentation, read fuse.html.

Requirements
------------

A supported 32-bit or 64-bit Windows system for the bundled 32-bit build.

Libraries
---------

These binaries are built with the MinGW/Cygwin packages used by the current
Windows CI workflow. Depending on the selected UI and enabled features, the
package may include DLLs from these components:

    audiofile
    bzip2
    libFLAC
    libgcrypt
    libgpg-error
    libOGG
    libpng
    libspectrum
    libstdc++
    libxml2
    win-iconv
    winpthreads
    xz / liblzma
    zlib
    SDL
    SDL2

See LICENSES.txt for copyright and license details.

For more detailed documentation, browse:
https://sourceforge.net/p/fuse-emulator/wiki/Building%20Fuse%20on%20Windows%20with%20Cygwin/
