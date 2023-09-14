#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#include "compression/src/compression.h"
#include "stream2.h"
#include "tinycbor/src/cbor.h"

static enum stream2_result decode_bytes(const struct stream2_bytes* bytes,
                                        const unsigned char** decoded,
                                        size_t* decoded_len,
                                        void** decompress_buffer) {
    const struct stream2_compression compression = bytes->compression;

    if (compression.algorithm == NULL) {
        *decoded = (const unsigned char*)bytes->ptr;
        *decoded_len = bytes->len;
        *decompress_buffer = NULL;
        return STREAM2_OK;
    }

    CompressionAlgorithm algorithm;
    if (strcmp(compression.algorithm, "bslz4") == 0) {
        algorithm = COMPRESSION_BSLZ4;
    } else if (strcmp(compression.algorithm, "lz4") == 0) {
        algorithm = COMPRESSION_LZ4;
    } else {
        return STREAM2_ERROR_NOT_IMPLEMENTED;
    }

    const size_t len = compression_decompress_buffer(
            algorithm, NULL, 0, (const char*)bytes->ptr, bytes->len,
            compression.elem_size);
    if (len == COMPRESSION_ERROR)
        return STREAM2_ERROR_DECODE;

    void* buffer = malloc(len);
    if (!buffer)
        return STREAM2_ERROR_OUT_OF_MEMORY;

    if (compression_decompress_buffer(algorithm, (char*)buffer, len,
                                      (const char*)bytes->ptr, bytes->len,
                                      compression.elem_size) != len)
    {
        free(buffer);
        return STREAM2_ERROR_DECODE;
    }

    *decoded = (const unsigned char*)buffer;
    *decoded_len = len;
    *decompress_buffer = buffer;
    return STREAM2_OK;
}

static enum stream2_result decode_typed_array(
        const struct stream2_typed_array* array,
        const unsigned char** data,
        size_t* len,
        size_t* elem_size,
        void** decompress_buffer) {
    enum stream2_result r;

    uint64_t elem_size64;
    if ((r = stream2_typed_array_elem_size(array, &elem_size64)))
        return r;

    if (elem_size64 > SIZE_MAX)
        return STREAM2_ERROR_NOT_IMPLEMENTED;

    *elem_size = elem_size64;

    if ((r = decode_bytes(&array->data, data, len, decompress_buffer)))
        return r;

    *len /= *elem_size;

    return STREAM2_OK;
}

static int print_typed_array_type(const struct stream2_typed_array* array) {
    switch (array->tag) {
        case STREAM2_TYPED_ARRAY_UINT8:
            return printf("uint8 (tag %" PRIu64 ")", array->tag);
        case STREAM2_TYPED_ARRAY_UINT16_LITTLE_ENDIAN:
            return printf("uint16 (tag %" PRIu64 ")", array->tag);
        case STREAM2_TYPED_ARRAY_UINT32_LITTLE_ENDIAN:
            return printf("uint32 (tag %" PRIu64 ")", array->tag);
        case STREAM2_TYPED_ARRAY_FLOAT32_LITTLE_ENDIAN:
            return printf("float32 (tag %" PRIu64 ")", array->tag);
        default:
            return printf("tag %" PRIu64, array->tag);
    }
}

static void print_multidim_array(
        const struct stream2_multidim_array* multidim) {
    enum stream2_result r;
    const unsigned char* data;
    size_t len;
    size_t elem_size;
    void* buffer;
    if ((r = decode_typed_array(&multidim->array, &data, &len, &elem_size,
                                &buffer)))
    {
        printf("error %i\n", (int)r);
        return;
    }
    printf("dim [%" PRIu64 " %" PRIu64 "] type ", multidim->dim[0],
           multidim->dim[1]);
    print_typed_array_type(&multidim->array);
    printf("\n");

    uint64_t ce = multidim->dim[1];
    int c_width;
    switch (multidim->array.tag) {
        default:
        case STREAM2_TYPED_ARRAY_UINT8:
            ce = multidim->dim[1] * elem_size;
            c_width = 2;
            break;
        case STREAM2_TYPED_ARRAY_UINT16_LITTLE_ENDIAN:
            c_width = 4;
            break;
        case STREAM2_TYPED_ARRAY_UINT32_LITTLE_ENDIAN:
            c_width = 8;
            break;
        case STREAM2_TYPED_ARRAY_FLOAT32_LITTLE_ENDIAN:
            c_width = 5;
            break;
    }
    const int COLS = 80;
    const uint64_t c_mid = ((COLS - 3) / (c_width + 1) & ~1) / 2;
    const uint64_t r_mid = 20 / 2;
    for (uint64_t ri = 0, re = multidim->dim[0]; ri < re; ri++, printf("\n")) {
        if (ri == r_mid && re > r_mid * 2) {
            ri = re - r_mid - 1;
            for (uint64_t ci = 0; ci < ce; ci++) {
                if (ci == c_mid && ce > c_mid * 2) {
                    ci = ce - c_mid - 1;
                    printf(":: ");
                    continue;
                }
                printf(":%*s: ", c_width - 2, "");
            }
            continue;
        }
        for (uint64_t ci = 0; ci < ce; ci++) {
            if (ci == c_mid && ce > c_mid * 2) {
                ci = ce - c_mid - 1;
                printf(".. ");
                continue;
            }
            switch (multidim->array.tag) {
                default:
                case STREAM2_TYPED_ARRAY_UINT8:
                    printf("%02" PRIx8 " ", data[ri * ce + ci]);
                    break;
                case STREAM2_TYPED_ARRAY_UINT16_LITTLE_ENDIAN: {
                    uint16_t v;
                    memcpy(&v, data + (ri * ce + ci) * elem_size, sizeof(v));
                    printf("%04" PRIx16 " ", v);
                    break;
                }
                case STREAM2_TYPED_ARRAY_UINT32_LITTLE_ENDIAN: {
                    uint32_t v;
                    memcpy(&v, data + (ri * ce + ci) * elem_size, sizeof(v));
                    printf("%08" PRIx32 " ", v);
                    break;
                }
                case STREAM2_TYPED_ARRAY_FLOAT32_LITTLE_ENDIAN: {
                    float v;
                    memcpy(&v, data + (ri * ce + ci) * elem_size, sizeof(v));
                    if (v >= 0.f && v < 10.f)
                        printf("%.3f ", v != 0.f ? v : 0.f);
                    else
                        printf("##### ");
                    break;
                }
            }
        }
    }
    free(buffer);
}

