#define __STDC_WANT_IEC_60559_TYPES_EXT__
#include "stream2.h"

#include <assert.h>
#include <float.h>
#if FLT16_MANT_DIG > 0 || __FLT16_MANT_DIG__ > 0
#else
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tinycbor/src/cbor.h"

enum { MAX_KEY_LEN = 64 };

static const CborTag TYPED_ARRAY_UINT32_LITTLE_ENDIAN = 70;
static const CborTag TYPED_ARRAY_FLOAT32_LITTLE_ENDIAN = 85;

static enum stream2_result CBOR_RESULT(CborError e) {
    switch (e) {
        case CborNoError:
            return STREAM2_OK;
        case CborErrorUnknownLength:
        case CborErrorDataTooLarge:
            return STREAM2_ERROR_PARSE;
        case CborErrorOutOfMemory:
            return STREAM2_ERROR_OUT_OF_MEMORY;
        default:
            return STREAM2_ERROR_DECODE;
    }
}

static enum stream2_result consume_byte_string_nocopy(const CborValue* it,
                                                      uint8_t** string,
                                                      size_t* len,
                                                      CborValue* next) {
    enum stream2_result r;

    assert(cbor_value_is_byte_string(it));

    if ((r = CBOR_RESULT(cbor_value_get_string_length(it, len))))
        return r;

    const uint8_t* ptr = cbor_value_get_next_byte(it);
    assert(*ptr >= 0x40 && *ptr <= 0x5b);
    switch (*ptr++) {
        case 0x58:
            ptr += 1;
            break;
        case 0x59:
            ptr += 2;
            break;
        case 0x5a:
            ptr += 4;
            break;
        case 0x5b:
            ptr += 8;
            break;
    }
    *string = (uint8_t*)ptr;

    if (next) {
        *next = *it;
        if ((r = CBOR_RESULT(cbor_value_advance(next))))
            return r;
    }
    return STREAM2_OK;
}

static enum stream2_result parse_tag(CborValue* it, CborTag* value) {
    enum stream2_result r;

    if (!cbor_value_is_tag(it))
        return STREAM2_ERROR_PARSE;

    if ((r = CBOR_RESULT(cbor_value_get_tag(it, value))))
        return r;

    return CBOR_RESULT(cbor_value_advance_fixed(it));
}

static enum stream2_result parse_bool(CborValue* it, bool* value) {
    enum stream2_result r;

    if (!cbor_value_is_boolean(it))
        return STREAM2_ERROR_PARSE;

    if ((r = CBOR_RESULT(cbor_value_get_boolean(it, value))))
        return r;

    return CBOR_RESULT(cbor_value_advance_fixed(it));
}

static float half_to_float(uint16_t x) {
#if FLT16_MANT_DIG > 0 || __FLT16_MANT_DIG__ > 0
    _Float16 f;
    memcpy(&f, &x, 2);
    return (float)f;
#else
    return _mm_cvtss_f32(_mm_cvtph_ps(_mm_cvtsi32_si128(x)));
#endif
}

static enum stream2_result parse_double(CborValue* it, double* value) {
    enum stream2_result r;

    if (cbor_value_is_half_float(it)) {
        uint16_t h;
        if ((r = CBOR_RESULT(cbor_value_get_half_float(it, &h))))
            return r;
        *value = (double)half_to_float(h);
    } else if (cbor_value_is_float(it)) {
        float f;
        if ((r = CBOR_RESULT(cbor_value_get_float(it, &f))))
            return r;
        *value = (double)f;
    } else if (cbor_value_is_double(it)) {
        if ((r = CBOR_RESULT(cbor_value_get_double(it, value))))
            return r;
    } else {
        return STREAM2_ERROR_PARSE;
    }

    return CBOR_RESULT(cbor_value_advance_fixed(it));
}

static enum stream2_result parse_uint64(CborValue* it, uint64_t* value) {
    enum stream2_result r;

    if (!cbor_value_is_unsigned_integer(it))
        return STREAM2_ERROR_PARSE;

