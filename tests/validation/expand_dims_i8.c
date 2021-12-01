/*
 * Copyright (C) 2016-2021 C-SKY Limited. All rights reserved.
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

#include "test_utils.h"
#include "csi_nn.h"
#include "math_snr.h"

int main(int argc, char** argv)
{
    init_testsuite("Testing function of expand_dims i8.\n");

    struct csi_tensor *input = csi_alloc_tensor(NULL);
    struct csi_tensor *output = csi_alloc_tensor(NULL);
    struct csi_tensor *reference = csi_alloc_tensor(NULL);
    struct expand_dims_params params;
    int in_size = 1;
    int out_size = 1;
    int zp, quantized_multiplier, shift;
    float scale, min_value, max_value;
    float max_error = 0.0f;
    int *buffer = read_input_data_f32(argv[1]);

    int dim_count = buffer[0];
    int axis = buffer[1];
    for(int i = 0; i < dim_count; i++) {
        input->dim[i] = buffer[2 + i];
        in_size *= input->dim[i];
    }
    input->dim_count = dim_count;
    output->dim_count = input->dim_count + 1;

    for(int i = 0; i < output->dim_count; i++) {
        if(i < axis) {
            output->dim[i] = input->dim[i];
        } else if(i == axis) {
            output->dim[i] = 1;
        } else {
            output->dim[i] = input->dim[i - 1];
        }
    }

    input->dtype = CSINN_DTYPE_INT8;
    output->dtype = CSINN_DTYPE_INT8;
    out_size = in_size;
    params.base.api = CSINN_API;
    params.base.run_mode = CSINN_RM_LAYER;


    float *src_in   = (float *)(buffer + 2 + dim_count);
    float *ref      = (float *)(buffer + 2 + dim_count + in_size);
    int8_t *src_tmp = malloc(in_size * sizeof(char));

    input->qinfo = get_quant_info_i8(src_in, in_size);

    for(int i = 0; i < in_size; i++) {
        src_tmp[i] = csi_ref_quantize_f32_to_i8(src_in[i], input->qinfo);
    }

    /* compute the max quantize error */
    for(int i = 0; i < in_size; i++) {
        float error1;
        float output_tmp  = csi_ref_dequantize_i8_to_f32(src_tmp[i], input->qinfo);
        if(isinf(src_in[i]) || isnan(src_in[i])){
            continue;
        } else {
            error1 = fabs(src_in[i] -output_tmp);
            if(error1 > 1e-6) {
                error1 = fabs(src_in[i] - output_tmp)/fabs(src_in[i] + 1e-9);
            }
        }
        if(error1 > max_error) {
            max_error = error1;
        }
    }


    output->qinfo = get_quant_info_i8(ref, out_size);

    input->data     = src_tmp;
    reference->data = ref;
    output->data    = malloc(out_size * sizeof(char));

    float difference = argc > 2 ? atof(argv[2]) : max_error;

    if (csi_expand_dims_init(input, output, &params) == CSINN_TRUE) {
        csi_expand_dims(input, output, &params);
    }

    result_verify_8(reference->data, output, input->data, difference, out_size, false);

    free(buffer);
    free(src_tmp);
    free(output->data);
    return done_testing();
}