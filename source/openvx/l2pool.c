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
#include "csi_ovx.h"


int csi_ovx_l2pool(struct csi_tensor *input,
                   struct csi_tensor *output,
                   struct pool_params *params)
{
    vsi_nn_node_t *node;
    vsi_nn_node_id_t node_id;
    vsi_nn_tensor_id_t input_id;
    vsi_nn_tensor_attr_t attr;
    vsi_nn_tensor_id_t output_id;
    vsi_nn_graph_t *graph = csi_ovx_get_graph(input->sess);
    output->sess = input->sess;
    uint32_t input_num = 1;
    uint32_t output_num = 1;
    node = vsi_nn_AddNode(graph, VSI_NN_OP_POOL, input_num, output_num, &node_id);
    node->nn_param.pool.ksize[0] = params->filter_width;
    node->nn_param.pool.ksize[1] = params->filter_height;
    node->nn_param.pool.stride[0] = params->stride_width;
    node->nn_param.pool.stride[1] = params->stride_height;

    int ceil_mode_h = csi_get_ceil_mode_fix(input->dim[2], params->filter_height,
                                            params->stride_height, params->pad_top);
    int ceil_mode_w = csi_get_ceil_mode_fix(input->dim[3], params->filter_width,
                                            params->stride_width, params->pad_right);
    node->nn_param.pool.pad[0] = params->pad_right;
    node->nn_param.pool.pad[1] = params->pad_right + ceil_mode_w;
    node->nn_param.pool.pad[2] = params->pad_top;
    node->nn_param.pool.pad[3] = params->pad_top + ceil_mode_h;
    node->nn_param.pool.type = VX_CONVOLUTIONAL_NETWORK_POOLING_L2;
    node->nn_param.pool.round_type = VSI_NN_ROUND_CEIL;
    node->vx_param.down_scale_size_rounding = VX_CONVOLUTIONAL_NETWORK_DS_SIZE_ROUNDING_FLOOR;

    attr.dtype.fmt = VSI_NN_DIM_FMT_NCHW;

    /* input */
    node->input.tensors[0] = (vsi_nn_tensor_id_t)input->data;

    /* output */
    attr.dtype.scale = output->scale;
    attr.dtype.zero_point = output->zero_point;
    attr.dtype.qnt_type = VSI_NN_QNT_TYPE_AFFINE_ASYMMETRIC;
    memset(attr.size, 0, VSI_NN_MAX_DIM_NUM * sizeof(uint32_t));
    attr.dim_num = VSI_NN_DIM_AUTO;
    attr.vtl = TRUE;
    attr.is_const = FALSE;
    attr.dtype.vx_type = VSI_NN_TYPE_UINT8;
    output_id = vsi_nn_AddTensor(graph, VSI_NN_TENSOR_ID_AUTO, &attr, NULL);
    node->output.tensors[0] = output_id;
    output->data = (void *)output_id;
}

