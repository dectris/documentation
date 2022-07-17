import cbor2
from dectris.compression import decompress
import numpy as np


def decode_multi_dim_array(tag, column_major):
    dimensions, contents = tag.value
    if isinstance(contents, list):
        array = np.empty((len(contents),), dtype=object)
        array[:] = contents
    elif isinstance(contents, (np.ndarray, np.generic)):
        array = contents
    else:
        raise cbor2.CBORDecodeValueError("expected array or typed array")
    return array.reshape(dimensions, order="F" if column_major else "C")


def decode_typed_array(tag, dtype):
    if not isinstance(tag.value, bytes):
        raise cbor2.CBORDecodeValueError("expected byte string in typed array")
    return np.frombuffer(tag.value, dtype=dtype)


def decode_dectris_compression(tag):
    algorithm, elem_size, encoded = tag.value
    return decompress(encoded, algorithm, elem_size=elem_size)


tag_decoders = {
    40: lambda tag: decode_multi_dim_array(tag, column_major=False),
    64: lambda tag: decode_typed_array(tag, dtype="u1"),
    65: lambda tag: decode_typed_array(tag, dtype=">u2"),
    66: lambda tag: decode_typed_array(tag, dtype=">u4"),
    67: lambda tag: decode_typed_array(tag, dtype=">u8"),
    68: lambda tag: decode_typed_array(tag, dtype="u1"),
    69: lambda tag: decode_typed_array(tag, dtype="<u2"),
    70: lambda tag: decode_typed_array(tag, dtype="<u4"),
    71: lambda tag: decode_typed_array(tag, dtype="<u8"),
    72: lambda tag: decode_typed_array(tag, dtype="i1"),
    73: lambda tag: decode_typed_array(tag, dtype=">i2"),
    74: lambda tag: decode_typed_array(tag, dtype=">i4"),
    75: lambda tag: decode_typed_array(tag, dtype=">i8"),
    77: lambda tag: decode_typed_array(tag, dtype="<i2"),
    78: lambda tag: decode_typed_array(tag, dtype="<i4"),
    79: lambda tag: decode_typed_array(tag, dtype="<i8"),
    80: lambda tag: decode_typed_array(tag, dtype=">f2"),
    81: lambda tag: decode_typed_array(tag, dtype=">f4"),
    82: lambda tag: decode_typed_array(tag, dtype=">f8"),
    83: lambda tag: decode_typed_array(tag, dtype=">f16"),
    84: lambda tag: decode_typed_array(tag, dtype="<f2"),
    85: lambda tag: decode_typed_array(tag, dtype="<f4"),
    86: lambda tag: decode_typed_array(tag, dtype="<f8"),
    87: lambda tag: decode_typed_array(tag, dtype="<f16"),
    1040: lambda tag: decode_multi_dim_array(tag, column_major=True),
    56500: lambda tag: decode_dectris_compression(tag),
}


def tag_hook(decoder, tag):
    tag_decoder = tag_decoders.get(tag.tag)
    return tag_decoder(tag) if tag_decoder else tag


if __name__ == "__main__":
    from pprint import pprint
    import sys
    import zmq

    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} HOSTNAME")

    context = zmq.Context()
    endpoint = f"tcp://{sys.argv[1]}:31001"
    socket = context.socket(zmq.PULL)

    with socket.connect(endpoint):
        print(f"PULL {endpoint}")
        while True:
            message = socket.recv()
            message = cbor2.loads(message, tag_hook=tag_hook)
            print(f"========== MESSAGE[{message['type']}] ==========")
            pprint(message, sort_dicts=False)
