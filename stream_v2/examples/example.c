#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#include "compression/src/compression.h"
#include "stream2.h"

#define IMAGE_BUFFER_SIZE (32 * 1024 * 1024 * sizeof(uint32_t))

static void print_array_uint64(struct stream2_array_uint64* array) {
    for (size_t i = 0; i < array->len; i++) {
        if (i > 0)
            printf(" ");
        printf("%" PRIu64, array->ptr[i]);
    }
}

static void print_data(const uint64_t dimensions[2],
                       const uint8_t* data,
                       size_t elem_size) {
    for (uint64_t ri = 0, re = dimensions[0]; ri < re; ri++) {
        if (ri == 12 && re > 25) {
            ri = re - 12 - 1;
            for (uint64_t ci = 0, ce = dimensions[1] * elem_size; ci < ce; ci++)
            {
                if (ci >= 25)
                    break;
                printf(":: ");
            }
        } else {
            for (uint64_t ci = 0, ce = dimensions[1] * elem_size; ci < ce; ci++)
            {
                if (ci == 12 && ce > 25) {
                    ci = ce - 12 - 1;
                    printf(".. ");
                    continue;
                }
                printf("%02" PRIx8 " ", data[ri * ce + ci]);
            }
        }
        printf("\n");
    }
}

static void handle_start_event(struct stream2_start_event* event) {
    printf("\nSTART EVENT: series_number=%" PRIu64 " series_unique_id=%s\n",
           event->series_number, event->series_unique_id);
    printf("beam_center_x: %f\n", event->beam_center_x);
    printf("beam_center_y: %f\n", event->beam_center_y);
    for (size_t i = 0; i < event->channels.len; i++) {
        struct stream2_start_channel* channel = &event->channels.ptr[i];
        printf("channel %zu: data_type=%s thresholds=[", i, channel->data_type);
        print_array_uint64(&channel->thresholds);
        printf("]\n");
    }
    printf("count_time: %f\n", event->count_time);
    if (event->countrate_correction.ptr) {
        printf("countrate_correction: %zu entries cutoff %" PRIu32 "\n",
               event->countrate_correction.len,
               event->countrate_correction.len >= 2
                       ? event->countrate_correction
                                 .ptr[event->countrate_correction.len - 2]
                       : 0);
    }
    printf("countrate_correction_applied: %s\n",
           event->countrate_correction_applied ? "true" : "false");
    printf("detector_decription: %s\n", event->detector_decription);
    printf("detector_distance: %f\n", event->detector_distance);
    printf("detector_serial_number: %s\n", event->detector_serial_number);
    for (size_t i = 0; i < event->flatfield.len; i++) {
        struct stream2_flatfield* flatfield = &event->flatfield.ptr[i];
        printf("flatfield: threshold %" PRIu64 ": [", flatfield->threshold);
        for (size_t j = 0; j < flatfield->array.data_len / sizeof(float); j++) {
            if (j >= 6) {
                printf(" ...");
                break;
            }
            if (j > 0)
                printf(" ");
            printf("%f", flatfield->array.data[j]);
        }
        printf("]\n");
    }
    printf("flatfield_applied: %s\n",
           event->flatfield_applied ? "true" : "false");
    printf("frame_time: %f\n", event->frame_time);
    printf("goniometer/chi: start=%f increment=%f\n",
           event->goniometer.chi.start, event->goniometer.chi.increment);
    printf("goniometer/kappa: start=%f increment=%f\n",
           event->goniometer.kappa.start, event->goniometer.kappa.increment);
    printf("goniometer/omega: start=%f increment=%f\n",
           event->goniometer.omega.start, event->goniometer.omega.increment);
    printf("goniometer/phi: start=%f increment=%f\n",
           event->goniometer.phi.start, event->goniometer.phi.increment);
    printf("goniometer/two_theta: start=%f increment=%f\n",
           event->goniometer.two_theta.start,
           event->goniometer.two_theta.increment);
    printf("image_size: [%" PRIu64 " %" PRIu64 "]\n", event->image_size[0],
           event->image_size[1]);
    printf("images_per_trigger: %" PRIu64 "\n", event->images_per_trigger);
    printf("incident_energy: %f\n", event->incident_energy);
    printf("incident_wavelength: %f\n", event->incident_wavelength);
    printf("number_of_images: %" PRIu64 "\n", event->number_of_images);
    printf("number_of_triggers: %" PRIu64 "\n", event->number_of_triggers);
    for (size_t i = 0; i < event->pixel_mask.len; i++) {
        struct stream2_pixel_mask* pixel_mask = &event->pixel_mask.ptr[i];
        printf("pixel_mask: threshold %" PRIu64 ": [", pixel_mask->threshold);
        for (size_t j = 0; j < pixel_mask->array.data_len / sizeof(uint32_t);
             j++) {
            if (j >= 6) {
                printf(" ...");
                break;
            }
            if (j > 0)
                printf(" ");
            printf("%08" PRIu32, pixel_mask->array.data[j]);
        }
        printf("]\n");
    }
    printf("pixel_mask_applied: %s\n",
           event->pixel_mask_applied ? "true" : "false");
    printf("pixel_size_x: %f\n", event->pixel_size_x);
    printf("pixel_size_y: %f\n", event->pixel_size_y);
    printf("roi_mode: %s\n", event->roi_mode);
    printf("saturation_value: %" PRIu64 "\n", event->saturation_value);
    printf("sensor_material: %s\n", event->sensor_material);
    printf("sensor_thickness: %f\n", event->sensor_thickness);
    printf("series_date: %s\n", event->series_date);
    for (size_t i = 0; i < event->threshold_energy.len; i++) {
        printf("threshold_energy: threshold %" PRIu64 ": %f\n",
               event->threshold_energy.ptr[i].threshold,
               event->threshold_energy.ptr[i].energy);
    }
    printf("virtual_pixel_correction_applied: %s\n",
           event->virtual_pixel_correction_applied ? "true" : "false");
}

