//
// Created by willisdai on 2021/3/9.
//
#pragma once
#include <string>
namespace tdf {
class BaseObject {
 public:
  virtual std::string ToString() const = 0;
};
}