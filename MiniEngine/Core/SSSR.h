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

#pragma once

#include "EngineTuning.h"

namespace Math { class Camera; }
class CommandContext;

namespace SSSR
{
    // Temporal antialiasing involves jittering sample positions and accumulating color over time to 
    // effectively supersample the image.
    extern BoolVar EnableSSSR;

    void Initialize( void );

    void Shutdown( void );

    // Call once per frame to increment the internal frame counter and, in the case of TAA, choosing the next
    // jittered sample position.
    //void Update( uint64_t FrameIndex );

    // Returns whether the frame is odd or even, relevant to checkerboard rendering.
    //uint32_t GetFrameIndexMod2( void );

    //void ClearHistory(CommandContext& Context);

    void Render(CommandContext& Context, const Math::Camera& camera);

    void DebugOverlay(CommandContext& BaseContext);
}
