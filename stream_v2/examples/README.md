# Stream V2 Examples

## C

`stream2.c` and `stream2.h` implement a stream V2 parser using [tinycbor]. `example.c` uses this parser to dump received messages to stdout. [dectris-compression] is used to decompress image channel data.

The code requires compiler support for half-float conversions. Any C compiler supporting C11 extension ISO/IEC TS 18661-3 will work. Otherwise, x86-64 intrinsics for SSE2 and F16C are required. If the code does not work with your compiler, please let us know.

#### Building

To get started, make sure that the submodules are initialized recursively:

```sh
git submodule update --init --recursive
```

Building the examples requires libzmq. By default, CMake will try to locate and use an installed version of libzmq. To download and build libzmq from source, set the CMake variable `BUILD_LIBZMQ` to `YES`.

Let `documentation/` be the location where this repository is cloned recursively. Build and run the example with:

```sh
mkdir examples_build
cd examples_build
cmake ../documentation/stream_v2/examples -DCMAKE_BUILD_TYPE=Debug -DBUILD_LIBZMQ=YES
cmake --build .
./example
```

## Python

`client.py` demonstrates how to receive and decode stream V2 data using Python 3. Fields of type `MultiDimArray` and `TypedArray` are represented as `numpy` arrays.

```sh
pip install cbor2~=6.0.0 dectris-compression~=0.3.0 numpy pyzmq
python client.py
```

Please note that this example requires cbor2 version 6.0.0 or later to work. An example compatible with older versions can be found in the branch [cbor2_version_below_6_0_0].

[dectris-compression]: https://github.com/dectris/compression
[tinycbor]: https://github.com/intel/tinycbor
[cbor2_version_below_6_0_0]: https://github.com/dectris/documentation/tree/cbor2_version_below_6_0_0
