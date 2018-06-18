# LRC-Erasure-Code

LRC(Local Reconstruction Codes) Erasure Code based on Reed-Solomon with Vandermonde matrix.

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Status](#status)
- [Description](#description)
  - [LRC parameters and the differences from original Erasure Code](#lrc-parameters-and-the-differences-from-original-erasure-code)
- [Synopsis](#synopsis)
- [Install](#install)
- [API](#api)
  - [lrc_init_n](#lrc_init_n)
  - [lrc_destroy](#lrc_destroy)
  - [lrc_encode](#lrc_encode)
  - [lrc_decode](#lrc_decode)
  - [lrc_get_source](#lrc_get_source)
  - [lrc_buf_init](#lrc_buf_init)
  - [lrc_buf_destroy](#lrc_buf_destroy)
- [Analysis](#analysis)
  - [Reliability](#reliability)
  - [IO bandwidth](#io-bandwidth)
- [TODO](#todo)
- [Author](#author)
- [Copyright and License](#copyright-and-license)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Status

This library is considered production ready.

And it is the core EC implementation in [open.sinastorage.com](http://open.sinastorage.com), which has been protecting dozens of PB user data.

# Description

LRC(Local Reconstruction Codes) Erasure Code supplies almost the same functionality and reliability as original Erasure Code does.
And at the same time it reduces reconstruction IO consumption by 50% or more.

Erasure Code Algorithm makes it possible to achieve as high reliability(11 9s)
as 3-copy replication provides, with highly reduced storage overhead(130% against 300%).

But one of the problems with Erasure Code is the high IO consumption during
data reconstruction.
Normally to reconstruct `1` chunk it is required to read `n` chunks.

LRC is a trade-off between storage cost and IO cost.

With several additional **local** `Coding` chunks calculated from subsets of `Data`
chunks, average IO consumption for reconstruction would be reduced to
`1 / number_of_local_sets`(normally 10% ~ 50%), at the cost of only about 10%(depends on LRC policy) more space used.

## LRC parameters and the differences from original Erasure Code

For a collection there are 5 data chunks in it.
To create LRC Erasure Code with:

*   2 local EC codes;
*   2 global EC codes;

LRC should be initialized with:

`lrc_init_n(lrc, 2, (uint8_t[]){3, 2}, 4)`

Here in this example, the first local EC code will be created from the 1st 3
data chunks, and the 2nd local EC code will be created from the last 2 data
chunks.

4 is the total number of codes, which includes:
*   2 of them are local EC codes, for data[0, 1, 2] and data[3, 4] respectively.
*   2 additional global EC codes.

The encoding matrix for this LRC parameter is:

```
1  1  1  0  0
0  0  0  1  1
1  2  4  8 16
1  3  9 27 81
```

LRC-EC `(2,2)+4` is identical to original EC `5+3`, except that it splits the first row
into 2 rows(which makes it possible to use less data/code chunks to reconstruct one).
The original EC `5+3` encoding matrix is:

```
1  1  1  1  1
1  2  4  8 16
1  3  9 27 81
```

If you prefer to use original EC `5+3` like above, `lrc` can be initialized with:

`lrc_init_n(lrc, 1, (uint8_t[]){5}, 3)`

# Synopsis

```c
#include "lrc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

    int        size = 16;
    lrc_t     *lrc  = &(lrc_t) {0};
    lrc_buf_t *buf  = &(lrc_buf_t) {0};

    if (lrc_init_n(lrc, 2, (uint8_t[]) {2, 2}, 3) != 0) {
        exit(-1);
    }

    if (lrc_buf_init(buf, lrc, size) != 0) {
        exit(-1);
    }

    strcpy(buf->data[0], "hello");
    strcpy(buf->data[1], "world");
    strcpy(buf->data[2], "lrc");
    strcpy(buf->data[3], "ec");

    if (lrc_encode(lrc, buf) != 0) {
        exit(-1);
    }

    strcpy(buf->data[0], "*");

    printf("damaged: %s %s %s %s\n", buf->data[0], buf->data[1], buf->data[2], buf->data[3]);

    int8_t erased[2 + 2 + 3] = {
        1, 0,
        0, 0,
        0, 0, 0};

    if (lrc_decode(lrc, buf, erased) != 0) {
        exit(-1);
    }

    printf("reconstructed: %s %s %s %s\n", buf->data[0], buf->data[1], buf->data[2], buf->data[3]);

    lrc_destroy(lrc);
    lrc_buf_destroy(buf);

    return 0;
}
```

# Install

```shell
./configure
make
sudo make install

# run a test
cd test
gcc example.c -o example -llrc
./example
```

# API

## lrc_init_n

`int lrc_init_n(lrc_t *lrc, int n_local, uint8_t *local_arr, int m)`

Initializes LRC descriptor `lrc`.

Parameters:

* `lrc`
Pointer to a struct `lrc_t`. a `lrc_t` describes the parameters LRC to
generate codes.

* `n_local`
Specify the number of local EC to create.

* `local_arr`
An array of length `n_local` of number of data chunks in each local EC.

* `m`
Specifies the total number of codes. It must be equal or greater than `n_local`.
Thus there are `n_local` local EC codes and `m - n_local + 1` global EC codes.
Because the first global EC code can be calculated by `local-code-1 ^ local-code-2 ^ ...`

Returns:

* `0`
If Success.

* `LRC_INIT_TWICE`
If `lrc` is already initialized.

* `LRC_INVALID_M`
If `m` is less than `n_local`.

* `LRC_OUT_OF_MEMORY`
If any `malloc()` fails during initializing.

## lrc_destroy

`void lrc_destroy(lrc_t *lrc);`

Free memory allocated by `lrc_init_n()`. It does not free `*lrc` itself.

## lrc_encode

`int lrc_encode(lrc_t *lrc, lrc_buf_t *lrc_buf);`

Generate `m`(from `lrc_init_n()`) code chunks from all `k` data chunks. `k = sum(local_arr)`.
`lrc_buf_t` is the container of all data chunks and code chunks. It must be
initialized with `lrc_buf_init()` before use.

After `lrc_encode()`, your program should save `lrc_buf->data[0..k-1]` and
`lrc_buf->code[0..m-1]` on persistent storage for later reconstruction.

Returns:

* `0`
If Success.

* `LRC_OUT_OF_MEMORY`
If any `malloc()` fails during initializing.

## lrc_decode

`int lrc_decode(lrc_t *lrc, lrc_buf_t *lrc_buf, int8_t *erased);`

Reconstruct lost data and code chunks from existing data and code.

If too many data or code are lost, reconstruction

Parameters:

* `lrc_buf`
Specifies data/code buffer for reconstruction and the buffer to store
reconstructed data/code.

* `erased`
Specifies which data / code are missing that needs to reconstruct.
It is an array of length `k + m`.
Array element `erased[i]` value `1` means the data(`i<k`) or code(`i<=k<m`) is missing,
`0` means data/code presents in `lrc_buf->data[i]` or `lrc_buf->code[i-k]`.

Returns:

* `0`
If Success.

* `LRC_OUT_OF_MEMORY`
If any `malloc()` fails during decoding.

* `LRC_UNRECOVERABLE`
If there is not enough data / code to reconstruct the missing ones.

## lrc_get_source

`int lrc_get_source(lrc_t *lrc, int8_t *erased, int8_t *source);`

If LRC is used(`n_local` passed to `lrc_init_n()` is greater than 1), not
always all data/code are required.
This function calculate which data/code is required.

For example if LRC parameter is `2, 2, 3`, and `erased = {1, 0, 0, 0, 0, 0, 0}`
which means only 0-th data is missing, `source` will be filled in with: `{0, 1, 0, 0, 1, 0, 0}`
which means only `data[1], code[0]` are required to reconstruct the missing
`data[0]`.

Parameters:

* `erased`
Specifies missing data/code. There must be at least `k+m` 0/1 elements in
`erased`.

* `source`
Specifies where to store the indexes of source data/code for reconstruction.
There must be at least `k+m` available bytes in `source`.

Returns:

* `0`
If Success.

* `LRC_UNRECOVERABLE`
If there is not enough data / code to reconstruct the missing ones.

## lrc_buf_init

`int  lrc_buf_init(lrc_buf_t *lrc_buf, lrc_t *lrc, int64_t chunk_size);`

Allocate memory that will be used during reconstruction,
which includes: `k+m` byte arrays and a matrix for reconstruction.

Parameters:

* `lrc`
Specifies LRC parameters. It must have been initialized by `lrc_init_n()` first.

* `chunk_size`
Specifies the size for each of `k+m` data/code buffers.
Internally, actual memory allocated is 16 byte aligned in order to utilize SMID
instructions.

Returns:

* `0`
If Success.

* `LRC_INIT_TWICE`
If `lrc` is already initialized.

* `LRC_OUT_OF_MEMORY`
If any `malloc()` fails during initializing.

## lrc_buf_destroy

`void lrc_buf_destroy(lrc_buf_t *lrc_buf);`

Free memory allocated by `lrc_buf_init()`.
It does not free `lrc_buf`.


This is a specialized Erasure Code implementation for storage service.
What matrix to choose does not matter.
Because usually most CPU cycles are spent on matrix multiplication to decode lost data,
but not on finding reversed matrix.

In this implementation Vandermonde matrix is used.

# Analysis

## Reliability

*   If `k`(number of data chunks) is not very large, reliability of Erasure Code(LRC-EC or EC) with `m` code is similar with n-copy replication with `m+1` copies.

*   LRC-EC can always reconstruct `m - n_local + 1` data loss. In a `(6,6)+4`
LRC-EC, 3 data loss is always reconstructible.

*   LRC-EC with `m` codes can not always reconstruct `m` data loss.
In a `(6,6)+4` LRC, there are 1820 different combinations but only 1568 of
them can be reconstructed(87%).

## IO bandwidth

In calculation, each TB of storage requires
`k * 0.13G` IO throughput(both for network and disk drive) each day
to reconstruct lost data.
Where `k` is the number of members in a Erasure Code group.

# TODO

*   Another local code that covers all global codes.

# Author

Zhang Yanpo (张炎泼) <drdr.xp@gmail.com>

# Copyright and License

The MIT License (MIT)

Copyright (c) 2015 Zhang Yanpo (张炎泼) <drdr.xp@gmail.com>
