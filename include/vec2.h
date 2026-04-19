#ifndef VEC2_H
#define VEC2_H

/*
 * Header-only 2D vector helpers used by movement, steering, and world wrapping.
 */

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    double x;
    double y;
} Vec2;

static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    Vec2 r = {a.x + b.x, a.y + b.y};
    return r;
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    Vec2 r = {a.x - b.x, a.y - b.y};
    return r;
}

static inline Vec2 vec2_scale(Vec2 v, double s) {
    Vec2 r = {v.x * s, v.y * s};
    return r;
}

static inline double vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline double vec2_len_sq(Vec2 v) {
    return vec2_dot(v, v);
}

static inline double vec2_len(Vec2 v) {
    return sqrt(vec2_len_sq(v));
}

static inline Vec2 vec2_normalize(Vec2 v) {
    double len = vec2_len(v);
    if (len < 1e-9) {
        Vec2 z = {0.0, 0.0};
        return z;
    }
    return vec2_scale(v, 1.0 / len);
}

static inline Vec2 vec2_limit(Vec2 v, double max) {
    double len_sq = vec2_len_sq(v);
    double max_sq = max * max;
    if (len_sq > max_sq && len_sq > 1e-12) {
        double scale = max / sqrt(len_sq);
        return vec2_scale(v, scale);
    }
    return v;
}

static inline Vec2 vec2_wrap(Vec2 v, double w, double h) {
    while (v.x < 0.0) {
        v.x += w;
    }
    while (v.y < 0.0) {
        v.y += h;
    }
    while (v.x >= w) {
        v.x -= w;
    }
    while (v.y >= h) {
        v.y -= h;
    }
    return v;
}

static inline double vec2_dist_sq(Vec2 a, Vec2 b) {
    return vec2_len_sq(vec2_sub(b, a));
}

static inline Vec2 vec2_delta_wrap(Vec2 from, Vec2 to, double w, double h) {
    if (w <= 0.0 || h <= 0.0) {
        return vec2_sub(to, from);
    }

    from = vec2_wrap(from, w, h);
    to = vec2_wrap(to, w, h);

    Vec2 d = vec2_sub(to, from);
    if (d.x > w * 0.5) {
        d.x -= w;
    } else if (d.x < -w * 0.5) {
        d.x += w;
    }
    if (d.y > h * 0.5) {
        d.y -= h;
    } else if (d.y < -h * 0.5) {
        d.y += h;
    }
    return d;
}

static inline double vec2_dist_sq_wrap(Vec2 a, Vec2 b, double w, double h) {
    Vec2 d = vec2_delta_wrap(a, b, w, h);
    return vec2_len_sq(d);
}

static inline Vec2 vec2_random_unit(void) {
    double a = ((double) rand() / (double) RAND_MAX) * (2.0 * M_PI);
    Vec2 r = {cos(a), sin(a)};
    return r;
}

static inline double vec2_angle(Vec2 v) {
    return atan2(v.y, v.x);
}

#endif
