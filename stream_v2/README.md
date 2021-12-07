# Stream V2 - Alpha

```diff
- NOTE: Stream V2 is currently a preview product. The interface is not final and will change.
```

Stream V2 is available starting with version release-2020.2.4. If you are interested in testing this new feature please contact our support team by email: support@dectris.com

Stream V2 is an interface for pushing series and image data to clients over [ZeroMQ](https://zeromq.org/). It is a redesign of the legacy stream (V1) interface. Unlike stream V1, stream V2 supports pushing multichannel (multi-threshold) image data. It can be enabled and configured using the SIMPLON API.

#### Stream Version Comparison

| Version | Configuration | Transport | Multipart | Message Encoding |
|---------|---------------|-----------|-----------|------------------|
| V1 | SIMPLON | ZeroMQ PUSH | Yes | JSON + Binary |
| V2 | SIMPLON | ZeroMQ PUSH | No | [CBOR](https://cbor.io/) |

Stream V2 supersedes stream V1. Although stream V1 will continue to be supported, new customers are encouraged to use stream V2.

## Stream V2 Configuration

Stream V2 can be enabled and configured using the SIMPLON API. The configuration interface is accessible at `http://<ADDRESS_OF_DCU>/api/2-preview/stream/config/<PARAMETER>`.

#### SIMPLON Configuration Parameters

| Parameter | Type | Access | Default | Description |
|-----------|------|--------|---------|-------------|
| `enabled`      | `bool`   | `rw` | `false` | Enabled state of the stream. |
| `start_fields` | `string` | `rw` | `default` | Select which fields are sent in start event messages.<br /><ul><li>`all`: Send all available fields.</li><li>`default`: Send available fields except for `countrate_correction`, `flatfield`, and `pixel_mask`.</li></ul> |

#### CURL Examples

Stream V2 may be enabled with cURL:
```sh
curl -X POST -H "Content-Type: application/json" $ADDRESS_OF_DCU/api/2-preview/stream/config/enabled/value -d 'true'
```
By default, large fields such as flatfields are not sent in start event messages. To enable all fields use the following command:
```sh
curl -X POST -H "Content-Type: application/json" $ADDRESS_OF_DCU/api/2-preview/stream/config/start_fields/value -d '"all"'
```

## Stream V2 Messages

Each stream V2 message is sent as one ZeroMQ message. Messages are sent to clients using a ZeroMQ PUSH socket. Clients should connect to the server using a ZeroMQ PULL socket over TCP port `31001`. Multiple clients may connect to the same server simultaneously. However, each message is only ever sent to a single client using a round-robin strategy.

The table below describes the available messages:

| Message | `type` | Description |
|---------|--------|-------------|
| Start | `start` | Sent at the start of each series (arm). |
| Image | `image` | Sent once per exposure. |
| End | `end` | Sent at the end of each series (disarm). |

### Message Encoding

Stream V2 messages are encoded as CBOR ([RFC 8949](https://www.rfc-editor.org/rfc/rfc8949.html)). Each message is serialized as a top-level CBOR map of text string keys to values of varying types (see "Map" below). Each such top-level map begins with the key `type` that specifies the type of message (event).

### Data Model

The stream V2 data model is based on the CBOR extended [generic data model](https://www.rfc-editor.org/rfc/rfc8949.html#name-cbor-data-models). The following table summarizes the types used by stream V2:

| Type | CBOR Encoding | Description |
|------|---------------|-------------|
| Array(T) | array | A sequence of zero or more data items of type T |
| Boolean | False / True | A boolean value |
| DateTime | tag(0) + text string | A standard date/time string defined in [RFC 8949 section 3.4.1](https://www.rfc-editor.org/rfc/rfc8949.html#stringdatetimesect) |
| Float | float | A floating-point value representable by IEEE 754 binary64 |
| Integer | unsigned integer | An unsigned integer representable in 64 bits |
| Map | map | A mapping from text string keys each to a data item |
| Map(K -> V) | map | A mapping from keys of type K to values of type V |
| MultiDimArray | tag + array |  A multi-dimensional typed array defined in [RFC 8746 section 3.1](https://www.rfc-editor.org/rfc/rfc8746.html#name-multi-dimensional-array) |
| TextString | text string | A sequence of zero or more Unicode code points |
| TypedArray | tag + byte string | A typed array defined in [RFC 8746 section 2](https://www.rfc-editor.org/rfc/rfc8746.html#name-typed-arrays) |

### Start Message

This message is sent once at the start of a series during the arm command. It contains the relevant series configuration.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "start" |
| `beam_center_x` | Float | x-position where the direct beam would hit the detector |
| `beam_center_y` | Float | y-position where the direct beam would hit the detector |
| `channels` | Array(Map) | Information about each data channel |
| &emsp;&emsp;`data_type` | TextString | The type of the channel data |
| &emsp;&emsp;`thresholds` | Array(Integer) | Threshold identifier(s) of the channel. For difference mode there are two threshold IDs. |
| `count_time` | Float | Exposure time per image |
| `countrate_correction` | TypedArray | Count rate correction table |
| `countrate_correction_applied` | Boolean | `True` if count rate correction was applied |
| `detector_description` | TextString | Detector model and type |
| `detector_distance` | Float | Sample to detector distance |
| `detector_serial_number` | TextString | Serial number of the detector |
| `flatfield` | Map(Integer -> MultiDimArray) | Map from threshold ID to flatfield |
| `flatfield_applied` | Boolean | `True` if thee flatfield was applied |
| `frame_time` | Float | Time interval between start of image acquisitions |
| `goniometer` | Map | Goniometer parameters |
| &emsp;&emsp;_AXIS_ | Map | Any goniometer axis |
| &emsp;&emsp;&emsp;&emsp;`increment` | Float | Step increment for _AXIS_ |
| &emsp;&emsp;&emsp;&emsp;`start` | Float | Starting value for _AXIS_ |
| `image_size` | Array(Integer) | Dimensions of each image |
| `images_per_trigger` | Integer | Images per trigger |
| `incident_energy` | Float | Configured energy of incident particles (e.g. X-rays) in eV |
| `incident_wavelength` | Float | Wavelength of incident particles (e.g. X-rays) in angstroms |
| `number_of_images` | Integer | Number of images in the series |
| `number_of_triggers` | Integer | Number of triggers |
| `pixel_mask` | Map(Integer -> MultiDimArray) | Map from threshold ID to 32-bit pixel mask |
| `pixel_mask_applied` | Boolean | `True` if the pixel mask correction was applied |
| `pixel_size_x` | Float | Physical size of a pixel in the x-direction |
| `pixel_size_y` | Float | Physical size of a pixel in the y-direction |
| `roi_mode` | TextString | Configured ROI (region of interest) mode |
| `saturation_value` | Integer | Maximum valid sample value (excluding the NODATA value) |
| `sensor_material` | TextString | Material used for direct detection in the sensor |
| `sensor_thickness` | Float | Thickness of the sensor material |
| `series_date` | DateTime | Date and time of data collection. This is the time when the ARM command was issued. |
| `series_number` | Integer | The series number |
| `series_unique_id` | TextString | The unique ID of the series |
| `threshold_energy` | Map(Integer -> Float) | Map from threshold ID to energy (eV) |
| `virtual_pixel_correction_applied` | Boolean | `True` if virtual-pixel correction was applied |

### Image Message

This message is sent for every exposure. Each message contains all active channel (threshold) data of the exposure.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "image" |
| `channels` | Array(Map) | The sequence of data channels |
| &emsp;&emsp;`compression` | TextString | Compression type (e.g. "bslz4" or "lz4") |
| &emsp;&emsp;`data_type` | TextString | Type of the channel data |
| &emsp;&emsp;`lost_pixel_count` | Integer | Count of lost pixels |
| &emsp;&emsp;`thresholds` | Array(Integer) | Threshold identifier(s) of the channel. For difference mode there are two threshold IDs. |
| &emsp;&emsp;`data` | MultiDimArray (untagged) | Channel data (compressed) |
| `hardware_exposure_time` | Array(Integer) | Hardware exposure time of the image |
| `hardware_start_time` | Array(Integer) | Hardware start time of the image |
| `hardware_stop_time` | Array(Integer) | Hardware stop time of the image |
| `image_number` | Integer | The image number |
| `series_number` | Integer | The series number |
| `series_unique_id` | TextString | The unique ID of the series |

### End Message

This message signifies the end of a series.

| Field | Type | Description |
|-------|------|-------------|
| `type` | TextString | The value "end" |
| `series_number` | Integer | The series number |
| `series_unique_id` | TextString | The unique ID of the series |
