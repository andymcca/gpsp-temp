gpsp-temp
---------

a standalone gameboy advance emulator for the psp.

i used the source files from libretro-gpsp, which unlike other gpsp variants has seen ongoing development in the last decade.

i added an implementation of the dynarec partial cache flushing routines originally created by Nebuleon for TempGBA (a gpsp variant) - these help improve speed in certain popular games (mario tennis, mario golf etc)

the basics work, and seem to work well enough.  not sure how much more i'll do with it, but have uploaded here if anyone else wants to contribute.

use build.sh to build it - you need the PSP SDK / Toolchain in place on a Linux box in the expected location to do so.

