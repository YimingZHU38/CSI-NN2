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

//the input->data is a 4-D Tensor with shape [batch, depth, height, width].
int csi_batch_to_space_f32(struct csi_tensor *input,
                           struct csi_tensor *output,
                           struct batch_to_space_params *params)
{
    float *input_data = (float *)input->data;
    float *output_data = (float *)output->data;

    int batch = input->dim[0];
    int in_channel = input->dim[1];
    int in_height = input->dim[2];
    int in_width = input->dim[3];

    int block_size = params->block_size;
    int block_size2 = block_size * block_size;

    int out_batch =  output->dim[0];    //out_batch = batch / block_size2;
    int out_channel = output->dim[1];   //out_channel = in_channel;
    int out_height = output->dim[2];    //out_height = in_height * block_size - params->crop_top - params->crop_bottom;
    int out_width = output->dim[3];     //out_width = in_width * block_size - params->crop_left - params->crop_right;

    for(int out_b = 0; out_b < out_batch; ++out_b) {
        for(int in_h = 0; in_h< in_height; ++in_h) {
            for(int in_w = 0; in_w < in_width; ++in_w) {
                for(int out_c=0; out_c<out_channel; ++out_c) {

                    float *temp = (float *)malloc(block_size2 * sizeof(float));
                    int in_start_addr = csi_get_index(input->dim, out_b, out_c, in_h, in_w);
                    for(int i = 0; i < block_size2; ++i) {
                        temp[i] = input_data[in_start_addr + i * out_batch * out_channel * in_height * in_width];
                    }

                    for(int h = 0; h < block_size; ++h) {
                        for(int w = 0; w < block_size; ++w) {
                            int h_now = in_h * block_size + h - params->crop_top;
                            int w_now = in_w * block_size + w - params->crop_left;
                            if(h_now >= 0 && h_now < out_height && w_now >= 0 && w_now < out_width) {
                                int out_addr = csi_get_index(output->dim, out_b, out_c, h_now, w_now);
                                output_data[out_addr] = temp[h * block_size + w];
                            }
                        }
                    }
                    free(temp);
                }
            }
        }
    }
    return CSINN_TRUE;
}

int csi_batch_to_space_u8(struct csi_tensor *input,
                          struct csi_tensor *output,
                          struct batch_to_space_params *params)
{
    uint8_t *input_data = (uint8_t *)input->data;
    uint8_t *output_data = (uint8_t *)output->data;

    int batch = input->dim[0];
    int in_channel = input->dim[1];
    int in_height = input->dim[2];
    int in_width = input->dim[3];

    int block_size = params->block_size;
    int block_size2 = block_size * block_size;

    int out_batch =  batch/block_size2;
    int out_channel = in_channel;
    int out_height = in_height * block_size - params->crop_top - params->crop_bottom;
    int out_width = in_width * block_size - params->crop_left - params->crop_right;

    for(int out_b = 0; out_b < out_batch; ++out_b) {
        for(int in_h = 0; in_h< in_height; ++in_h) {
            for(int in_w = 0; in_w < in_width; ++in_w) {
                for(int out_c=0; out_c<out_channel; ++out_c) {

                    uint8_t *temp = (uint8_t *)malloc(block_size2 * sizeof(uint8_t));
                    int in_start_addr = csi_get_index(input->dim, out_b, out_c, in_h, in_w);
                    for(int i = 0; i < block_size2; ++i) {
                        temp[i] = input_data[in_start_addr + i * out_batch * out_channel * in_height * in_width];
                    }

                    for(int h = 0; h < block_size; ++h) {
                        for(int w = 0; w < block_size; ++w) {
                            int h_now = in_h * block_size + h - params->crop_top;
                            int w_now = in_w * block_size + w - params->crop_left;
                            if(h_now >= 0 && h_now < out_height && w_now >= 0 && w_now < out_width) {
                                int out_addr = csi_get_index(output->dim, out_b, out_c, h_now, w_now);
                                // output_data[out_addr] = temp[h * block_size + w];
                                output_data[out_addr] = csi_requantize_u8(temp[h * block_size + w], input->zero_point, input->multiplier, input->shift,
                                                                     output->zero_point, output->multiplier, output->shift);
                            }
                        }
                    }
                    free(temp);
                }
            }
        }
    }
    return CSINN_TRUE;
}

int csi_batch_to_space_init(struct csi_tensor *input,
                            struct csi_tensor *output,
                            struct batch_to_space_params *params)
{
    params->bc = csi_bc_map(params->api, CSINN_OP_BATCH_TO_SPACE, input->dtype);
    if (params->bc == NULL) {
        return CSINN_UNSUPPORT_DTYPE;
    }
    return CSINN_TRUE;
}

int csi_batch_to_space(struct csi_tensor *input,
                       struct csi_tensor *output,
                       struct batch_to_space_params *params)
{
    if (params->bc != NULL) {
        params->bc(input, output, params);
    } else {
        return CSINN_CALLBACK_UNSET;
    }
    return CSINN_TRUE;
}
