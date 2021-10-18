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

/* logsoftmax = logits - log(reduce_sum(exp(logits), axis)) */
int csi_log_softmax_f32(struct csi_tensor *input,
                        struct csi_tensor *output,
                        struct softmax_params *params)
{
    // now only support 2D input
    assert(params->axis == 1 && input->dim_count == 2);
    float *input_data = (float *)input->data;
    float *output_data = (float *)output->data;

    int in_size = 1, out_size = 1;
    for(int i = 0; i < input->dim_count; i++) {
        in_size *= input->dim[i];
    }
    out_size = in_size;
    int input_outer_size = 1;
    for(int i = 0; i < params->axis; i++) {
        input_outer_size *= input->dim[i];
    }
    int input_inner_size = 1;
    for(int i = params->axis + 1; i < input->dim_count; i++) {
        input_inner_size *= input->dim[i];
    }
    int axis_dim = input->dim[params->axis];

    for(int i = 0; i < input_outer_size; i++) {
        for(int k = 0; k < input_inner_size; k++) {
            float acc = 0.0f;
            float input_val = 0.0f;
            for(int j = 0; j < axis_dim; j++) {
                input_val = *(input_data + j * input_inner_size + k);
                acc += exp(input_val);
            }
            acc = log(acc);
            for(int j = 0; j < axis_dim; j++) {
                *(output_data + j * input_inner_size + k) = *(input_data + j * input_inner_size + k) - acc;
            }
        }
        input_data += input_inner_size * axis_dim;
        output_data += input_inner_size * axis_dim;
    }
    return CSINN_TRUE;
}

int csi_log_softmax_u8(struct csi_tensor *input,
                       struct csi_tensor *output,
                       struct softmax_params *params)
{
    // now only support 2D input
    assert(params->axis == 1 && input->dim_count == 2);
    uint8_t *input_data = (uint8_t *)input->data;
    uint8_t *output_data = (uint8_t *)output->data;

    int in_size = 1, out_size = 1;
    for(int i = 0; i < input->dim_count; i++) {
        in_size *= input->dim[i];
    }
    out_size = in_size;
    int input_outer_size = 1;
    for(int i = 0; i < params->axis; i++) {
        input_outer_size *= input->dim[i];
    }
    int input_inner_size = 1;
    for(int i = params->axis + 1; i < input->dim_count; i++) {
        input_inner_size *= input->dim[i];
    }
    int axis_dim = input->dim[params->axis];

    for(int i = 0; i < input_outer_size; i++) {
        for(int k = 0; k < input_inner_size; k++) {
            float acc = 0.0f;
            float input_temp = 0.0f;
            for(int j = 0; j < axis_dim; j++) {
                uint8_t input_val = *(input_data + j * input_inner_size + k);
                input_temp = csi_dequantize_u8_to_f32(input_val, input->zero_point, input->multiplier, input->shift);
                acc += exp(input_temp);
            }
            acc = log(acc);
            for(int j = 0; j < axis_dim; j++) {
                input_temp = csi_dequantize_u8_to_f32(*(input_data + j * input_inner_size + k), input->zero_point, input->multiplier, input->shift);
                *(output_data + j * input_inner_size + k) = csi_quantize_f32_to_u8(input_temp - acc, output->zero_point, output->multiplier, output->shift);
            }
        }
        input_data += input_inner_size * axis_dim;
        output_data += input_inner_size * axis_dim;
    }
    return CSINN_TRUE;
}

int csi_log_softmax_init(struct csi_tensor *input,
                         struct csi_tensor *output,
                         struct softmax_params *params)
{
    params->bc = csi_bc_map(params->api, CSINN_OP_LOG_SOFTMAX, input->dtype);
    if (params->bc == NULL) {
        return CSINN_UNSUPPORT_DTYPE;
    }
    return CSINN_TRUE;
}

int csi_log_softmax(struct csi_tensor *input,
                    struct csi_tensor *output,
                    struct softmax_params *params)
{
    if(params->bc != NULL) {
        params->bc(input, output, params);
    } else {
        return CSINN_CALLBACK_UNSET;
    }
    return CSINN_TRUE;
}