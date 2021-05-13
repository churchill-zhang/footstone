/**
 * Copyright (c) 2020 Tencent Corporation. All rights reserved.
 * @brief 动态化框架反射实现
 * @details 主要基于静态反射和少量动态反射，对性能无影响
 */

#pragma once

#include <iostream>
#include <sstream>
#include "refl.hpp"

namespace tdf {

/**
 * @brief 运行时反射中类型描述信息
 */
class TypeInfo {
 public:
  /**
   * @brief 获取类型描述信息
   * @return 类型描述信息。
   */
  template<typename T>
  static const TypeInfo &Get() {
    static const TypeInfo ti(refl::reflect<T>());
    return ti;
  }

  /**
   * @brief 获取类型信息中的类名
   * @return 类名的字符串。
   */
  const std::string &GetName() const { return name_; }

 private:
  std::string name_;

  template<typename T, typename... Fields>
  explicit TypeInfo(refl::type_descriptor<T> td) : name_(td.name) {}
};

/**
 * @brief 运行时反射的接口
 */
class Reflectable {
 public:
  virtual const TypeInfo &GetTypeInfo() const = 0;
};

}  // namespace tdf