    if ((r = CBOR_RESULT(cbor_value_get_uint64(it, value))))
        return r;

    return CBOR_RESULT(cbor_value_advance_fixed(it));
}

static enum stream2_result parse_text_string(CborValue* it, char** string) {
    if (!cbor_value_is_text_string(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    return CBOR_RESULT(cbor_value_dup_text_string(it, string, &len, it));
}

static enum stream2_result parse_array_uint64(
        CborValue* it,
        struct stream2_array_uint64* array) {
    enum stream2_result r;

    if (!cbor_value_is_array(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    if ((r = CBOR_RESULT(cbor_value_get_array_length(it, &len))))
        return r;

    array->ptr = calloc(len, sizeof(uint64_t));
    if (array->ptr == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;
    array->len = len;

    CborValue elt;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &elt))))
        return r;

    for (size_t i = 0; i < len; i++) {
        if ((r = parse_uint64(&elt, &array->ptr[i])))
            return r;
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &elt));
}

static enum stream2_result parse_array_2_uint64(CborValue* it,
                                                uint64_t array[2]) {
    enum stream2_result r;

    if (!cbor_value_is_array(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    if ((r = CBOR_RESULT(cbor_value_get_array_length(it, &len))))
        return r;

    if (len != 2)
        return STREAM2_ERROR_PARSE;

    CborValue elt;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &elt))))
        return r;

    for (size_t i = 0; i < len; i++) {
        if ((r = parse_uint64(&elt, &array[i])))
            return r;
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &elt));
}

static enum stream2_result parse_key(CborValue* it, char key[MAX_KEY_LEN]) {
    if (!cbor_value_is_text_string(it))
        return STREAM2_ERROR_PARSE;

    size_t key_len = MAX_KEY_LEN;
    CborError e = cbor_value_copy_text_string(it, key, &key_len, it);
    if (e == CborErrorOutOfMemory || key_len == MAX_KEY_LEN) {
        // The key is longer than any we suppport. Return the empty key which
        // should be handled by the caller like other unknown keys.
        key[0] = '\0';
        return STREAM2_OK;
    }
    return CBOR_RESULT(e);
}

// Fill multidim_array, fields dimensions and array. cbor tag 0x40.
// https://tools.ietf.org/html/rfc8746 section 3.1.1
// Note: this expects only a 2d array.
//
// If nocopy is true, the array data will point into the buffer, without
// any alignment. Therefore it is only used for the image data.
//
// data_tag returns the tag for the byte stream. If not set, the invalid tag
// with value UINT64_MAX is returned.
static enum stream2_result parse_multidim_array(CborValue* it,
                                                struct multidim_array* array,
                                                bool nocopy,
                                                CborTag* data_tag) {
    enum stream2_result r;

    if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
        return r;

    if (!cbor_value_is_array(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    if ((r = CBOR_RESULT(cbor_value_get_array_length(it, &len))))
        return r;

    if (len != 2)
        return STREAM2_ERROR_PARSE;

    CborValue elt;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &elt))))
        return r;

    if ((r = parse_array_2_uint64(&elt, array->dimensions)))
        return r;

    if (cbor_value_is_tag(&elt)) {
        if ((r = parse_tag(&elt, data_tag)))
            return r;
    } else {
        *data_tag = UINT64_MAX;
    }

    if (!cbor_value_is_byte_string(&elt))
        return STREAM2_ERROR_PARSE;

    if (nocopy) {
        if ((r = consume_byte_string_nocopy(&elt, &array->data,
                                            &array->data_len, &elt)))
            return r;
    } else {
        if ((r = CBOR_RESULT(cbor_value_dup_byte_string(
                     &elt, &array->data, &array->data_len, &elt))))
            return r;
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &elt));
}

