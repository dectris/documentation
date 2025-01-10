# CBOR Tag for DECTRIS Compressed Byte Strings

## Summary

This document describes CBOR tag 56500 which represents a compressed byte string.

* Tag: 56500
* Data Item: Array (major type 4).
* Semantics: Compressed byte string.
* Contact: Dirk Boye <<dirk.boye@dectris.com>>

## Semantics

Tag 56500 describes a byte string of compressed data. The compression algorithm used is represented in the tag content.

Compression algorithms commonly used with DECTRIS products are specified (see [Compression Algorithms](#compression-algorithms)). Additional compression algorithms may be added in the future.

### Data Item

The tag content is an array of three elements: a text string specifying the compression algorithm used; an algorithm-dependent modifier item; and a byte string of the compressed data.

### Equivalence

This tag is equivalent to a byte string of the decompressed data. In particular, it MAY be used with tags that require data items of major type 2, as allowed by the CBOR protocol (see [Tag Equivalence]).

[Tag Equivalence]: https://www.ietf.org/archive/id/draft-ietf-cbor-packed-08.html#name-tag-equivalence

## Compression Algorithms

### Bitshuffle + LZ4 (HDF5 framing)

| Algorithm | Modifier | Data |
|:---------:|----------|------|
| `"bslz4"` | Integer specifying the bitshuffle element size. | Byte string of bitshuffle-transposed blocks each compressed using LZ4. The format is the same as the bitshuffle HDF5 filter. |

```
#6.56500(["bslz4", 1, ...])
#6.56500(["bslz4", 2, ...])
#6.56500(["bslz4", 4, ...])
```

<https://github.com/kiyo-masui/bitshuffle>

<https://github.com/kiyo-masui/bitshuffle/blob/f57e2e50cc6855d5cf7689b9bc688b3ef1dffe02/src/bshuf_h5filter.c>

<https://lz4.github.io/lz4/>

<https://github.com/lz4/lz4/blob/master/doc/lz4_Block_format.md>

### LZ4 (HDF5 framing)

| Algorithm | Modifier | Data |
|:---------:|----------|------|
| `"lz4"` | Unused. | Byte string of LZ4 compressed blocks. The LZ4 filter format for HDF5 is used for framing. |

The modifier item is unused with this compression algorithm. The zero integer is serialized as a placeholder.

```
#6.56500(["lz4", 0, ...])
```

<https://lz4.github.io/lz4/>

<https://github.com/lz4/lz4/blob/master/doc/lz4_Block_format.md>

<https://support.hdfgroup.org/services/filters/HDF5_LZ4.pdf>

## Example

### Decompression

`pip install cbor2 dectris-compression~=0.3.0`

```python
import cbor2
from dectris.compression import decompress

def tag_hook(decoder, tag):
    if tag.tag == 56500:
        algorithm, elem_size, encoded = tag.value
        return decompress(encoded, algorithm, elem_size=elem_size)
    else:
        return tag

cbor2.loads(message, tag_hook=tag_hook)
```
