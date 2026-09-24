#pragma once

namespace geom {

struct Point2D {
    double x = 0;
    double y = 0;
};

struct Vec2D {
    Vec2D() = default;
    Vec2D(double x, double y)
        : x(x)
        , y(y) {
    }

    Vec2D& operator*=(double scale) {
        x *= scale;
        y *= scale;
        return *this;
    }

    double x = 0;
    double y = 0;
};

inline Vec2D operator*(Vec2D lhs, double rhs) {
    return lhs *= rhs;
}

inline Vec2D operator*(double lhs, Vec2D rhs) {
    return rhs *= lhs;
}

inline Vec2D operator+(Vec2D lhs, Vec2D rhs) {
    return Vec2D{lhs.x + rhs.x, lhs.y + rhs.y};
}

inline Point2D operator+(Point2D lhs, Vec2D rhs) {
    return Point2D{lhs.x + rhs.x, lhs.y + rhs.y};
}

}  // namespace geom