static size_t data_type_size(const char* data_type) {
    if (strcmp(data_type, "uint8") == 0) {
        return 1;
    } else if (strcmp(data_type, "uint16le") == 0) {
        return 2;
    } else if (strcmp(data_type, "uint32le") == 0) {
        return 4;
    } else {
        fprintf(stderr, "error: unsupported data type `%s`\n", data_type);
        exit(EXIT_FAILURE);
    }
}

static void handle_image_event(struct stream2_image_event* event) {
    printf("\nIMAGE EVENT: series_number=%" PRIu64 " series_unique_id=%s\n",
           event->series_number, event->series_unique_id);
    printf("image_number: %" PRIu64 "\n", event->image_number);
    printf("hardware_start_time: %" PRIu64 "/%" PRIu64 "\n",
           event->hardware_start_time[0], event->hardware_start_time[1]);
    printf("hardware_stop_time: %" PRIu64 "/%" PRIu64 "\n",
           event->hardware_stop_time[0], event->hardware_stop_time[1]);
    printf("hardware_exposure_time: %" PRIu64 "/%" PRIu64 "\n",
           event->hardware_exposure_time[0], event->hardware_exposure_time[1]);

    static uint8_t buffer[IMAGE_BUFFER_SIZE];

    for (size_t i = 0; i < event->channels.len; i++) {
        struct stream2_image_channel* channel = &event->channels.ptr[i];
        size_t elem_size = data_type_size(channel->data_type);

        printf("channel %zu: compression=%s data_type=%s "
               "lost_pixel_count=%" PRIu64 " thresholds=[",
               i, channel->compression, channel->data_type,
               channel->lost_pixel_count);
        print_array_uint64(&channel->thresholds);
        printf("]\n");

        const uint8_t* data;
        size_t data_len;
        if (strcmp(channel->compression, "bslz4") == 0) {
            data = buffer;
            data_len = compression_decompress_buffer(
                    COMPRESSION_BSLZ4_HDF5, (char*)buffer, sizeof(buffer),
                    (char*)channel->array.data, channel->array.data_len,
                    elem_size);
        } else if (strcmp(channel->compression, "lz4") == 0) {
            data = buffer;
            data_len = compression_decompress_buffer(
                    COMPRESSION_LZ4_HDF5, (char*)buffer, sizeof(buffer),
                    (char*)channel->array.data, channel->array.data_len,
                    elem_size);
        } else {
            fprintf(stderr, "error: unsupported compression `%s`\n",
                    channel->compression);
            exit(EXIT_FAILURE);
        }
        if (data_len == COMPRESSION_ERROR) {
            fprintf(stderr, "error: failed to decompress channel\n");
            exit(EXIT_FAILURE);
        } else if (data_len != channel->array.dimensions[0] *
                                       channel->array.dimensions[1] * elem_size)
        {
            fprintf(stderr, "error: bad data size %zu\n", data_len);
            exit(EXIT_FAILURE);
        }
        print_data(channel->array.dimensions, data, elem_size);
    }
}

static void handle_end_event(struct stream2_end_event* event) {
    printf("\nEND EVENT: series_number=%" PRIu64 " series_unique_id=%s\n",
           event->series_number, event->series_unique_id);
}

static void handle_event(struct stream2_event* event) {
    switch (event->type) {
        case STREAM2_EVENT_START:
            handle_start_event((struct stream2_start_event*)event);
            break;
        case STREAM2_EVENT_IMAGE:
            handle_image_event((struct stream2_image_event*)event);
            break;
        case STREAM2_EVENT_END:
            handle_end_event((struct stream2_end_event*)event);
            break;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s HOST\n", argv[0]);
        return EXIT_FAILURE;
    }

    char address[100];
    sprintf(address, "tcp://%s:31001", argv[1]);

    void* context = zmq_ctx_new();
    void* socket = zmq_socket(context, ZMQ_PULL);

    zmq_connect(socket, address);
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    for (;;) {
        zmq_msg_recv(&msg, socket, 0);

        const uint8_t* msg_data = (const uint8_t*)zmq_msg_data(&msg);
        size_t msg_size = zmq_msg_size(&msg);
        struct stream2_event* event;
        enum stream2_result r;
        if ((r = stream2_parse_event(msg_data, msg_size, &event))) {
            fprintf(stderr, "error: error %i parsing message\n", (int)r);
            return EXIT_FAILURE;
        }
        handle_event(event);
        stream2_free_event(event);
    }
    zmq_msg_close(&msg);
    zmq_close(socket);
    zmq_ctx_term(context);

    return EXIT_SUCCESS;
}
