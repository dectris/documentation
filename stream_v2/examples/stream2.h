#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

enum stream2_result {
    STREAM2_OK = 0,
    STREAM2_ERROR_OUT_OF_MEMORY,
    STREAM2_ERROR_SIGNATURE,
    STREAM2_ERROR_DECODE,
    STREAM2_ERROR_PARSE,
};

struct stream2_array {
    void* ptr;
    size_t len;
};

struct stream2_array_float {
    float* ptr;
    size_t len;
};

struct stream2_array_uint32 {
    uint32_t* ptr;
    size_t len;
};

struct stream2_array_text_string {
    char** ptr;
    size_t len;
};

// https://www.rfc-editor.org/rfc/rfc8746.html#name-row-major-order
struct stream2_multidim_array {
    uint64_t dim[2];
    struct stream2_array array;
};

struct stream2_multidim_array_float {
    uint64_t dim[2];
    struct stream2_array_float array;
};

struct stream2_multidim_array_uint32 {
    uint64_t dim[2];
    struct stream2_array_uint32 array;
};

struct stream2_flatfield {
    char* channel;
    struct stream2_multidim_array_float flatfield;
};

struct stream2_flatfield_map {
    struct stream2_flatfield* ptr;
    size_t len;
};

struct stream2_goniometer_axis {
    double increment;
    double start;
};

struct stream2_goniometer {
    struct stream2_goniometer_axis chi;
    struct stream2_goniometer_axis kappa;
    struct stream2_goniometer_axis omega;
    struct stream2_goniometer_axis phi;
    struct stream2_goniometer_axis two_theta;
};

struct stream2_image_data {
    char* channel;
    struct stream2_multidim_array data;
    uint64_t elem_size;
};

struct stream2_image_data_map {
    struct stream2_image_data* ptr;
    size_t len;
};

struct stream2_pixel_mask {
    char* channel;
    struct stream2_multidim_array_uint32 pixel_mask;
};

struct stream2_pixel_mask_map {
    struct stream2_pixel_mask* ptr;
    size_t len;
};

struct stream2_threshold_energy {
    char* channel;
    double energy;
};

struct stream2_threshold_energy_map {
    struct stream2_threshold_energy* ptr;
    size_t len;
};

enum stream2_msg_type {
    STREAM2_MSG_START,
    STREAM2_MSG_IMAGE,
    STREAM2_MSG_END,
};

struct stream2_msg {
    enum stream2_msg_type type;
    uint64_t series_id;
    char* series_unique_id;
};

struct stream2_start_msg {
    enum stream2_msg_type type;
    uint64_t series_id;
    char* series_unique_id;

    char* arm_date;
    double beam_center_x;
    double beam_center_y;
    struct stream2_array_text_string channels;
    double count_time;
    bool countrate_correction_enabled;
    struct stream2_array_uint32 countrate_correction_lookup_table;
    char* detector_description;
    char* detector_serial_number;
    double detector_translation[3];
    struct stream2_flatfield_map flatfield;
    bool flatfield_enabled;
    double frame_time;
    struct stream2_goniometer goniometer;
    uint64_t image_size_x;
    uint64_t image_size_y;
    double incident_energy;
    double incident_wavelength;
    uint64_t number_of_images;
    struct stream2_pixel_mask_map pixel_mask;
    bool pixel_mask_enabled;
    double pixel_size_x;
    double pixel_size_y;
    uint64_t saturation_value;
    char* sensor_material;
    double sensor_thickness;
    struct stream2_threshold_energy_map threshold_energy;
    bool virtual_pixel_interpolation_enabled;
};

struct stream2_image_msg {
    enum stream2_msg_type type;
    uint64_t series_id;
    char* series_unique_id;

    uint64_t image_id;
    uint64_t real_time[2];
    char* series_date;
    uint64_t start_time[2];
    uint64_t stop_time[2];
    struct stream2_image_data_map data;
};

struct stream2_end_msg {
    enum stream2_msg_type type;
    uint64_t series_id;
    char* series_unique_id;
};

enum stream2_result stream2_parse_msg(const uint8_t* buffer,
                                      const size_t size,
                                      struct stream2_msg** msg_out);
void stream2_free_msg(struct stream2_msg* msg);

#if defined(__cplusplus)
}
#endif
