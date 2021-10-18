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

int csi_fullyconnected_f32_c906(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct csi_tensor *weights,
                                struct csi_tensor *bias,
                                struct fc_params *params)
{
    float *input_data = input->data;
    float *output_data = output->data;
    float *weights_data = weights->data;
    float *bias_data = bias->data;
    const int output_dims_count = output->dim_count;
    const int weights_dims_count = weights->dim_count;
    const int batches = output->dim[0];
    const int output_depth = weights->dim[weights_dims_count - 2];  // output_nodes
    const int accum_depth = weights->dim[weights_dims_count - 1];   // input_nodes

    float zero = 0.0f;
    asm volatile(
                "mv             a0, %5\n\t"
                "loop3:\n\t"
                "mv             a1, %6\n\t"
                "loop2:\n\t"
                "mv             a2, %7\n\t"
                "vfmv.s.f       v8, %8\n\t"
                "loop1:\n\t"
                "vsetvli        t0, a2, e32, m1\n\t"
                "vlw.v          v2, (%2)\n\t"       // load input_data
                "sub            a2, a2, t0\n\t"
                "slli           t0, t0, 2\n\t"
                "add            %2, %2, t0\n\t"     // bump input_data pointer
                "vlw.v          v4, (%3)\n\t"       // load weight_data
                "add            %3, %3, t0\n\t"     // bump weight_data pointer
                "vfsub.vv       v6, v6, v6\n\t"     // clear v6
                "vfmacc.vv      v6, v2, v4\n\t"
                "vfredsum.vs    v8, v6, v8\n\t"     // v8[0] = v8[0] + sum(v6[0..i])

                "bnez           a2, loop1\n\t"

                "flw            ft0, 0(%4)\n\t"     // load bias_data
                "addi           %4, %4, 4\n\t"      // bump bias_data pointer
                "vfmv.f.s       ft1, v8\n\t"
                "fadd.s         ft2, ft1, ft0\n\t"
                "fsw            ft2, 0(%0)\n\t"     // store output_data
                "addi           %0, %0, 4\n\t"      // bump output_data pointer

                "slli           a3, %7, 2\n\t"
                "sub            %2, %2, a3\n\t"
                "addi           a1, a1, -1\n\t"
                "bnez           a1, loop2\n\t"

                "add            %2, %2, a3\n\t"
                "mul            t1, %6, %7\n\t"
                "slli           t1, t1, 2\n\t"
                "sub            %3, %3, t1\n\t"     // finish all output_nodes, jump weights_data pointer
                "slli           t2, %6, 2\n\t"
                "sub            %4, %4, t2\n\t"     // finish all output_nodes, jump bias_data pointer

                "addi           a0, a0, -1\n\t"
                "bnez           a0, loop3\n\t"

                :"=r"(output_data)  // %0
                :"0"(output_data),  // %1
                "r"(input_data),    // %2
                "r"(weights_data),  // %3
                "r"(bias_data),     // %4
                "r"(batches),       // %5
                "r"(output_depth),  // %6
                "r"(accum_depth),   // %7
                "f"(zero)           // %8
                : "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "ft0", "ft1", "ft2"
    );

    // for (int b = 0; b < batches; ++b) {
    //     for (int out_c = 0; out_c < output_depth; ++out_c) {
    //         float total = 0.f;
    //         for (int d = 0; d < accum_depth; ++d) {
    //             total += input_data[b * accum_depth + d] * weights_data[out_c * accum_depth + d];
    //         }
    //         float bias_value = 0.0f;
    //         if (bias_data != NULL) {
    //             bias_value = bias_data[out_c];
    //         }
    //         output_data[out_c + output_depth * b] = total + bias_value;
    //     }
    // }
    return CSINN_TRUE;
}

int csi_fullyconnected_u8_c906(struct csi_tensor *input,
                               struct csi_tensor *output,
                               struct csi_tensor *weights,
                               struct csi_tensor *bias,
                               struct fc_params *params)
{
    uint8_t *input_data = input->data;
    uint8_t *output_data = output->data;
    uint8_t *weights_data = weights->data;
    int32_t *bias_data = bias->data;
    const int output_dims_count = output->dim_count;
    const int weights_dims_count = weights->dim_count;
    const int batches = output->dim[0];
    const int output_depth = weights->dim[weights_dims_count - 2];
    const int accum_depth = weights->dim[weights_dims_count - 1];
    for (int b = 0; b < batches; ++b) {
        #pragma omp parallel for num_threads(8)
        for (int out_c = 0; out_c < output_depth; ++out_c) {
            int32_t acc = 0;
            for (int d = 0; d < accum_depth; ++d) {
                int32_t input_val = input_data[b * accum_depth + d];
                int32_t filter_val = weights_data[out_c * accum_depth + d];
                acc += (filter_val + weights->zero_point) * (input_val + input->zero_point);
            }
            if (bias_data != NULL) {
                acc += bias_data[out_c];
            }

            output_data[out_c + output_depth * b] =
                csi_quantize_u8(acc, output->zero_point, output->multiplier, output->shift);
        }
    }
    return CSINN_TRUE;
}