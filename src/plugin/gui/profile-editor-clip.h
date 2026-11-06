// SPDX-License-Identifier: LGPL-2.1-only
#ifndef HCBRAVO_PROFILE_EDITOR_CLIP_H
#define HCBRAVO_PROFILE_EDITOR_CLIP_H

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

// Project local ImGui coordinates through X-Plane's actual GL matrices. This
// also accounts for UI scaling, Retina framebuffers and nonzero viewports.
inline std::array<int, 4> editor_scissor(const std::array<float, 4> & clip,
    const float * model, const float * projection, const int * viewport,
    const int * outer_clip) {
    float min_x = std::numeric_limits<float>::max(), min_y = min_x;
    float max_x = -min_x, max_y = -min_y;
    for(float x : {clip[0], clip[2]}) {
        for(float y : {clip[1], clip[3]}) {
            float eye[4], ndc[4];
            for(int i = 0; i < 4; ++i) eye[i] = model[i] * x + model[4+i] * y + model[12+i];
            for(int i = 0; i < 4; ++i) {
                ndc[i] = 0;
                for(int j = 0; j < 4; ++j) ndc[i] += projection[4*j+i] * eye[j];
            }
            if(ndc[3] == 0) return {0, 0, 0, 0};
            const float px = viewport[0] + (ndc[0] / ndc[3] + 1) * viewport[2] * 0.5f;
            const float py = viewport[1] + (ndc[1] / ndc[3] + 1) * viewport[3] * 0.5f;
            if(!std::isfinite(px) || !std::isfinite(py)) return {0, 0, 0, 0};
            min_x = (std::min)(min_x, px); max_x = (std::max)(max_x, px);
            min_y = (std::min)(min_y, py); max_y = (std::max)(max_y, py);
        }
    }
    const int left = static_cast<int>(std::clamp(std::floor(min_x),
        static_cast<float>(outer_clip[0]), static_cast<float>(outer_clip[0] + outer_clip[2])));
    const int bottom = static_cast<int>(std::clamp(std::floor(min_y),
        static_cast<float>(outer_clip[1]), static_cast<float>(outer_clip[1] + outer_clip[3])));
    const int right = static_cast<int>(std::clamp(std::ceil(max_x),
        static_cast<float>(outer_clip[0]), static_cast<float>(outer_clip[0] + outer_clip[2])));
    const int top = static_cast<int>(std::clamp(std::ceil(max_y),
        static_cast<float>(outer_clip[1]), static_cast<float>(outer_clip[1] + outer_clip[3])));
    return {left, bottom, (std::max)(0, right - left), (std::max)(0, top - bottom)};
}

#endif
