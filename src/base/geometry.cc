//
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by rogersxiao on 2020/6/1.
//

#include "base/geometry.h"
#include <cmath>
#include "base/property.h"

namespace tdf {

bool Offset::IsFinite() const { return std::isfinite(x_) && std::isfinite(y_); }

bool Offset::operator<(const Offset &base) const {
  return this->x_ < base.x_ && this->y_ < base.y_;
}

bool Offset::operator<=(const Offset &base) const {
  return this->x_ <= base.x_ && this->y_ <= base.y_;
}

bool Offset::operator>(const Offset &base) const {
  return this->x_ > base.x_ && this->y_ > base.y_;
}

bool Offset::operator>=(const Offset &base) const {
  return this->x_ >= base.x_ && this->y_ >= base.y_;
}

bool Offset::operator==(const Offset &base) const {
  return this->x_ == base.x_ && this->y_ == base.y_;
}

Offset Offset::operator-() const { return Offset(-x_, -y_); }

Offset Offset::operator-(const Offset &other) const { return Offset(x_ - other.x_, y_ - other.y_); }

Offset Offset::operator+(const Offset &other) const { return Offset(x_ + other.x_, y_ + other.y_); }

Offset Offset::operator*(double operand) const { return Offset(x_ * operand, y_ * operand); }

Offset Offset::operator/(double operand) const {
  assert(operand != 0);
  return Offset(x_ / operand, y_ / operand);
}

Offset::Offset(double x, double y) : x_(x), y_(y) {
}
double Offset::Distance() const { return sqrt(x_ * x_ + y_ * y_); }

double Offset::DistanceSquared() const { return (x_ * x_ + y_ * y_); }

double Offset::Direction() const { return atan2(y_, x_); }

Offset Offset::Scale(double scale_x, double scale_y) const {
  assert(scale_x > 0);
  assert(scale_y > 0);
  return Offset(x_ * scale_x, y_ * scale_y);
}

Offset Offset::Translate(double translate_x, double translate_y) const {
  return Offset(x_ + translate_x, y_ + translate_y);
}

Offset Offset::Lerp(const Offset &a, const Offset &b, double t) {
  return Offset(a.x_ + t * (b.x_ - a.x_), a.y_ + t * (b.y_ - a.y_));
}

}  // namespace tdf
