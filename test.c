#define LIBTABFS_ALLOC
#include "libtabfs.h"

#include <stdio.h>

int main() {

    int i = sizeof(struct tabfs_header);
    printf("%d\n", i);

}