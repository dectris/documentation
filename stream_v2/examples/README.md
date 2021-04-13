# Stream V2 Examples

## C

`stream2.c` and `stream2.h` implement a generic stream V2 parser using [tinycbor](https://github.com/intel/tinycbor). `example.c` uses this parser to dump received messages to stdout. The example depends on [dectris-compression](https://github.com/dectris/compression) to decompress image channel data.

The code uses x86-64 intrinsics to handle conversions from half-floats and requires a processor with SSE2 and F16C support. If you are using a processor that does not support these extensions, please let us know and we will add support for your processor.

#### Building

To get started, make sure that the submodules are initialized recursively:

```sh
git submodule update --init --recursive
```

Building the examples requires libzmq. By default, CMake will try to locate and use an installed version of libzmq. To download and build libzmq from source, set the CMake variable `BUILD_LIBZMQ` to `YES`.

Let `documentation` be the location where this repository is cloned recursively. Build and run the example with:

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
pip install cbor2 dectris-compression numpy zmq
python client.py
```