static enum stream2_result parse_goniometer_axis(
        CborValue* it,
        struct stream2_goniometer_axis* axis) {
    enum stream2_result r;

    if (!cbor_value_is_map(it))
        return STREAM2_ERROR_PARSE;

    CborValue field;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
        return r;

    while (cbor_value_is_valid(&field)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(&field, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(&field))))
            return r;

        if (strcmp(key, "increment") == 0) {
            if ((r = parse_double(&field, &axis->increment)))
                return r;
        } else if (strcmp(key, "start") == 0) {
            if ((r = parse_double(&field, &axis->start)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(&field))))
                return r;
        }
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &field));
}

static enum stream2_result parse_goniometer(
        CborValue* it,
        struct stream2_goniometer* goniometer) {
    enum stream2_result r;

    if (!cbor_value_is_map(it))
        return STREAM2_ERROR_PARSE;

    CborValue field;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
        return r;

    while (cbor_value_is_valid(&field)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(&field, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(&field))))
            return r;

        if (strcmp(key, "chi") == 0) {
            if ((r = parse_goniometer_axis(&field, &goniometer->chi)))
                return r;
        } else if (strcmp(key, "kappa") == 0) {
            if ((r = parse_goniometer_axis(&field, &goniometer->kappa)))
                return r;
        } else if (strcmp(key, "omega") == 0) {
            if ((r = parse_goniometer_axis(&field, &goniometer->omega)))
                return r;
        } else if (strcmp(key, "phi") == 0) {
            if ((r = parse_goniometer_axis(&field, &goniometer->phi)))
                return r;
        } else if (strcmp(key, "two_theta") == 0) {
            if ((r = parse_goniometer_axis(&field, &goniometer->two_theta)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(&field))))
                return r;
        }
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &field));
}

static enum stream2_result parse_start_channel(
        CborValue* it,
        struct stream2_start_channel* channel) {
    enum stream2_result r;

    if (!cbor_value_is_map(it))
        return STREAM2_ERROR_PARSE;

    CborValue field;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
        return r;

    while (cbor_value_is_valid(&field)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(&field, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(&field))))
            return r;

        if (strcmp(key, "data_type") == 0) {
            if ((r = parse_text_string(&field, &channel->data_type)))
                return r;
        } else if (strcmp(key, "thresholds") == 0) {
            if ((r = parse_array_uint64(&field, &channel->thresholds)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(&field))))
                return r;
        }
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &field));
}

static enum stream2_result parse_start_channels(
        CborValue* it,
        struct stream2_start_channels* channels) {
    enum stream2_result r;

    if (!cbor_value_is_array(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    if ((r = CBOR_RESULT(cbor_value_get_array_length(it, &len))))
        return r;

    channels->ptr = calloc(len, sizeof(struct stream2_start_channel));
    if (channels->ptr == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;
    channels->len = len;

    CborValue elt;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &elt))))
        return r;

    for (size_t i = 0; i < len; i++) {
        if ((r = parse_start_channel(&elt, &channels->ptr[i])))
            return r;
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &elt));
}

