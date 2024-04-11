# A model for fleet sizing and vehicle allocation
## Installation
### Build from source using CMake
Thus library uses few dependencies:
1. [**HiGHs**](https://github.com/ERGO-Code/HiGHS/blob/master) - high performance serial and parallel solver for large scale sparse
linear optimization problems of the form\
(build as a static library; look `HiGHs/bin, HiGHs/lib, HiGHs/include`)
2. [**OpenXLSX**](https://github.com/ycphs/openxlsx) - simplifies parsing of .xlsx files by providing a high level interface to reading worksheets.\
(not pre-build library by now - provided by source code 😳; look `OpenXLSX/`)
3. [**GoogleTest**](https://github.com/google/googletest) - Google's C++ test framework.\
(install by yourself: `sudo apt install libgtest-dev`)

uses CMake as build system, and requires at least version 3.1. To generate build files in a new subdirectory called 'build', run:
```sh
    mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
```
(`DEBUG` being build by not giving any `CMAKE_BUILD_TYPE`)
##### To test whether the compilation was successful, change into the build directory and run
```sh
  ./test/main_test
```
