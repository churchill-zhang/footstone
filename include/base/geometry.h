//
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by rogersxiao on 2020/6/1.
//

#pragma once
#include "diagnosticable.h"

namespace tdf {
class Offset {
 public:
  Offset() = default;
  Offset(double x, double y);

  /**
   * x_ 和 y_ 是否都为Finite
   * @return x_ 和 y_都为finite返回true否则返回false
   */
  bool IsFinite() const;
  constexpr double GetX() const { return x_; }
  constexpr double GetY() const { return y_; }

  /**
   * offset a与b之间的线性差值变化
   * @param a
   * @param b
   * @param t 代表线性差值变化过程中时间线的位置，取值在 0-1之间。 0表示还在a位置，1表示已经到了b位置。
   * @return 返回差值变化过程中的 Offset
   */
  static Offset Lerp(const Offset &a, const Offset &b, double t);
  static Offset Zero() { return Offset(0, 0); }
  double Distance() const;
  double DistanceSquared() const;
  double Direction() const;
  Offset Scale(double scale_x, double scale_y) const;
  Offset Translate(double translate_x, double translate_y) const;

  bool operator<(const Offset &base) const;
  bool operator<=(const Offset &base) const;
  bool operator>(const Offset &base) const;
  bool operator>=(const Offset &base) const;
  bool operator==(const Offset &other) const;

  Offset operator-() const;
  Offset operator-(const Offset &other) const;
  Offset operator+(const Offset &other) const;
  Offset operator*(double operand) const;
  Offset operator/(double operand) const;

 protected:
  double x_;
  double y_;
};

}  // namespace tdf
