# Build MINIX/arm with clang

It is now possible to build a full minix distribution for BeaglBone White/Black and BeagleBoardxM using clang instead of GCC.

This also add support to run the Kuya tests on ARM, which was not possible when GCC was used, because of problems in the C++ exception handling.

## Known Bugs

The following tests still fails:
 1. 53: Division by zero does not trigger exceptions
 2. 75: ru.tv_secs can't be zero (and is zero)
 3. 85: hangs
 4. isofs: Fails because of an out of memory condition
 5. vnd: crash
 6. Running two times the kyua tests in a row, without rebooting in between will lead to a mostly failed second run because of copy-on-write errors.

 