static void print_user_data(struct stream2_user_data* user_data) {
    if (user_data->ptr != NULL) {
        CborParser parser;
        CborValue it;
        CborError e;
        if ((e = cbor_parser_init(user_data->ptr, user_data->len, 0, &parser,
                                  &it)) ||
            (e = cbor_value_to_pretty(stdout, &it)))
        {
            printf("error: %s\n", cbor_error_string(e));
            return;
        }
    }
    printf("\n");
}

static void handle_start_msg(struct stream2_start_msg* msg) {
    printf("\nSTART MESSAGE: series_id %" PRIu64 " series_unique_id %s\n",
           msg->series_id, msg->series_unique_id);
    printf("arm_date: %s\n", msg->arm_date ? msg->arm_date : "");
    printf("beam_center_x: %f\n", msg->beam_center_x);
    printf("beam_center_y: %f\n", msg->beam_center_y);
    printf("channels: [");
    for (size_t i = 0; i < msg->channels.len; i++) {
        if (i > 0)
            printf(" ");
        printf("\"%s\"", msg->channels.ptr[i]);
    }
    printf("]\n");
    printf("count_time: %f\n", msg->count_time);
    printf("countrate_correction_enabled: %s\n",
           msg->countrate_correction_enabled ? "true" : "false");
    {
        enum stream2_result r;
        const unsigned char* array;
        size_t len;
        size_t elem_size;
        void* buffer;
        if (msg->countrate_correction_lookup_table.tag == UINT64_MAX) {
            printf("countrate_correction_lookup_table:\n");
        } else if (msg->countrate_correction_lookup_table.tag !=
                   STREAM2_TYPED_ARRAY_UINT32_LITTLE_ENDIAN)
        {
            printf("countrate_correction_lookup_table: error: unexpected tag "
                   "%" PRIu64 "\n",
                   msg->countrate_correction_lookup_table.tag);
        } else if ((r = decode_typed_array(
                            &msg->countrate_correction_lookup_table, &array,
                            &len, &elem_size, &buffer)))
        {
            printf("countrate_correction_lookup_table: error %i\n", (int)r);
        } else {
            uint32_t cutoff = 0;
            if (len >= 2)
                memcpy(&cutoff, array + (len - 2) * elem_size, sizeof(cutoff));
            printf("countrate_correction_lookup_table: %zu entries cutoff "
                   "%" PRIu32 "\n",
                   len, cutoff);
            free(buffer);
        }
    }
    printf("detector_description: \"%s\"\n",
           msg->detector_description ? msg->detector_description : "");
    printf("detector_serial_number: \"%s\"\n",
           msg->detector_serial_number ? msg->detector_serial_number : "");
    printf("detector_translation: [%f %f %f]\n", msg->detector_translation[0],
           msg->detector_translation[1], msg->detector_translation[2]);
    for (size_t i = 0; i < msg->flatfield.len; i++) {
        struct stream2_flatfield* flatfield = &msg->flatfield.ptr[i];
        printf("flatfield: \"%s\" ", flatfield->channel);
        print_multidim_array(&flatfield->flatfield);
    }
    printf("flatfield_enabled: %s\n",
           msg->flatfield_enabled ? "true" : "false");
    printf("frame_time: %f\n", msg->frame_time);
    printf("goniometer: chi: start %f increment %f\n",
           msg->goniometer.chi.start, msg->goniometer.chi.increment);
    printf("goniometer: kappa: start %f increment %f\n",
           msg->goniometer.kappa.start, msg->goniometer.kappa.increment);
    printf("goniometer: omega: start %f increment %f\n",
           msg->goniometer.omega.start, msg->goniometer.omega.increment);
    printf("goniometer: phi: start %f increment %f\n",
           msg->goniometer.phi.start, msg->goniometer.phi.increment);
    printf("goniometer: two_theta: start %f increment %f\n",
           msg->goniometer.two_theta.start,
           msg->goniometer.two_theta.increment);
    printf("image_dtype: \"%s\"\n", msg->image_dtype ? msg->image_dtype : "");
    printf("image_size_x: %" PRIu64 "\n", msg->image_size_x);
    printf("image_size_y: %" PRIu64 "\n", msg->image_size_y);
    printf("incident_energy: %f\n", msg->incident_energy);
    printf("incident_wavelength: %f\n", msg->incident_wavelength);
    printf("number_of_images: %" PRIu64 "\n", msg->number_of_images);
    for (size_t i = 0; i < msg->pixel_mask.len; i++) {
        struct stream2_pixel_mask* pixel_mask = &msg->pixel_mask.ptr[i];
        printf("pixel_mask: \"%s\" ", pixel_mask->channel);
        print_multidim_array(&pixel_mask->pixel_mask);
    }
    printf("pixel_mask_enabled: %s\n",
           msg->pixel_mask_enabled ? "true" : "false");
    printf("pixel_size_x: %f\n", msg->pixel_size_x);
    printf("pixel_size_y: %f\n", msg->pixel_size_y);
    printf("saturation_value: %" PRIu64 "\n", msg->saturation_value);
    printf("sensor_material: \"%s\"\n",
           msg->sensor_material ? msg->sensor_material : "");
    printf("sensor_thickness: %f\n", msg->sensor_thickness);
    for (size_t i = 0; i < msg->threshold_energy.len; i++) {
        printf("threshold_energy: \"%s\" %f\n",
               msg->threshold_energy.ptr[i].channel,
               msg->threshold_energy.ptr[i].energy);
    }
    printf("user_data: ");
    print_user_data(&msg->user_data);
    printf("virtual_pixel_interpolation_enabled: %s\n",
           msg->virtual_pixel_interpolation_enabled ? "true" : "false");
}

