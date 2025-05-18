#pragma once

namespace math
{
    struct vec2
    {
        float x, y;

        vec2(float __x, float __y) { x = __x; y = __y; };
    };

    struct vec3
    {
        float x, y, z;

        vec3(float __x, float __y, float __z) { x = __x; y = __y; z = __z; };
    };
}