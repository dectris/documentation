#pragma once

//  This module contains a few functions to show how stream2 cbor messages
//  can be decoded.

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

enum stream2_event_type {
    STREAM2_EVENT_START,
    STREAM2_EVENT_IMAGE,
    STREAM2_EVENT_END,
};

struct stream2_array_uint32 {
    uint32_t* ptr;
    size_t len;
};

struct stream2_array_uint64 {
    uint64_t* ptr;
    size_t len;
};

// https://tools.ietf.org/html/rfc8746#section-3.1
struct multidim_array {
    uint64_t dimensions[2];
    uint8_t* data;
    size_t data_len;
};

struct multidim_array_float {
    uint64_t dimensions[2];
    float* data;
    size_t data_len;
};

struct multidim_array_uint32 {
    uint64_t dimensions[2];
    uint32_t* data;
    size_t data_len;
};

struct stream2_flatfield {
    uint64_t threshold;
    struct multidim_array_float array;
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

struct stream2_image_channel {
    char* compression;
    char* data_type;
    uint64_t lost_pixel_count;
    struct stream2_array_uint64 thresholds;
    struct multidim_array array;
};

struct stream2_image_channels {
    struct stream2_image_channel* ptr;
    size_t len;
};

struct stream2_pixel_mask {
    uint64_t threshold;
    struct multidim_array_uint32 array;
};

struct stream2_pixel_mask_map {
    struct stream2_pixel_mask* ptr;
    size_t len;
};

struct stream2_start_channel {
    char* data_type;
    struct stream2_array_uint64 thresholds;
};

struct stream2_start_channels {
    struct stream2_start_channel* ptr;
    size_t len;
};

struct stream2_threshold_energy {
    uint64_t threshold;
    double energy;
};

struct stream2_threshold_energy_map {
    struct stream2_threshold_energy* ptr;
    size_t len;
};

struct stream2_event {
    enum stream2_event_type type;
    uint64_t series_number;
    char* series_unique_id;
};

struct stream2_start_event {
    enum stream2_event_type type;
    uint64_t series_number;
    char* series_unique_id;

    double beam_center_x;
    double beam_center_y;
    struct stream2_start_channels channels;
    double count_time;
    struct stream2_array_uint32 countrate_correction;
    bool countrate_correction_applied;
    char* detector_decription;
    double detector_distance;
    char* detector_serial_number;
    struct stream2_flatfield_map flatfield;
    bool flatfield_applied;
    double frame_time;
    struct stream2_goniometer goniometer;
    uint64_t image_size[2];
    uint64_t images_per_trigger;
    double incident_energy;
    double incident_wavelength;
    uint64_t number_of_images;
    uint64_t number_of_triggers;
    struct stream2_pixel_mask_map pixel_mask;
    bool pixel_mask_applied;
    double pixel_size_x;
    double pixel_size_y;
    char* roi_mode;
    uint64_t saturation_value;
    char* sensor_material;
    double sensor_thickness;
    char* series_date;
    struct stream2_threshold_energy_map threshold_energy;
    bool virtual_pixel_correction_applied;
};

struct stream2_image_event {
    enum stream2_event_type type;
    uint64_t series_number;
    char* series_unique_id;

    uint64_t image_number;
    uint64_t hardware_start_time[2];
    uint64_t hardware_stop_time[2];
    uint64_t hardware_exposure_time[2];
    struct stream2_image_channels channels;
};

struct stream2_end_event {
    enum stream2_event_type type;
    uint64_t series_number;
    char* series_unique_id;
};

enum stream2_result stream2_parse_event(const uint8_t* message,
                                        const size_t size,
                                        struct stream2_event** event);
void stream2_free_event(struct stream2_event*);

#if defined(__cplusplus)
}
#endif
