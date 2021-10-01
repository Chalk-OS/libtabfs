# libTabFs

This projects aims to implement TabFs-28 as an header-only library
so various other projects can use it as they need.

## Usage

To use libTabfs's public api you can simply include it: `#include <libtabfs.h>`.

In order to let gcc generate the neccessary code, you create an C source file with the following content:
```c
// here goes some more #define statements that we will discuss below
#define LIBTABFS_IMPLEMENTATION
#include <libtabfs.h>
```

This library is build in a way that no kernel specific functions are used, to be precise, it's even not using many libc functions!
But in order to communicate with the user (the program that uses libtabfs) this library uses a few macros that need to be defined:

- Memory management
    - `LIBTABFS_ALLOC`: an allocation function `void* LIBTABFS_ALLOC(int bytes_to_alloc)`
    - `LIBTABFS_FREE`: an de-allocation function `void LIBTABFS_FREE(void* memory, int bytes_to_free)`
    - (optional) `LIBTABFS_REALLOC`: an re-allocation function `void* LIBTABFS_REALLOC(void* old, int old_byte_size, int new_byte_size)`;
        if this symbol is not defined, libtabfs uses an default implementation that combines `LIBTABFS_ALLOC` and `LIBTABFS_FREE`.

### Device communication

`LIBTABFS_HEADER_DEVICE_FIELDS`: macro where the user can define additional fields to `libtabfs_header_t`,
that holds device informations (in linux this would be `dev_t`):
```c
#define LIBTABFS_HEADER_DEVICE_FIELDS   dev_t __user_dev_t;

// since its a macro, multiple fields are possible:
#define LIBTABFS_HEADER_DEVICE_FIELDS   dev_t __user_dev_t; \
                                        int some_other_field;
```

`LIBTABFS_DEVICE_PARAMS(header)`: macro that is used to generate parameters for the device read/write functions;
the header param is of type `libtabfs_header_t`, so an user can access it's defined fields from `LIBTABFS_HEADER_DEVICE_FIELDS`:
```c
#define LIBTABFS_DEVICE_PARAMS(header)  header->__user_dev_t

// or when using multiple fields
#define LIBTABFS_DEVICE_PARAMS(header)  header->__user_dev_t, header->some_other_field
```

`LIBTABFS_DEVICE_TYPES`: this macro is not actually used by libtabfs, but it's an shortcut in error messages and this documentation,
to tell you that wherever this appears, it assumes the types of the fields you declared in `LIBTABFS_HEADER_DEVICE_FIELDS`:
```c
#define LIBTABFS_HEADER_DEVICE_FIELDS   dev_t __user_dev_t;

// would be

#define LIBTABFS_DEVICE_TYPES           dev_t

// wich then would be:
// void some_function(LIBTABFS_DEVICE_TYPES) => void some_function(dev_t)
```

`LIBTABFS_READ_DEVICE`: macro that gets called when libtabfs wants to read form a device:
It's function needs to be `void LIBTABFS_READ_DEVICE(LIBTABFS_DEVICE_TYPES, long long lba_address, void offset, void* buffer, int bufferSize)`.
The parameter `offset` means the offset into the sector addressed by `lba_address`.