static enum stream2_result parse_start_event(CborValue* it,
                                             struct stream2_event** e) {
    enum stream2_result r;

    struct stream2_start_event* event =
            calloc(1, sizeof(struct stream2_start_event));
    if (event == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;

    *e = (struct stream2_event*)event;
    event->type = STREAM2_EVENT_START;

    while (cbor_value_is_valid(it)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(it, key)))
            return r;

        // skip any tag for a value, except where verified
        if (strcmp(key, "countrate_correction") != 0) {
            if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
                return r;
        }

        if (strcmp(key, "series_number") == 0) {
            if ((r = parse_uint64(it, &event->series_number)))
                return r;
        } else if (strcmp(key, "series_unique_id") == 0) {
            if ((r = parse_text_string(it, &event->series_unique_id)))
                return r;
        } else if (strcmp(key, "beam_center_x") == 0) {
            if ((r = parse_double(it, &event->beam_center_x)))
                return r;
        } else if (strcmp(key, "beam_center_y") == 0) {
            if ((r = parse_double(it, &event->beam_center_y)))
                return r;
        } else if (strcmp(key, "channels") == 0) {
            if ((r = parse_start_channels(it, &event->channels)))
                return r;
        } else if (strcmp(key, "count_time") == 0) {
            if ((r = parse_double(it, &event->count_time)))
                return r;
        } else if (strcmp(key, "countrate_correction") == 0) {
            CborTag tag;
            if ((r = parse_tag(it, &tag)))
                return r;

            if (tag != TYPED_ARRAY_UINT32_LITTLE_ENDIAN)
                return STREAM2_ERROR_PARSE;

            if (!cbor_value_is_byte_string(it))
                return STREAM2_ERROR_PARSE;

            uint8_t* ptr;
            size_t len;
            if ((r = CBOR_RESULT(
                         cbor_value_dup_byte_string(it, &ptr, &len, it))))
                return r;
            event->countrate_correction.ptr = (uint32_t*)ptr;
            event->countrate_correction.len = len / sizeof(uint32_t);
        } else if (strcmp(key, "countrate_correction_applied") == 0) {
            if ((r = parse_bool(it, &event->countrate_correction_applied)))
                return r;
        } else if (strcmp(key, "detector_description") == 0) {
            if ((r = parse_text_string(it, &event->detector_decription)))
                return r;
        } else if (strcmp(key, "detector_distance") == 0) {
            if ((r = parse_double(it, &event->detector_distance)))
                return r;
        } else if (strcmp(key, "detector_serial_number") == 0) {
            if ((r = parse_text_string(it, &event->detector_serial_number)))
                return r;
        } else if (strcmp(key, "flatfield") == 0) {
            if (!cbor_value_is_map(it))
                return STREAM2_ERROR_PARSE;

            size_t len;
            if ((r = CBOR_RESULT(cbor_value_get_map_length(it, &len))))
                return r;

            event->flatfield.ptr =
                    calloc(len, sizeof(struct stream2_flatfield));
            if (event->flatfield.ptr == NULL)
                return STREAM2_ERROR_OUT_OF_MEMORY;
            event->flatfield.len = len;

            CborValue field;
            if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
                return r;

            for (size_t i = 0; i < len; i++) {
                if ((r = parse_uint64(&field,
                                      &event->flatfield.ptr[i].threshold)))
                    return r;
                CborTag data_tag;
                if ((r = parse_multidim_array(
                             &field,
                             (struct multidim_array*)&event->flatfield.ptr[i]
                                     .array,
                             false, &data_tag)))
                    return r;
                if (data_tag != TYPED_ARRAY_FLOAT32_LITTLE_ENDIAN)
                    return STREAM2_ERROR_PARSE;
            }

            if ((r = CBOR_RESULT(cbor_value_leave_container(it, &field))))
                return r;
        } else if (strcmp(key, "flatfield_applied") == 0) {
            if ((r = parse_bool(it, &event->flatfield_applied)))
                return r;
        } else if (strcmp(key, "frame_time") == 0) {
            if ((r = parse_double(it, &event->frame_time)))
                return r;
        } else if (strcmp(key, "goniometer") == 0) {
            if ((r = parse_goniometer(it, &event->goniometer)))
                return r;
        } else if (strcmp(key, "image_size") == 0) {
            if ((r = parse_array_2_uint64(it, event->image_size)))
                return r;
        } else if (strcmp(key, "images_per_trigger") == 0) {
            if ((r = parse_uint64(it, &event->images_per_trigger)))
                return r;
        } else if (strcmp(key, "incident_energy") == 0) {
            if ((r = parse_double(it, &event->incident_energy)))
                return r;
        } else if (strcmp(key, "incident_wavelength") == 0) {
            if ((r = parse_double(it, &event->incident_wavelength)))
                return r;
        } else if (strcmp(key, "number_of_images") == 0) {
            if ((r = parse_uint64(it, &event->number_of_images)))
                return r;
        } else if (strcmp(key, "number_of_triggers") == 0) {
            if ((r = parse_uint64(it, &event->number_of_triggers)))
                return r;
        } else if (strcmp(key, "pixel_mask") == 0) {
            if (!cbor_value_is_map(it))
                return STREAM2_ERROR_PARSE;

            size_t len;
            if ((r = CBOR_RESULT(cbor_value_get_map_length(it, &len))))
                return r;

            event->pixel_mask.ptr =
                    calloc(len, sizeof(struct stream2_pixel_mask));
            if (event->pixel_mask.ptr == NULL)
                return STREAM2_ERROR_OUT_OF_MEMORY;
            event->pixel_mask.len = len;

            CborValue field;
            if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
                return r;

            for (size_t i = 0; i < len; i++) {
                if ((r = parse_uint64(&field,
                                      &event->pixel_mask.ptr[i].threshold)))
                    return r;
                CborTag data_tag;
                if ((r = parse_multidim_array(
                             &field,
                             (struct multidim_array*)&event->pixel_mask.ptr[i]
                                     .array,
                             false, &data_tag)))
                    return r;
                if (data_tag != TYPED_ARRAY_UINT32_LITTLE_ENDIAN)
                    return STREAM2_ERROR_PARSE;
            }

            if ((r = CBOR_RESULT(cbor_value_leave_container(it, &field))))
                return r;
        } else if (strcmp(key, "pixel_mask_applied") == 0) {
            if ((r = parse_bool(it, &event->pixel_mask_applied)))
                return r;
        } else if (strcmp(key, "pixel_size_x") == 0) {
            if ((r = parse_double(it, &event->pixel_size_x)))
                return r;
        } else if (strcmp(key, "pixel_size_y") == 0) {
            if ((r = parse_double(it, &event->pixel_size_y)))
                return r;
        } else if (strcmp(key, "roi_mode") == 0) {
            if ((r = parse_text_string(it, &event->roi_mode)))
                return r;
        } else if (strcmp(key, "saturation_value") == 0) {
            if ((r = parse_uint64(it, &event->saturation_value)))
                return r;
        } else if (strcmp(key, "sensor_material") == 0) {
            if ((r = parse_text_string(it, &event->sensor_material)))
                return r;
        } else if (strcmp(key, "sensor_thickness") == 0) {
            if ((r = parse_double(it, &event->sensor_thickness)))
                return r;
        } else if (strcmp(key, "series_date") == 0) {
            if ((r = parse_text_string(it, &event->series_date)))
                return r;
        } else if (strcmp(key, "threshold_energy") == 0) {
            if (!cbor_value_is_map(it))
                return STREAM2_ERROR_PARSE;

            size_t len;
            if ((r = CBOR_RESULT(cbor_value_get_map_length(it, &len))))
                return r;

            event->threshold_energy.ptr =
                    calloc(len, sizeof(struct stream2_threshold_energy));
            if (event->threshold_energy.ptr == NULL)
                return STREAM2_ERROR_OUT_OF_MEMORY;
            event->threshold_energy.len = len;

            CborValue field;
            if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
                return r;

            for (size_t i = 0; i < len; i++) {
                if ((r = parse_uint64(
                             &field,
                             &event->threshold_energy.ptr[i].threshold)))
                    return r;
                if ((r = parse_double(&field,
                                      &event->threshold_energy.ptr[i].energy)))
                    return r;
            }

            if ((r = CBOR_RESULT(cbor_value_leave_container(it, &field))))
                return r;
        } else if (strcmp(key, "virtual_pixel_correction_applied") == 0) {
            if ((r = parse_bool(it, &event->virtual_pixel_correction_applied)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(it))))
                return r;
        }
    }
    return STREAM2_OK;
}

