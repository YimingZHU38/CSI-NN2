/*
 * Copyright (C) 2016-2020 C-SKY Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "csi_nn.h"
#include "csi_utils.h"

static int csi_deconv2d_nhwc_u8(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct csi_tensor *kernel,
                                struct csi_tensor *bias,
                                struct conv2d_params *params)
{
    uint8_t *input_data = input->data;
    uint8_t *output_data = output->data;
    uint8_t *filter_data = kernel->data;
    int32_t *bias_data = bias->data;
    const int batches = input->dim[0];
    const int input_depth = input->dim[3];
    const int output_depth = output->dim[3];
    const int input_height = input->dim[1];
    const int input_width = input->dim[2];
    const int filter_height = kernel->dim[1];
    const int filter_width = kernel->dim[2];
    const int output_height = output->dim[1];
    const int output_width = output->dim[2];
    const int output_batch = output->dim[0];
    const int32_t input_offset = input->zero_point;
    const int32_t filter_offset = kernel->zero_point;
    const int32_t output_offset = output->zero_point;
    const int32_t output_multiplier = output->multiplier;
    const int output_shift = output->shift;

    int num_elements = 1;
    for (int i = 0; i < output->dim_count; i++) {
        num_elements *= output->dim[i];
    }
    // We need to initialize scratch_buffer to all 0s, as we apply the same
    // 'scatter' based trick as in float version.
    int32_t *scratch_buffer = malloc(num_elements * sizeof(int32_t));
    memset(scratch_buffer, 0, num_elements * sizeof(int32_t));

    // Loop through input elements one at a time.
    for (int batch = 0; batch < batches; ++batch) {
        for (int in_y = 0; in_y < input_height; ++in_y) {
            for (int in_x = 0; in_x < input_width; ++in_x) {
                for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                    // Loop through the output elements it will influence.
                    const int out_x_origin = (in_x * params->stride_width) - params->pad_left;
                    const int out_y_origin = (in_y * params->stride_height) - params->pad_top;
                    for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
                        for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                            for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
                                // Compute output element location.
                                const int out_x = out_x_origin + filter_x;
                                const int out_y = out_y_origin + filter_y;
                                // We cannot accumulate out of bounds.
                                if ((out_x >= 0) && (out_x < output_width) && (out_y >= 0) &&
                                    (out_y < output_height)) {
                                    uint8_t input_value = input_data[csi_get_index(
                                        input->dim, batch, in_y, in_x, in_channel)];
                                    uint8_t filter_value = filter_data[csi_get_index(
                                        kernel->dim, out_channel, filter_y, filter_x, in_channel)];
                                    scratch_buffer[csi_get_index(output->dim, batch, out_y, out_x,
                                                                 out_channel)] +=
                                        (input_value - input_offset) *
                                        (filter_value - filter_offset);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (bias->dim_count != 0){
        for (int batch = 0; batch < output_batch; batch++) {
            for (int o_y = 0; o_y < output_height; o_y++) {
                for (int o_x = 0; o_x < output_width; o_x++) {
                    for (int o_channel = 0; o_channel < output_depth; ++o_channel) {
                        scratch_buffer[csi_get_index(output->dim, batch, o_y, o_x,
                                o_channel)] += bias_data[o_channel];
                    }
                }
            }
        }
    }

    for (int i = 0; i < num_elements; ++i) {
        output_data[i] =
            csi_quantize_u8(scratch_buffer[i], output->zero_point, output->multiplier, output->shift);
    }

    return CSINN_TRUE;
}

static int csi_deconv2d_nchw_u8(struct csi_tensor *o_input,
                                struct csi_tensor *o_output,
                                struct csi_tensor *o_kernel,
                                struct csi_tensor *o_bias,
                                struct conv2d_params *params)
{
    struct csi_tensor* input = csi_nchw_to_nhwc_8(o_input);
    struct csi_tensor* output = csi_nchw_to_nhwc_8(o_output);
    int32_t permute[4] = {1, 2, 3, 0};
    struct csi_tensor* kernel = csi_deconv_kernel_nchw_to_nhwc_u8(o_kernel, permute);
    struct csi_tensor* bias = o_bias;

    uint8_t *input_data = input->data;
    uint8_t *output_data = output->data;
    uint8_t *filter_data = kernel->data;
    int32_t *bias_data = bias->data;
    const int batches = input->dim[0];
    const int input_depth = input->dim[3];
    const int output_depth = output->dim[3];
    const int input_height = input->dim[1];
    const int input_width = input->dim[2];
    const int filter_height = kernel->dim[1];
    const int filter_width = kernel->dim[2];
    const int output_height = output->dim[1];
    const int output_width = output->dim[2];
    const int output_batch = output->dim[0];
    const int32_t input_offset = input->zero_point;
    const int32_t filter_offset = kernel->zero_point;
    const int32_t output_offset = output->zero_point;
    const int32_t output_multiplier = output->multiplier;
    const int output_shift = output->shift;

    int num_elements = 1;
    for (int i = 0; i < output->dim_count; i++) {
        num_elements *= output->dim[i];
    }
    // We need to initialize scratch_buffer to all 0s, as we apply the same
    // 'scatter' based trick as in float version.
    int32_t *scratch_buffer = malloc(num_elements * sizeof(int32_t));
    memset(scratch_buffer, 0, num_elements * sizeof(int32_t));

    // Loop through input elements one at a time.
    for (int batch = 0; batch < batches; ++batch) {
        for (int in_y = 0; in_y < input_height; ++in_y) {
            for (int in_x = 0; in_x < input_width; ++in_x) {
                for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                    // Loop through the output elements it will influence.
                    const int out_x_origin = (in_x * params->stride_width) - params->pad_left;
                    const int out_y_origin = (in_y * params->stride_height) - params->pad_top;
                    for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
                        for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                            for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
                                // Compute output element location.
                                const int out_x = out_x_origin + filter_x;
                                const int out_y = out_y_origin + filter_y;
                                // We cannot accumulate out of bounds.
                                if ((out_x >= 0) && (out_x < output_width) && (out_y >= 0) &&
                                    (out_y < output_height)) {
                                    uint8_t input_value = input_data[csi_get_index(
                                        input->dim, batch, in_y, in_x, in_channel)];
                                    uint8_t filter_value = filter_data[csi_get_index(
                                        kernel->dim, out_channel, filter_y, filter_x, in_channel)];
                                    scratch_buffer[csi_get_index(output->dim, batch, out_y, out_x,
                                                                 out_channel)] +=
                                        (input_value - input_offset) *
                                        (filter_value - filter_offset);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (bias->dim_count != 0){
        for (int batch = 0; batch < output_batch; batch++) {
            for (int o_y = 0; o_y < output_height; o_y++) {
                for (int o_x = 0; o_x < output_width; o_x++) {
                    for (int o_channel = 0; o_channel < output_depth; ++o_channel) {
                        scratch_buffer[csi_get_index(output->dim, batch, o_y, o_x,
                                o_channel)] += bias_data[o_channel];
                    }
                }
            }
        }
    }

    for (int i = 0; i < num_elements; ++i) {
        output_data[i] =
            csi_quantize_u8(scratch_buffer[i], output->zero_point, output->multiplier, output->shift);
    }

    csi_nhwc_to_nchw_8(o_output, output);
    return CSINN_TRUE;
}

int csi_depthwise_deconv2d_u8(struct csi_tensor *o_input,
                                          struct csi_tensor *o_output,
                                          struct csi_tensor *o_kernel,
                                          struct csi_tensor *o_bias,
                                          struct conv2d_params *params)
{
    struct csi_tensor* input = csi_nchw_to_nhwc_8(o_input);
    struct csi_tensor* output = csi_nchw_to_nhwc_8(o_output);
    int32_t permute[4] = {1, 2, 3, 0};
    struct csi_tensor* kernel = csi_deconv_kernel_nchw_to_nhwc_u8(o_kernel, permute);
    struct csi_tensor* bias = o_bias;

    uint8_t *input_data = input->data;
    uint8_t *output_data = output->data;
    uint8_t *filter_data = kernel->data;
    int32_t *bias_data = bias->data;
    const int batches = input->dim[0];
    const int input_depth = input->dim[3];
    const int output_depth = output->dim[3];
    const int input_height = input->dim[1];
    const int input_width = input->dim[2];
    const int filter_height = kernel->dim[1];
    const int filter_width = kernel->dim[2];
    const int output_height = output->dim[1];
    const int output_width = output->dim[2];
    const int output_batch = output->dim[0];
    const int32_t input_offset = input->zero_point;
    const int32_t filter_offset = kernel->zero_point;
    const int32_t output_offset = output->zero_point;
    const int32_t output_multiplier = output->multiplier;
    const int output_shift = output->shift;

    int num_elements = 1;
    for (int i = 0; i < output->dim_count; i++) {
        num_elements *= output->dim[i];
    }
    // We need to initialize scratch_buffer to all 0s, as we apply the same
    // 'scatter' based trick as in float version.
    int32_t *scratch_buffer = malloc(num_elements * sizeof(int32_t));
    memset(scratch_buffer, 0, num_elements * sizeof(int32_t));

    // Loop through input elements one at a time.
    for (int batch = 0; batch < batches; ++batch) {
        for (int in_y = 0; in_y < input_height; ++in_y) {
            for (int in_x = 0; in_x < input_width; ++in_x) {
                for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                    // Loop through the output elements it will influence.
                    const int out_x_origin = (in_x * params->stride_width) - params->pad_left;
                    const int out_y_origin = (in_y * params->stride_height) - params->pad_top;
                    for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
                        for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
							// Compute output element location.
							const int out_x = out_x_origin + filter_x;
							const int out_y = out_y_origin + filter_y;
							// We cannot accumulate out of bounds.
							if ((out_x >= 0) && (out_x < output_width) && (out_y >= 0) &&
								(out_y < output_height)) {
								uint8_t input_value = input_data[csi_get_index(
									input->dim, batch, in_y, in_x, in_channel)];
								uint8_t filter_value = filter_data[csi_get_index(
									kernel->dim, 0, filter_y, filter_x, in_channel)];
								scratch_buffer[csi_get_index(output->dim, batch, out_y, out_x,
															 in_channel)] +=
									(input_value - input_offset) *
									(filter_value - filter_offset);
							}
                        }
                    }
                }
            }
        }
    }
    if (bias->dim_count != 0){
        for (int batch = 0; batch < output_batch; batch++) {
            for (int o_y = 0; o_y < output_height; o_y++) {
                for (int o_x = 0; o_x < output_width; o_x++) {
                    for (int o_channel = 0; o_channel < output_depth; ++o_channel) {
                        scratch_buffer[csi_get_index(output->dim, batch, o_y, o_x,
                                o_channel)] += bias_data[o_channel];
                    }
                }
            }
        }
    }

    for (int i = 0; i < num_elements; ++i) {
        output_data[i] =
            csi_quantize_u8(scratch_buffer[i], output->zero_point, output->multiplier, output->shift);
    }

    csi_nhwc_to_nchw_8(o_output, output);
    return CSINN_TRUE;
}

int csi_deconv2d_u8(struct csi_tensor *input,
                    struct csi_tensor *output,
                    struct csi_tensor *kernel,
                    struct csi_tensor *bias,
                    struct conv2d_params *params)
{
    if (params->layout == CSINN_NCHW) {
        csi_deconv2d_nchw_u8(input, output, kernel, bias, params);
    } else if (params->layout == CSINN_NHWC) {
        csi_deconv2d_nhwc_u8(input, output, kernel, bias, params);
    } else {
        return CSINN_UNSUPPORT_LAYOUT;
    }
}

int csi_deconv2d_init(struct csi_tensor *input,
                      struct csi_tensor *output,
                      struct csi_tensor *kernel,
                      struct csi_tensor *bias,
                      struct conv2d_params *params)
{
    if (params->group == 1) {
        params->bc = csi_bc_map(params->api, CSINN_OP_DECONV2D, input->dtype);
        if (params->bc == NULL) {
            return CSINN_UNSUPPORT_DTYPE;
        }
    } else if (params->group == output->dim[1] && params->layout == CSINN_NCHW) {
        params->bc = csi_bc_map(params->api, CSINN_OP_DEPTHWISE_DECONV2D, input->dtype);
        if (params->bc == NULL) {
            return CSINN_UNSUPPORT_DTYPE;
        }
    } else {
        return CSINN_FALSE;
    }
    return CSINN_TRUE;
}

int csi_deconv2d(struct csi_tensor *input,
                 struct csi_tensor *output,
                 struct csi_tensor *kernel,
                 struct csi_tensor *bias,
                 struct conv2d_params *params)
{
    if (params->bc != NULL) {
        params->bc(input, output, kernel, bias, params);
    } else {
        return CSINN_CALLBACK_UNSET;
    }
    return CSINN_TRUE;
}
