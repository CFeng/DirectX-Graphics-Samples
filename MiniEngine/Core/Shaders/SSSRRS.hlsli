//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#define SSSR_RootSig \
    "RootFlags(0), " \
    "RootConstants(b0, num32BitConstants = 4)," \
    "DescriptorTable(SRV(t0, numDescriptors = 10))," \
    "DescriptorTable(UAV(u0, numDescriptors = 10))," \
    "CBV(b1), " \
    "StaticSampler(s0," \
        "addressU = TEXTURE_ADDRESS_CLAMP," \
        "addressV = TEXTURE_ADDRESS_CLAMP," \
        "addressW = TEXTURE_ADDRESS_CLAMP," \
        "filter = FILTER_MIN_MAG_MIP_LINEAR)," \
    "StaticSampler(s1," \
        "addressU = TEXTURE_ADDRESS_BORDER," \
        "addressV = TEXTURE_ADDRESS_BORDER," \
        "addressW = TEXTURE_ADDRESS_BORDER," \
        "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK," \
        "filter = FILTER_MIN_MAG_MIP_POINT)," \
    "StaticSampler(s2," \
        "addressU = TEXTURE_ADDRESS_WRAP," \
        "addressV = TEXTURE_ADDRESS_WRAP," \
        "addressW = TEXTURE_ADDRESS_WRAP," \
        "filter = FILTER_ANISOTROPIC)"