static enum stream2_result parse_image_channel(
        CborValue* it,
        struct stream2_image_channel* channel) {
    enum stream2_result r;

    if (!cbor_value_is_map(it))
        return STREAM2_ERROR_PARSE;

    CborValue field;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &field))))
        return r;

    while (!cbor_value_at_end(&field)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(&field, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
            return r;

        if (strcmp(key, "compression") == 0) {
            if ((r = parse_text_string(&field, &channel->compression)))
                return r;
        } else if (strcmp(key, "data_type") == 0) {
            if ((r = parse_text_string(&field, &channel->data_type)))
                return r;
        } else if (strcmp(key, "lost_pixel_count") == 0) {
            if ((r = parse_uint64(&field, &channel->lost_pixel_count)))
                return r;
        } else if (strcmp(key, "thresholds") == 0) {
            if ((r = parse_array_uint64(&field, &channel->thresholds)))
                return r;
        } else if (strcmp(key, "data") == 0) {
            CborTag data_tag;
            if ((r = parse_multidim_array(&field, &channel->array, true,
                                          &data_tag)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(it))))
                return r;
        }
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &field));
}

static enum stream2_result parse_image_channels(
        CborValue* it,
        struct stream2_image_channels* channels) {
    enum stream2_result r;

    if (!cbor_value_is_array(it))
        return STREAM2_ERROR_PARSE;

    size_t len;
    if ((r = CBOR_RESULT(cbor_value_get_array_length(it, &len))))
        return r;

    channels->ptr = calloc(len, sizeof(struct stream2_image_channel));
    if (channels->ptr == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;
    channels->len = len;

    CborValue elt;
    if ((r = CBOR_RESULT(cbor_value_enter_container(it, &elt))))
        return r;

    for (size_t i = 0; i < len; i++) {
        if ((r = parse_image_channel(&elt, &channels->ptr[i])))
            return r;
    }

    return CBOR_RESULT(cbor_value_leave_container(it, &elt));
}

static enum stream2_result parse_image_event(CborValue* it,
                                             struct stream2_event** e) {
    enum stream2_result r;

    struct stream2_image_event* event =
            calloc(1, sizeof(struct stream2_image_event));
    if (event == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;

    *e = (struct stream2_event*)event;
    event->type = STREAM2_EVENT_IMAGE;

    while (cbor_value_is_valid(it)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(it, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
            return r;

        if (strcmp(key, "series_number") == 0) {
            if ((r = parse_uint64(it, &event->series_number)))
                return r;
        } else if (strcmp(key, "series_unique_id") == 0) {
            if ((r = parse_text_string(it, &event->series_unique_id)))
                return r;
        } else if (strcmp(key, "image_number") == 0) {
            if ((r = parse_uint64(it, &event->image_number)))
                return r;
        } else if (strcmp(key, "hardware_start_time") == 0) {
            if ((r = parse_array_2_uint64(it, event->hardware_start_time)))
                return r;
        } else if (strcmp(key, "hardware_stop_time") == 0) {
            if ((r = parse_array_2_uint64(it, event->hardware_stop_time)))
                return r;
        } else if (strcmp(key, "hardware_exposure_time") == 0) {
            if ((r = parse_array_2_uint64(it, event->hardware_exposure_time)))
                return r;
        } else if (strcmp(key, "channels") == 0) {
            if ((r = parse_image_channels(it, &event->channels)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(it))))
                return r;
        }
    }
    return STREAM2_OK;
}

static enum stream2_result parse_end_event(CborValue* it,
                                           struct stream2_event** e) {
    enum stream2_result r;

    struct stream2_end_event* event =
            calloc(1, sizeof(struct stream2_end_event));
    if (event == NULL)
        return STREAM2_ERROR_OUT_OF_MEMORY;

    *e = (struct stream2_event*)event;
    event->type = STREAM2_EVENT_END;

    while (cbor_value_is_valid(it)) {
        char key[MAX_KEY_LEN];
        if ((r = parse_key(it, key)))
            return r;

        if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
            return r;

        if (strcmp(key, "series_number") == 0) {
            if ((r = parse_uint64(it, &event->series_number)))
                return r;
        } else if (strcmp(key, "series_unique_id") == 0) {
            if ((r = parse_text_string(it, &event->series_unique_id)))
                return r;
        } else {
            if ((r = CBOR_RESULT(cbor_value_advance(it))))
                return r;
        }
    }
    return STREAM2_OK;
}

static enum stream2_result parse_event_type(CborValue* it,
                                            char type[MAX_KEY_LEN]) {
    enum stream2_result r;

    char key[MAX_KEY_LEN];
    if ((r = parse_key(it, key)))
        return r;

    if (strcmp(key, "type") != 0)
        return STREAM2_ERROR_PARSE;

    if ((r = CBOR_RESULT(cbor_value_skip_tag(it))))
        return r;

    return parse_key(it, type);
}

static enum stream2_result parse_event(const uint8_t* message,
                                       size_t size,
                                       struct stream2_event** event) {
    enum stream2_result r;

    // https://www.rfc-editor.org/rfc/rfc8949.html#name-self-described-cbor
    const uint8_t MAGIC[3] = {0xd9, 0xd9, 0xf7};
    if (size < sizeof(MAGIC) || memcmp(message, MAGIC, sizeof(MAGIC)) != 0)
        return STREAM2_ERROR_SIGNATURE;

    message += sizeof(MAGIC);
    size -= sizeof(MAGIC);

    CborParser parser;
    CborValue it;
    if ((r = CBOR_RESULT(cbor_parser_init(message, size, 0, &parser, &it))))
        return r;

    if (!cbor_value_is_map(&it))
        return STREAM2_ERROR_PARSE;

    CborValue field;
    if ((r = CBOR_RESULT(cbor_value_enter_container(&it, &field))))
        return r;

    char type[MAX_KEY_LEN];
    if ((r = parse_event_type(&field, type)))
        return r;

    if (strcmp(type, "start") == 0) {
        if ((r = parse_start_event(&field, event)))
            return r;
    } else if (strcmp(type, "image") == 0) {
        if ((r = parse_image_event(&field, event)))
            return r;
    } else if (strcmp(type, "end") == 0) {
        if ((r = parse_end_event(&field, event)))
            return r;
    } else {
        return STREAM2_ERROR_PARSE;
    }

    return CBOR_RESULT(cbor_value_leave_container(&it, &field));
}

enum stream2_result stream2_parse_event(const uint8_t* message,
                                        size_t size,
                                        struct stream2_event** event) {
    enum stream2_result r;

    if ((r = parse_event(message, size, event))) {
        if (*event) {
            stream2_free_event(*event);
            *event = NULL;
        }
        return r;
    }
    return STREAM2_OK;
}

static void free_start_event(struct stream2_start_event* event) {
    for (size_t i = 0; i < event->channels.len; i++) {
        free(event->channels.ptr[i].data_type);
        free(event->channels.ptr[i].thresholds.ptr);
    }
    free(event->channels.ptr);
    free(event->countrate_correction.ptr);
    free(event->detector_decription);
    free(event->detector_serial_number);
    for (size_t i = 0; i < event->flatfield.len; i++)
        free(event->flatfield.ptr[i].array.data);
    free(event->flatfield.ptr);
    for (size_t i = 0; i < event->pixel_mask.len; i++)
        free(event->pixel_mask.ptr[i].array.data);
    free(event->pixel_mask.ptr);
    free(event->roi_mode);
    free(event->sensor_material);
    free(event->series_date);
    free(event->threshold_energy.ptr);
}

static void free_image_event(struct stream2_image_event* event) {
    for (size_t i = 0; i < event->channels.len; i++) {
        free(event->channels.ptr[i].compression);
        free(event->channels.ptr[i].data_type);
        free(event->channels.ptr[i].thresholds.ptr);
    }
    free(event->channels.ptr);
}

void stream2_free_event(struct stream2_event* event) {
    switch (event->type) {
        case STREAM2_EVENT_START:
            free_start_event((struct stream2_start_event*)event);
            break;
        case STREAM2_EVENT_IMAGE:
            free_image_event((struct stream2_image_event*)event);
            break;
        case STREAM2_EVENT_END:
            break;
    }
    free(event->series_unique_id);
    free(event);
}
