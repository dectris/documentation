# FileWriter V2

---

> **Warning**
>
> FileWriter V2 is currently in a preview state and **will change** in the future.

Feedback is very welcome!

---

FileWriter writes image data and metadata to [HDF5](https://www.hdfgroup.org/solutions/hdf5/) files. Currently, there are two different formats available. V1 is a legacy format that follows [version 1.4](https://github.com/nexusformat/definitions/releases/tag/NXmx-1.4) of the _NXmx application definition_ of the [NeXus](https://manual.nexusformat.org) format. V2 follows the [_NXmx application definition_](https://manual.nexusformat.org/classes/applications/NXmx.html) of NeXus Version 2022.07, with the addition of multi-channel data support[^1].

| Version | Configuration | NeXus/NXmx Version | Multi-Channel Data |
|----|----|----|----|
| V1 | SIMPLON | [NeXus 3.2 NXmx (2016)](https://github.com/nexusformat/definitions/blob/main/legacy_docs/nexus-v3.2-2016-11-22-32b130a.pdf) | No |
| V2 | SIMPLON | [NeXus 2022.07 NXmx](https://github.com/nexusformat/definitions/blob/v2022.07/applications/NXmx.nxdl.xml) | Yes |

[^1]: This is not part of the NeXus standard yet, a detailed PR will be opened when this data format has been approved by the community. The general idea of this representation has already been discussed and approved by the NIAC though, see this [issue](https://github.com/nexusformat/definitions/issues/940).


## FileWriter V2 Configuration

FileWriter V2 can be enabled and configured using the SIMPLON API.

### SIMPLON Configuration Parameters

The configuration interface is accessible at `http://<ADDRESS_OF_DCU>/filewriter/api/1.8.0/config`.

| Parameter | JSON Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | string | `"disabled"` | Enabled state of the FileWriter, either `"enabled"` or `"disabled"`. |
| `compression_enabled` | boolean | `true` | Optionally disable compression of image data. |
| `image_nr_start` | number | `1` | Unsigned integer defining the first image id on the axis `/entry/image_id`. |
| `name_pattern` | string | `"series_$id"` | Base name of the files. `$id` will be replaced by the series id, i.e. using the default, the generated files will have the names:<br> `series_<series_id>_master.h5`<br> `series_<series_id>_data_<file_number>.h5`|
| `nimages_per_file` | number | `1000` | Unsigned integer defining the maximum number of images stored in each data file. If set to `0`, all images are stored directly in the master file and no data files are created. If set to a value greater than zero, the images are stored in multiple data files. |
| `v2_preview_enabled` | boolean | `false` | Enabled state of the FileWriter V2 preview. <br>**NOTE:** This key will be replaced when FileWriter V2 is released. There will likely be a `format` key instead, in analogy to Stream V2. |

### Example

FileWriter V2 may be configured with cURL.

```sh
curl \
    -X PUT \
    -H "Content-Type: application/json" \
    -d '{"mode": {"value": "enabled"}, "v2_preview_enabled": {"value": true}}' \
    $ADDRESS_OF_DCU/filewriter/api/1.8.0/config
```

## Data Access

Files are stored locally (in memory) on the DCU and may be accessed over HTTP.

> **Warning**
>
> Files are **not** stored persistently and all data will be lost on reboot.

```sh
# list available files
curl $ADDRESS_OF_DCU/filewriter/api/1.8.0/status/files

# file access
curl -O $ADDRESS_OF_DCU/data/<name_pattern>_master.h5
curl -O $ADDRESS_OF_DCU/data/<name_pattern>_data_<file_number>.h5
```

For further details on available commands and status parameters please refer to the [SIMPLON API documentation](https://media.dectris.com/210607-DECTRIS-SIMPLON-API-Manual_EIGER2-chip-based_detectros.pdf).

## Multi-Channel Data

One of the main new features of FileWriter V2 is the support of multi-channel data, i.e. each image consists of multiple channels, one per enabled threshold/difference mode. The dataset `/entry/data/data` is four-dimensional with shape `[nP, nC, i, j]`, the notation used is defined [here](#notation). The channels are identified via strings given in the `channel` [axis](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-field) of the _NXdata_ class[^2]. For every channel name listed in this field, there exists a subgroup of the [_NXdetector_]() group of type _NXdetector_channel_[^1] with the same name followed by the suffix `_channel`. This group contains channel-specific metadata, for example `threshold_energy`, `flatfield` or `pixel_mask`.

An _NXdetector_channel_ group could contain every field that is allowed for NXdetector and that makes sense to be given per channel.

[^2]: Currently, _AXISNAME_ is defined to be of type [_NX_NUMBER_](https://manual.nexusformat.org/nxdl-types.html#nx-number), but there is an [open PR](https://github.com/nexusformat/NIAC/issues/97) to allow [_NX_CHAR_](https://manual.nexusformat.org/nxdl-types.html#nx-char) as well.

#### Example

For example, if `threshold_1`, `threshold_2` and `difference` are enabled, we would get the following:

```
data: NXdata
  @signal = "data"
  @axes = ["image_id", "channel", ".", "."]
  @image_id_indices = 0
  @channel_indices = 1
  image_id = [1, ..., nP]
  channel = ["threshold_1", "threshold_2", "difference"]
  data = uint[i, j]

detector: NXdetector
  ...
  threshold_1_channel: NXdetector_channel
    threshold_energy = float
    flatfield = float[i, j]
    pixel_mask = uint[i, j]
  threshold_2_channel: NXdetector_channel
    threshold_energy = float
    flatfield = float[i, j]
    pixel_mask = uint[i, j]
  difference_channel: NXdetector_channel
    threshold_energy = float[2]
```

## File Structure

A FileWriter V2 file is structured into the following (HDF5) groups:

```
/
|
+-- entry
    |
    +-- data
    +-- instrument
    |   |
    |   +-- beam
    |   +-- detector
    |       |
    |       +-- threshold_1_channel*
    |       +-- threshold_2_channel*
    |       +-- difference_channel*
    |       +-- geometry
    |       +-- module
    |       +-- transformations
    |
    +-- sample
    |   |
    |   +-- transformations
    |   +-- beam
    |
    +-- source
```
\* this is not part of the NeXus standard yet, see [Multi-Channel Data](#multi-channel-data).

## List of Fields

A file generated within V2 format may contain the fields and attributes listed below. For further details and descriptions please refer to the provided links to the NeXus definitions.

#### Notation

The following symbols will be used (in accordance with the NeXus documentation):

**nP**: Number of images<br>
**nC**: Number of channels<br>
**i**: Height of one image<br>
**j**: Width of one image


### / (root)

| Name | NeXus Type | Description |
|---|---|---|
| [@default](https://manual.nexusformat.org/classes/base_classes/NXroot.html#nxroot-default-attribute) | NX_CHAR | Declares which NXentry group contains the data to be shown by default. |

### [/entry](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-group)

The top-level NeXus group contains all the data and associated information that comprise a single measurement.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXentry"`](https://manual.nexusformat.org/classes/base_classes/NXentry.html#nxentry)
| [@default](https://manual.nexusformat.org/classes/base_classes/NXentry.html#nxentry-default-attribute) | NX_CHAR | Declares which NXdata group contains the data to be shown by default. |
| [@version](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-version-attribute) | NX_CHAR | Describes the version of the NXmx definition used to write this data. |
| [definition](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-definition-field) | NX_CHAR | NeXus NXDL schema to which this file conforms, `"NXmx"`. |
| [end_time_estimated](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-end-time-estimated-field) | NX_DATE_TIME | Estimated time/date of the last data point collected in UTC, following ISO 8601 with suffix Z. Note that the time zone of the beamline is provided in `/entry/instrument/time_zone`. |
| [program_name](https://manual.nexusformat.org/classes/base_classes/NXentry.html#nxentry-program-name-field) | NX_CHAR | Name of the program used to generate this file, `"DECTRIS-DAQ"`.
| [program_name@version](https://manual.nexusformat.org/classes/base_classes/NXentry.html#nxentry-program-name-version-attribute) | NX_CHAR | Version number of the program used to generate this file. |
| [start_time](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-start-time-field) | NX_DATE_TIME | ISO 8601 time/date of the first data point collected in UTC. |

## [/entry/data](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-data-group)

Describes the plottable data and related dimension scales. Please also refer to [Multi-Channel Data](#multi-channel-data) for further details.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXdata"`](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata) |
| [@axes](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axes-attribute) | NX_CHAR[4] | String array that defines the independent data fields used in the default plot for all of the dimensions of the `/entry/data/data` field, `"image_id, channel, ".", "."`. |
| [@image_id_indices](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-indices-attribute) | NX_INT | Indicates the dependency relationship of the `image_id` field with a dimension of the data, in this case `0`.
| [@start_time_indices](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-indices-attribute) | NX_INT | Indicates the dependency relationship of the `start_time` field with a dimension of the data, in this case `0`.
| [@channel_indices](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-indices-attribute) | NX_INT | Indicates the dependency relationship of the `channel` field with a dimension of the data, in this case `1`.
| [@signal](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-signal-attribute) | NX_CHAR | Declares which field is the default to be plotted, `"data"`. |
| [image_id](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-field) | NX_NUMBER[] | Dimension scale defining the axis `image_id` of the data, typically this is `[1, ..., nP]`. |
| [start_time](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-field) | NX_NUMBER[] | Dimension scale defining the axis `start_time` of the data. These are the relative start times for all images with absolute reference `/entry/start_time`. |
| [channel](https://manual.nexusformat.org/classes/base_classes/NXdata.html#nxdata-axisname-field) | NX_CHAR[][^2] | Dimension scale defining the axis `channel` of the data. These are the enabled channels, for example `["threshold_1", "threshold_2", "difference"]`. |
| [data](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-data-data-field) | NX_NUMBER[nP,nC,i,j] | The image data.<br> **Note:** Files in V2 format always have only one `"/entry/data/data"` dataset, even in case `nimages_per_file` is greater than `0`. This is because it is a [Virtual Dataset](https://portal.hdfgroup.org/display/HDF5/Virtual+Dataset++-+VDS). |

## [/entry/instrument](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-group)

Collection of the components of the instrument or beamline.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXinstrument"`](https://manual.nexusformat.org/classes/base_classes/NXinstrument.html#nxinstrument) |
| [name](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-name-field) | NX_CHAR | Name of the instrument. |
| [time_zone](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-time-zone-field) | NX_DATE_TIME | ISO 8601 time_zone offset from UTC. |

## [/entry/instrument/beam](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-beam-group)

Properties of the neutron or X-ray beam.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXbeam"`](https://manual.nexusformat.org/classes/base_classes/NXbeam.html#nxbeam) |
| [incident_energy](https://manual.nexusformat.org/classes/base_classes/NXbeam.html#nxbeam-incident-energy-field) | NX_FLOAT | Energy carried by each particle of the beam on entering the beamline component. |
| incident_energy@units | NX_ENERGY | `"eV"` |
| [incident_wavelength](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-beam-incident-wavelength-field) | NX_FLOAT | Wavelength of the beam in the case of monochromatic beam. |
| incident_wavelength@units | NX_WAVELENGTH | `"angstrom"` |
| [total_flux](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-beam-total-flux-field) | NX_FLOAT | Flux incident on beam plane in photons per second. In other words, this is the flux integrated over the area. In the case of a beam that varies in flux shot-to-shot, this is an array of values, one for each recorded shot.|
| total_flux@units | NX_FREQUENCY | `"Hz"` |

## [/entry/instrument/detector](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-group)

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXdetector"`](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector) |
| [beam_center_x](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-beam-center-x-field) | NX_FLOAT | The x position of the direct beam on the detector. |
| beam_center_x@units | NX_LENGTH | `"pixel"` |
| [beam_center_y](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-beam-center-y-field) | NX_FLOAT | The y position of the direct beam on the detector. |
| beam_center_y@units | NX_LENGTH | `"pixel"` |
| [bit_depth_readout](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-bit-depth-readout-field) | NX_INT | Bit depth of the internal readout. |
| [count_time](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-count-time-field) | NX_FLOAT | Exposure time per image. |
| count_time@units | NX_TIME | `"s"` |
| [countrate_correction_applied](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-countrate-correction-applied-field) | NX_BOOLEAN | Indicates whether count rate correction was applied. |
| [countrate_correction_lookup_table](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-countrate-correction-lookup-table-field) | NX_NUMBER[] | The countrate_correction_lookup_table defines the LUT used for count rate correction. It maps a measured count `c` to its corrected value `countrate_correction_lookup_table[c]`. |
| [depends_on](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-depends-on-field) | NX_CHAR | NeXus path to the detector positioner axis that most directly supports the detector. In the case of a single-module detector, the detector axis chain may start here. |
| [description](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-description-field) | NX_CHAR | Detector type and model information. |
| [detector_readout_time](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-readout-time-field) | NX_FLOAT | Readout dead time between consecutive detector frames. |
| detector_readout_time@units | NX_TIME | `"s"` |
| [distance](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-distance-field) | NX_FLOAT | Distance from the sample to the beam center on the detector. |
| distance@units | NX_LENGTH | `"m"` |
| [flatfield_applied](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-flatfield-applied-field) | NX_BOOLEAN | Indicates whether a flatfield has been applied. |
| [frame_time](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-frame-time-field) | NX_FLOAT | Time interval between the start of image acquisitions. This defines the speed of data collection and is the inverse of the frame rate, the frequency of image acquisition. |
| frame_time@units | NX_TIME | `"s"` |
| [number_of_cycles](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-number-of-cycles-field) | NX_INT | When auto-summation is enabled, each frame is constructed by summing multiple subframes. `number_of_cycles` is the number of summed subframes. If auto-summation is disabled, this field has the value `1`. |
| [pixel_mask_applied](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-pixel-mask-applied-field) | NX_BOOLEAN | Indicates whether a pixel mask has been applied. |
| [saturation_value](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-saturation-value-field) | NX_INT | The value at which the detector goes into saturation. Data above this value is known to be invalid. |
| [sensor_material](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-sensor-material-field) | NX_CHAR | Material used for direct detection of X-rays in the sensor. |
| [sensor_thickness](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-sensor-thickness-field) | NX_FLOAT | Thickness of the sensor material. |
| sensor_thickness@units | NX_LENGTH | `"m"` |
| [serial_number](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-serial-number-field) | NX_CHAR | Serial number of the detector. |
| [start_time](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-start-time-field) | NX_FLOAT[nP] | Relative start time for each frame, with `/entry/start_time` as absolute reference. |
| [virtual_pixel_interpolation_applied](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-virtual-pixel-interpolation-applied-field) | NX_BOOLEAN | Indicates whether virtual pixel interpolation has been applied. |
| [x_pixel_size](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-x-pixel-size-field) | NX_FLOAT | Size of a single pixel along the x-axis of the detector. |
| x_pixel_size@units | NX_LENGTH | `"m"` |
| [y_pixel_size](https://manual.nexusformat.org/classes/base_classes/NXdetector.html#nxdetector-y-pixel-size-field) | NX_FLOAT | Size of a single pixel along the y-axis of the detector. |
| y_pixel_size@units | NX_LENGTH | `"m"` |


## /entry/instrument/detector/CHANNELNAME_channel

For every channel enabled, there will be one _NXdetector_channel_ group containing metadata specific to that channel. For further details see [Multi-Channel Data](#multi-channel-data).

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | `"NXdetector_channel"` |
| flatfield | NX_FLOAT[] | The flatfield applied to the channel data. |
| pixel_mask | NX_UINT[] | The pixel mask applied to the channel data. |
| threshold_energy | NX_FLOAT or NX_FLOAT[2] | The threshold energy for this channel. In case of difference mode, this is an array containing both the upper and lower threshold energies. |
| threshold_energy@units | NX_ENERGY | `"eV"` |

## [/entry/instrument/detector/geometry](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-transformations-group)

This group contains information on the orientation and position of the detector.

Given a pixel with detector coordinates $P^{'}$, lab coordinates $P$ can be obtained by
$$P = R \cdot P^{'} + t,$$
where $R$ is a rotation matrix and $t$ a translation vector.
Detailed information can be found [here](https://media.dectris.com/DectrisGeometryDocumentation.pdf).

Please also refer to the [_NXtransformations_](https://manual.nexusformat.org/classes/base_classes/NXtransformations.html#nxtransformations) documentation for further details on the representation of transformations and the `depens_on`-chain.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXtransformations"`](https://manual.nexusformat.org/classes/base_classes/NXtransformations.html#nxtransformations) |
| orientation | NX_FLOAT | Rotation angle of the detector. |
| orientation@depends_on | NX_CHAR | `"/entry/instrument/detector/geometry/translation"` |
| orientation@transformation_type | NX_CHAR | `"rotation"` |
| orientation@units | NX_CHAR | `"rad"` |
| orientation@vector | NX_FLOAT[3] | Unit vector describing the axis of the rotation. |
| translation | NX_FLOAT | Distance from the sample to the detector. |
| translation@depends_on | NX_CHAR | The transformation chain begins here, therefore this is `"."`. |
| translation@transformation_type | NX_CHAR | `"translation"` |
| translation@units | NX_CHAR | `"m"` |
| translation@vector | NX_FLOAT[3] | Unit vector describing the direction of the translation from the sample to the detector. |

## [/entry/instrument/detector/module](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-group)

Geometry and logical description of a detector module. In our case, the full data array is represented as one single [_NXdetector_module_](https://manual.nexusformat.org/classes/base_classes/NXdetector_module.html#nxdetector-module).

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXdetector_module"`](https://manual.nexusformat.org/classes/base_classes/NXdetector_module.html#nxdetector-module) |
| [data_origin](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-data-origin-field) | NX_INT[2] | Indices of the data origin. The order of indices is slow to fast. |
| [data_size](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-data-size-field) | NX_INT[2] | Size of one channel image in pixels, the order of indices is slow to fast. |
| [fast_pixel_direction](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-fast-pixel-direction-field) | NX_NUMBER |  Size of a pixel along the fastest varying pixel direction. |
| [fast_pixel_direction@depends_on](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-fast-pixel-direction-depends-on-attribute) | NX_CHAR | `"/entry/instrument/detector/transformations/translation"` |
| [fast_pixel_direction@offset](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-fast-pixel-direction-offset-attribute) | NX_NUMBER[3] | A fixed offset applied before the transformation. |
| [fast_pixel_direction@transformation_type](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-fast-pixel-direction-transformation-type-attribute) | NX_CHAR | `"translation"` |
| fast_pixel_direction@units | NX_LENGTH | `"m"` |
| [fast_pixel_direction@vector](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-fast-pixel-direction-vector-attribute) | NX_NUMBER[3] | Unit vector describing the fastest varying pixel direction. |
| [slow_pixel_direction](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-slow-pixel-direction-field) | NX_NUMBER | Size of a pixel along the slowest varying pixel direction. |
| [slow_pixel_direction@depends_on](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-slow-pixel-direction-depends-on-attribute) | NX_CHAR | `"/entry/instrument/detector/transformations/translation"` |
| [slow_pixel_direction@offset](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-slow-pixel-direction-offset-attribute) | NX_NUMBER[3] | A fixed offset applied before the transformation. |
| [slow_pixel_direction@transformation_type](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-slow-pixel-direction-transformation-type-attribute) | NX_CHAR | `"translation"` |
| slow_pixel_direction@units | NX_LENGTH | `"m"` |
| [slow_pixel_direction@vector](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-detector-module-slow-pixel-direction-vector-attribute) | NX_NUMBER[3] | Unit vector describing the slowest varying pixel direction. |

## [/entry/instrument/detector/transformations](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-instrument-detector-transformations-group)

| Name | NeXus Type | Description |
|---|---|---|
|  |  |
| @NX_class | NX_CHAR | [`"NXtransformations"`](https://manual.nexusformat.org/classes/base_classes/NXtransformations.html#nxtransformations) |
| translation | NX_FLOAT | Distance from the sample to the detector. |
| translation@depends_on | NX_CHAR | `"."` |
| translation@offset | NX_NUMBER[3] | A fixed offset applied before the transformation. |
| translation@transformation_type | NX_CHAR | `"translation"` |
| translation@units | NX_LENGTH | `"m"` |
| translation@vector | NX_NUMBER[3] | Unit vector describing the direction of the translation from the sample to the detector. |
| two_theta | NX_FLOAT[nP] | Starting $2\theta$ value for each frame. |
| two_theta@offset | NX_NUMBER[3] | A fixed offset applied before the transformation. |
| two_theta@transformation_type | NX_CHAR | `"rotation"` |
| two_theta@units | NX_ANGLE | `"degree"` |
| two_theta@vector | NX_NUMBER[3] | Unit vector describing the axis of the rotation. |
| two_theta_end | NX_FLOAT[nP] | $2\theta$ end value for each frame. |
| two_theta_end@units | NX_ANGLE | `"degree"` |
| two_theta_increment_set | NX_FLOAT | The intended average range through which $2\theta$ moves during the exposure of a frame. |
| two_theta_increment_set@units | NX_ANGLE | `"degree"` |



## [/entry/sample](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-sample-group)

Group containing information on the sample.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXsample"`](https://manual.nexusformat.org/classes/base_classes/NXsample.html#nxsample) |
| [depends_on](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-sample-depends-on-field) | NX_CHAR | `"/entry/sample/transformations/omega"` |
| [name](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-sample-name-field) | NX_CHAR | Name of the sample. |


## [/entry/sample/transformations](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-sample-transformations-group)

Group containing sample goniometer and other related axes.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXtransformations"`](https://manual.nexusformat.org/classes/base_classes/NXtransformations.html#nxtransformations) |
| chi | NX_FLOAT[nP] | Starting $\chi$ value for each frame. |
| chi@units | NX_ANGLE | `"degree"` |
| chi_end | NX_FLOAT[nP] | $\chi$ end value for each frame. |
| chi_end@units | NX_ANGLE | `"degree"` |
| chi_increment_set | NX_NUMBER | The intended average range through which $\chi$ moves during the exposure of a frame. |
| chi_increment_set@units | NX_ANGLE | `"degree"` |
| kappa | NX_FLOAT[nP] | Starting $\kappa$ value for each frame. |
| kappa@units | NX_ANGLE | `"degree"` |
| kappa_end | NX_FLOAT[nP] | $\kappa$ end value for each frame. |
| kappa_end@units | NX_ANGLE | `"degree"` |
| kappa_increment_set | NX_NUMBER | The intended average range through which $\kappa$ moves during the exposure of a frame. |
| kappa_increment_set@units | NX_ANGLE | `"degree"` |
| omega | NX_FLOAT[nP] | Starting $\omega$ value for each frame. |
| omega@depends_on | NX_CHAR | `"."` |
| omega@offset | NX_NUMBER[3] | A fixed offset applied before the transformation. |
| omega@transformation_type | NX_CHAR | `"rotation"` |
| omega@units | NX_ANGLE | `"degree"` |
| omega@vector | NX_NUMBER[3] | Unit vector describing the axis of the rotation. |
| omega_end | NX_FLOAT[nP] | $\omega$ end value for each frame. |
| omega_end@units | NX_ANGLE | `"degree"` |
| omega_increment_set | NX_FLOAT | The intended average range through which $\omega$ moves during the exposure of a frame. |
| omega_increment_set@units | NX_ANGLE | `"degree"` |
| phi | NX_FLOAT[nP] | Starting $\phi$ value for each frame. |
| phi@units | NX_ANGLE | `"degree"` |
| phi_end | NX_FLOAT[nP] | $\phi$ end value for each frame. |
| phi_end@units | NX_ANGLE | `"degree"` |
| phi_increment_set | NX_NUMBER | The intended average range through which $\phi$ moves during the exposure of a frame. |
| phi_increment_set@units | NX_ANGLE | `"degree"` |


## [/entry/source](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-source-group)

Group containing information about the neutron or x-ray storage ring/facility.

| Name | NeXus Type | Description |
|---|---|---|
| @NX_class | NX_CHAR | [`"NXsource"`](https://manual.nexusformat.org/classes/base_classes/NXsource.html#nxsource) |
| [name](https://manual.nexusformat.org/classes/applications/NXmx.html#nxmx-entry-source-name-field) | NX_CHAR | Name of the source. |
