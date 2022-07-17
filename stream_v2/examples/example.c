#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

#include "stream2.h"

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
    if (msg->countrate_correction_lookup_table.ptr) {
        printf("countrate_correction_lookup_table: %zu entries cutoff %" PRIu32
               "\n",
               msg->countrate_correction_lookup_table.len,
               msg->countrate_correction_lookup_table.len >= 2
                       ? msg->countrate_correction_lookup_table.ptr
                                 [msg->countrate_correction_lookup_table.len -
                                  2]
                       : 0);
    }
    printf("detector_description: \"%s\"\n",
           msg->detector_description ? msg->detector_description : "");
    printf("detector_serial_number: \"%s\"\n",
           msg->detector_serial_number ? msg->detector_serial_number : "");
    printf("detector_translation: [%f %f %f]\n", msg->detector_translation[0],
           msg->detector_translation[1], msg->detector_translation[2]);
    for (size_t i = 0; i < msg->flatfield.len; i++) {
        struct stream2_flatfield* flatfield = &msg->flatfield.ptr[i];
        printf("flatfield: \"%s\" dim [%" PRIu64 " %" PRIu64 "]\n",
               flatfield->channel, flatfield->flatfield.dim[0],
               flatfield->flatfield.dim[1]);
        for (uint64_t ri = 0, re = flatfield->flatfield.dim[0]; ri < re; ri++) {
            if (ri == 6 && re > 13) {
                ri = re - 6 - 1;
                for (uint64_t ci = 0, ce = flatfield->flatfield.dim[1]; ci < ce;
                     ci++) {
                    if (ci == 6 && ce > 13) {
                        ci = ce - 6 - 1;
                        printf(":: ");
                        continue;
                    }
                    printf(":   : ");
                }
            } else {
                for (uint64_t ci = 0, ce = flatfield->flatfield.dim[1]; ci < ce;
                     ci++) {
                    if (ci == 6 && ce > 13) {
                        ci = ce - 6 - 1;
                        printf(".. ");
                        continue;
                    }
                    const float f =
                            flatfield->flatfield.array.ptr[ri * ce + ci];
                    if (f >= 0.f && f < 10.f)
                        printf("%.3f ", f != 0.f ? f : 0.f);
                    else
                        printf("##### ");
                }
            }
            printf("\n");
        }
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
    printf("image_size_x: %" PRIu64 "\n", msg->image_size_x);
    printf("image_size_y: %" PRIu64 "\n", msg->image_size_y);
    printf("incident_energy: %f\n", msg->incident_energy);
    printf("incident_wavelength: %f\n", msg->incident_wavelength);
    printf("number_of_images: %" PRIu64 "\n", msg->number_of_images);
    for (size_t i = 0; i < msg->pixel_mask.len; i++) {
        struct stream2_pixel_mask* pixel_mask = &msg->pixel_mask.ptr[i];
        printf("pixel_mask: \"%s\" dim [%" PRIu64 " %" PRIu64 "]\n",
               pixel_mask->channel, pixel_mask->pixel_mask.dim[0],
               pixel_mask->pixel_mask.dim[1]);
        for (uint64_t ri = 0, re = pixel_mask->pixel_mask.dim[0]; ri < re; ri++)
        {
            if (ri == 4 && re > 9) {
                ri = re - 4 - 1;
                for (uint64_t ci = 0, ce = pixel_mask->pixel_mask.dim[1];
                     ci < ce; ci++) {
                    if (ci == 4 && ce > 9) {
                        ci = ce - 4 - 1;
                        printf(":: ");
                        continue;
                    }
                    printf(":      : ");
                }
            } else {
                for (uint64_t ci = 0, ce = pixel_mask->pixel_mask.dim[1];
                     ci < ce; ci++) {
                    if (ci == 4 && ce > 9) {
                        ci = ce - 4 - 1;
                        printf(".. ");
                        continue;
                    }
                    printf("%08" PRIx32 " ",
                           pixel_mask->pixel_mask.array.ptr[ri * ce + ci]);
                }
            }
            printf("\n");
        }
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
    for (size_t i = 0; i < msg->data.len; i++) {
        struct stream2_image_data* data = &msg->data.ptr[i];
        printf("data: \"%s\" dim [%" PRIu64 " %" PRIu64 "] elem size %zu\n",
               data->channel, data->data.dim[0], data->data.dim[1],
               data->elem_size);
        for (uint64_t ri = 0, re = data->data.dim[0]; ri < re; ri++) {
            if (ri == 12 && re > 25) {
                ri = re - 12 - 1;
                for (uint64_t ci = 0, ce = data->data.dim[1] * data->elem_size;
                     ci < ce; ci++) {
                    if (ci >= 25)
                        break;
                    printf(":: ");
                }
            } else {
                for (uint64_t ci = 0, ce = data->data.dim[1] * data->elem_size;
                     ci < ce; ci++) {
                    if (ci == 12 && ce > 25) {
                        ci = ce - 12 - 1;
                        printf(".. ");
                        continue;
                    }
                    printf("%02" PRIx8 " ",
                           ((uint8_t*)data->data.array.ptr)[ri * ce + ci]);
                }
            }
            printf("\n");
        }
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
