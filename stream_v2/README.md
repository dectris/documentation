# Stream V2

Stream V2 is an interface for pushing series and image data to clients over [ZeroMQ](https://zeromq.org/). It is a redesign of the legacy Stream (V1) interface. Unlike Stream V1, Stream V2 supports pushing multichannel (multi-threshold) image data. It can be enabled and configured using the SIMPLON API.

NOTE: Stream V2 is available starting with release **2022.1**.

#### Stream Version Comparison

| Version | Configuration | Transport | Port | Multipart | Message Encoding |
|---------|---------------|-----------|------|-----------|------------------|
| V1 | SIMPLON | ZeroMQ [PUSH] | 9999 | Yes | JSON + Binary |
| V2 | SIMPLON | ZeroMQ [PUSH] | 31001 | No | [CBOR](https://cbor.io/) |

[PUSH]: https://zeromq.org/socket-api/#push-socket

Stream V2 supersedes Stream V1. Although Stream V1 will continue to be supported, new users are encouraged to use Stream V2.

## Stream V2 Configuration

Stream V2 can be enabled and configured using the SIMPLON API.

#### SIMPLON Configuration Parameters

The configuration interface is accessible at `http://<ADDRESS_OF_DCU>/stream/api/1.8.0/config`.

| Parameter | JSON Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | string | `disabled` | Enabled state of the Stream, either `enabled` or `disabled`. |
| `format` | string | `legacy` | The format of the messages.<br /><ul><li>`legacy`: Use legacy multi-part JSON messages (Stream V1).</li><li>`cbor`: Use CBOR messages (Stream V2).</li></ul> |
| `header_detail` | string | `basic` | Select which fields are sent in start messages.<br /><ul><li>`all`: All fields.</li><li>`basic`: Exclude countrate correction tables, flatfields, and pixel masks.</li><li>`none`: Same as `basic`.</li> |
| `header_appendix` | * |  | User data to be included in start messages. |
| `image_appendix` | * |  | User data to be included in image messages. |

#### Example

Stream V2 may be configured with cURL:

```sh
curl \
    -X PUT \
    -H "Content-Type: application/json" \
    -d '{"mode": {"value": "enabled"}, "format": {"value": "cbor"}}' \
    $ADDRESS_OF_DCU/stream/api/1.8.0/config
```

## Stream V2 Messages

Each Stream V2 message is sent as one ZeroMQ message. Messages are sent to clients using a ZeroMQ PUSH socket. Clients should connect to the server using a ZeroMQ PULL socket over TCP port `31001`. Multiple clients may connect to the same server simultaneously. However, each message is only ever sent to a single client using a round-robin strategy.

The table below describes the available messages:

| Message | `type` | Description |
|---------|--------|-------------|
| Start | `start` | Sent at the start of each series (arm). |
| Image | `image` | Sent once per exposure. |
| End | `end` | Sent at the end of each series (disarm). |

### Message Encoding

Stream V2 messages are encoded as CBOR ([RFC 8949]). Each message is serialized as a top-level CBOR map of text string keys to values of varying types (see "Map" below). Each such top-level map begins with the key `type` that specifies the type of message.

[RFC 8949]: https://www.rfc-editor.org/rfc/rfc8949.html

### Data Model

The Stream V2 data model is based on the CBOR extended [generic data model][CBOR Data Models]. The following table summarizes the types used by Stream V2:

[CBOR Data Models]: https://www.rfc-editor.org/rfc/rfc8949.html#name-cbor-data-models

| Type | CBOR Encoding | Description |
|------|---------------|-------------|
| Any | (any) | Any valid data item. |
| Array(T) | array | A sequence of zero or more data items of type T. |
| Boolean | false / true | A boolean value. |
| DateTime | tag(0) + text string | A standard date/time string defined in [RFC 8949 section 3.4.1]. |
| Float | float | A floating-point value representable by IEEE 754 binary64. |
| Integer | unsigned integer | An unsigned integer representable in 64 bits. |
| Map | map | A mapping from text string keys each to a data item. |
| Map(K -> V) | map | A mapping from keys of type K to values of type V. |
| MultiDimArray | tag(40) + array | A multi-dimensional typed array defined in [RFC 8746 section 3.1]. |
| Rational | array of two unsigned integers | A number expressed as the ratio of two integers. The numerator and denominator are represented as the first and second elements of an array, respectively. The rational number need not be in lowest terms. |
| TextString | text string | A sequence of zero or more Unicode code points. |
| TypedArray | tag + byte string | A typed array defined in [RFC 8746 section 2]. |

[RFC 8746 section 2]: https://www.rfc-editor.org/rfc/rfc8746.html#name-typed-arrays
[RFC 8746 section 3.1]: https://www.rfc-editor.org/rfc/rfc8746.html#name-multi-dimensional-array
[RFC 8949 section 3.4.1]: https://www.rfc-editor.org/rfc/rfc8949.html#stringdatetimesect

### Compression

Compression is realized using CBOR [tag 56500][DECTRIS Compression Tag]. This tag is equivalent to a byte string of the decompressed data. It may appear anywhere a byte string is expected, including in tags that require data items of major type 2.

[DECTRIS Compression Tag]: ../cbor/dectris-compression-tag.md

### Start Message

This message is sent once at the start of a series during the arm command. It contains the relevant series configuration.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "start". |
| `arm_date` | DateTime | Date and time the ARM command was issued. |
| `beam_center_x` | Float | x-position where the direct beam would hit the detector. |
| `beam_center_y` | Float | y-position where the direct beam would hit the detector. |
| `channels` | Array(TextString) | List of active channel names. Image channel data is serialized in this order. |
| `count_time` | Float | Exposure time per image. |
| `countrate_correction_enabled` | Boolean | `true` if count rate correction is enabled. |
| `countrate_correction_lookup_table` | TypedArray | The lookup table used for count rate correction. |
| `detector_description` | TextString | Detector model and type. |
| `detector_serial_number` | TextString | Serial number of the detector. |
| `detector_translation` | Array(Float) | Translation vector of the detector in meters. |
| `flatfield` | Map(TextString -> MultiDimArray) | Map from channel name to flatfield. |
| `flatfield_enabled` | Boolean | `true` if flatfield correction is enabled. |
| `frame_time` | Float | Time interval between start of image acquisitions. |
| `goniometer` | Map | Goniometer parameters. |
| &emsp;&emsp;_AXIS_ | Map | Any goniometer axis. |
| &emsp;&emsp;&emsp;&emsp;`increment` | Float | Step increment for _AXIS_. |
| &emsp;&emsp;&emsp;&emsp;`start` | Float | Starting value for _AXIS_. |
| `image_size_x` | Integer | The x-size (width) of each image in pixels. |
| `image_size_y` | Integer | The y-size (height) of each image in pixels. |
| `incident_energy` | Float | Configured energy of incident particles (e.g. X-rays) in eV. |
| `incident_wavelength` | Float | Wavelength of incident particles (e.g. X-rays) in angstroms. |
| `number_of_images` | Integer | Number of images in the series. |
| `pixel_mask` | Map(TextString -> MultiDimArray) | Map from channel name to 32-bit pixel mask. |
| `pixel_mask_enabled` | Boolean | `true` if pixel masking is enabled. |
| `pixel_size_x` | Float | Physical size of a pixel in the x-direction. |
| `pixel_size_y` | Float | Physical size of a pixel in the y-direction. |
| `saturation_value` | Integer | Maximum valid sample value (excluding the NODATA value). |
| `sensor_material` | TextString | Material used for direct detection in the sensor. |
| `sensor_thickness` | Float | Thickness of the sensor material. |
| `series_id` | Integer | The series number. |
| `series_unique_id` | TextString | The unique ID of the series. |
| `threshold_energy` | Map(TextString -> Float) | Map from channel name to energy (eV). |
| `user_data` | Any | Additional data specified by the user. |
| `virtual_pixel_interpolation_enabled` | Boolean | `true` if virtual-pixel interpolation is enabled. |

### Image Message

This message is sent for every exposure. Each message contains all active channel (threshold) data of the exposure.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "image". |
| `data` | Map(TextString -> MultiDimArray) | The map of channel data. |
| `image_id` | Integer | The image number. |
| `real_time` | Rational | Real exposure time of the image in seconds. The denominator is the time base frequency which is constant for the duration of the series. |
| `series_date` | DateTime | Date and time of the start of the series. |
| `series_id` | Integer | The series number. |
| `series_unique_id` | TextString | The unique ID of the series. |
| `start_time` | Rational | Start time of the image in seconds, relative to `series_date`. The denominator is the time base frequency which is constant for the duration of the series. |
| `stop_time` | Rational | Stop time of the image in seconds, relative to `series_date`. The denominator is the time base frequency which is constant for the duration of the series. |
| `user_data` | Any | Additional data specified by the user. |

### End Message

This message signifies the end of a series.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "end". |
| `series_id` | Integer | The series number. |
| `series_unique_id` | TextString | The unique ID of the series. |