static void handle_image_msg(struct stream2_image_msg* msg) {
    printf("\nIMAGE MESSAGE: series_id %" PRIu64 " series_unique_id %s\n",
           msg->series_id, msg->series_unique_id);
    printf("image_id: %" PRIu64 "\n", msg->image_id);
    printf("real_time: %" PRIu64 "/%" PRIu64 "\n", msg->real_time[0],
           msg->real_time[1]);
    printf("series_date: %s\n", msg->series_date ? msg->series_date : "");
    printf("start_time: %" PRIu64 "/%" PRIu64 "\n", msg->start_time[0],
           msg->start_time[1]);
    printf("stop_time: %" PRIu64 "/%" PRIu64 "\n", msg->stop_time[0],
           msg->stop_time[1]);
    printf("user_data: ");
    print_user_data(&msg->user_data);
    for (size_t i = 0; i < msg->data.len; i++) {
        struct stream2_image_data* data = &msg->data.ptr[i];
        printf("data: \"%s\" ", data->channel);
        print_multidim_array(&data->data);
    }
}

static void handle_end_msg(struct stream2_end_msg* msg) {
    printf("\nEND MESSAGE: series_id %" PRIu64 " series_unique_id %s\n",
           msg->series_id, msg->series_unique_id);
}

static void handle_msg(struct stream2_msg* msg) {
    switch (msg->type) {
        case STREAM2_MSG_START:
            handle_start_msg((struct stream2_start_msg*)msg);
            break;
        case STREAM2_MSG_IMAGE:
            handle_image_msg((struct stream2_image_msg*)msg);
            break;
        case STREAM2_MSG_END:
            handle_end_msg((struct stream2_end_msg*)msg);
            break;
    }
}

static enum stream2_result parse_msg(const uint8_t* msg_data, size_t msg_size) {
    enum stream2_result r;

    struct stream2_msg* msg;
    if ((r = stream2_parse_msg(msg_data, msg_size, &msg))) {
        fprintf(stderr, "error: error %i parsing message\n", (int)r);
        return r;
    }

    handle_msg(msg);

    stream2_free_msg(msg);

    return STREAM2_OK;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s HOST\n", argv[0]);
        return EXIT_FAILURE;
    }

    char address[100];
    sprintf(address, "tcp://%s:31001", argv[1]);

    void* ctx = zmq_ctx_new();
    void* socket = zmq_socket(ctx, ZMQ_PULL);

    zmq_connect(socket, address);
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    for (;;) {
        zmq_msg_recv(&msg, socket, 0);

        const uint8_t* msg_data = (const uint8_t*)zmq_msg_data(&msg);
        size_t msg_size = zmq_msg_size(&msg);

        enum stream2_result r;
        if ((r = parse_msg(msg_data, msg_size)))
            break;
    }
    zmq_msg_close(&msg);
    zmq_close(socket);
    zmq_ctx_term(ctx);
    return EXIT_FAILURE;
}
