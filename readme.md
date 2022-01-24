# libtabfs

This projects aims to implement TabFs-28 as an minimal dependency library
so various other projects can use it as they need.

## License

libtabfs is licensed under the AGPL-v3 license. For more informations, see the `LICENSE` file.

## Usage

To use libtabfs's public api you can simply include it: `#include <libtabfs.h>`.

### Bridge functions

In order to connect the library to your environment, you have to implement some functions; all of them can be found in the `bridge.h` headerfile. For more details on them and how to implement them, please read the doxygen comments of the functions in `bridge.h`.

This library uses these bridge-functions to not rely on many if not any libc functions.

### Type / Definitions

This library uses following type and/or definitions of libc:
- `bool`
- `NULL`

## Roadmap

- Implementing FAT files
- Implementing Segmented files
- Adding debugging capabilities