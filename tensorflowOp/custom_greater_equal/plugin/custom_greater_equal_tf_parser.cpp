﻿    /* Copyright (c) Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
    *
    * This program is free software; you can redistribute it and/or modify
    * it under the terms of the Apache License Version 2.0.You may not use this file except in compliance with the License.
    *
    * This program is distributed in the hope that it will be useful,
    * but WITHOUT ANY WARRANTY; without even the implied warranty of
    * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    * Apache License for more details at
    * http:// www.apache.org/licenses/LICENSE-2.0
    */

#include <Python.h>
#include "custom/custom_op.h"
#include "framework/omg/register.h"
#include "framework/omg/omg_types.h"
#include "operator.h"
#include "attr_value.h"
#include <memory>
#include <string>
#include <vector>

using namespace ge;
namespace domi {
const int DOMI_COMMON_ONE = 1;
const int DOMI_COMMON_TWO = 2;
const int DOMI_COMMON_THREE = 3;
// Shape broadcast function
bool ProduceShape(vector<int64_t> shape1, vector<int64_t> shape2, vector<int64_t> &output_shape)
{
    vector<int64_t> temp;
    if (shape1.size() < shape2.size()) {
        temp = shape1;
        shape1 = shape2;
        shape2 = temp;
    }
    int64_t output_shape_size = shape1.size();
    int64_t dec = output_shape_size - shape2.size();
    for (int64_t i = 0; i < dec; i++) {
        shape2.insert(shape2.begin(), 1);
    }

    output_shape.clear();
    for (int64_t i = 0; i < output_shape_size; i++) {
        if (shape1[i] != shape2[i] && shape1[i] != 1 && shape2[i] != 1) {
            printf("the two input shape broadcast failed!\n");
            return 0;
        }
        output_shape.push_back(shape1[i] > shape2[i] ? shape1[i] : shape2[i]);
    }
    return 1;
}

// #### Obtains the processing function of the output tensor description.
Status TFcustomGreaterEqualInferShapeAndType(const ge::Operator &op, vector<ge::TensorDesc> &v_output_desc)
{
    auto tensorDesc = op.GetInputDesc(0);
    auto tensorDesc2 = op.GetInputDesc(1);
    auto shape1 = tensorDesc.GetShape();
    auto shape2 = tensorDesc2.GetShape();

    vector<int64_t> dims1 = shape1.GetDims();
    vector<int64_t> dims2 = shape2.GetDims();
    vector<int64_t> outputDims;

    if (ProduceShape(dims1, dims2, outputDims) == 0) { // Shape broadcast
        return FAILED;
    }

    for (size_t i = 0; i < outputDims.size(); i++) {
        shape1.SetDim(i, outputDims[i]);
    }

    tensorDesc.SetShape(shape1);
    v_output_desc.push_back(tensorDesc);

    return SUCCESS;
}

// build Te Binary file
Status TFcustomGreaterEqualBuildTeBin(const ge::Operator &op, TEBinInfo &te_bin_info)
{
    // ### Parse input tensor description
    TensorDesc inputDesc = op.GetInputDesc(0);
    TensorDesc inputDesc2 = op.GetInputDesc(1);

    std::string filePath = "../operator/custom_greater_equal";
    std::string funcName = "custom_greater_equal";
    std::string kernelName = "custom_greater_equal" + std::to_string(inputDesc.GetShape().GetDim(0)) + std::to_string(inputDesc.GetShape().GetDim(DOMI_COMMON_ONE));

    // get real path of Py module
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        printf("Get current directory path failed!\n");
        return FAILED;
    }
    std::string cwdS(cwd);
    char *realPath = realpath((cwdS + "/" + filePath + ".py").c_str(), NULL);
    if (realPath == NULL) {
        printf("Get real path of Py module failed!\n");
        return FAILED;
    }
    std::string realFilePathString = std::string(realPath);
    std::string realFilePath = realFilePathString.substr(0, realFilePathString.rfind("."));

    std::map<ge::DataType, std::string> operation_map = {
        { ge::DT_UNDEFINED,      "undefined" },
        { ge::DT_FLOAT,          "float32" },
        { ge::DT_FLOAT16,        "float16" },
        { ge::DT_INT8,           "int8" },
        { ge::DT_INT16,          "int16" },
        { ge::DT_INT32,          "int32" },
        { ge::DT_INT64,          "int64" },
        { ge::DT_UINT8,          "uint8" },
        { ge::DT_UINT16,         "uint16" },
        { ge::DT_UINT32,         "uint32" },
        { ge::DT_UINT64,         "uint64" },
        { ge::DT_BOOL,           "bool" },
        { ge::DT_DOUBLE,         "double" },
        { ge::DT_DUAL,           "dual" },
        { ge::DT_DUAL_SUB_INT8,  "dual_sub_int8" },
        { ge::DT_DUAL_SUB_UINT8, "dual_sub_uint8" }
    };
    std::string dtype = operation_map[op.GetInputDesc(0).GetDataType()];

    // i => int; s => string; f => dobule; O => bool, and bool value is Py_True or Py_False
    te::BuildTeCustomOp(te_bin_info.ddk_version, op.GetName(), realFilePath, funcName,
                        "(i,i,i,i),(i,i,i,i),s, s,O",
                        inputDesc.GetShape().GetDim(0), inputDesc.GetShape().GetDim(DOMI_COMMON_ONE),
                        inputDesc.GetShape().GetDim(DOMI_COMMON_TWO), inputDesc.GetShape().GetDim(DOMI_COMMON_THREE),
                        inputDesc2.GetShape().GetDim(0), inputDesc2.GetShape().GetDim(DOMI_COMMON_ONE),
                        inputDesc2.GetShape().GetDim(DOMI_COMMON_TWO), inputDesc2.GetShape().GetDim(DOMI_COMMON_THREE),
                        dtype.c_str(),
                        kernelName.c_str(),
                        Py_True);

    // set te op json to te_bin_info
    te_bin_info.bin_file_path = "./kernel_meta/" + kernelName + ".o";
    te_bin_info.json_file_path = "./kernel_meta/" + kernelName + ".json";

    return SUCCESS;
}

REGISTER_CUSTOM_OP("custom_greater_equal")                       // custom_greater_equal is the type name of the operator in the OM model. It can be specified randomly and cannot be the same as an existing type name.
.FrameworkType(TENSORFLOW)                                   // Enumerated type. The options are as follows: CAFFE, TENSORFLOW
.OriginOpType("GreaterEqual")                                // GreaterEqual indicates the type name of the operator in the caffe framework.
.ParseParamsFn(AutoMappingFn)                                // AutoMappingFn indicates automatic mapping the parameters of op.
.InferShapeAndTypeFn(TFcustomGreaterEqualInferShapeAndType)  // Set output description and datatype function
.TEBinBuildFn(TFcustomGreaterEqualBuildTeBin)                // Build Te op binary function
.ImplyType(ImplyType::TVM)                                   // Implementation type. Enumerated type, The options are as follows: TVM, AI_CPU.
.Formats({ DOMI_TENSOR_ND }, { DOMI_TENSOR_ND });            // #### Format of the input and output

}  // namespace domi
