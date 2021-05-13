/**
 * Copyright (c) 2020 Tencent Corporation. All rights reserved.
 * @brief 动态化框架反射实现
 * @details 主要基于静态反射和少量动态反射，对性能无影响
 */

#pragma once

#include <iostream>
#include <sstream>
#include "reflection.h"

#define TDF_REFF_CLASS_META(class_name)                        \
 public:                                                       \
  friend class ::refl_impl::metadata::type_info__<class_name>; \
  TDF_REFLECTABLE


#define TDF_REFL_DEFINE(...) REFL_TYPE(__VA_ARGS__)
#define TDF_REFL_MEMBER(...) REFL_FIELD(__VA_ARGS__)
#define TDF_REFL_END(class_name)                                                \
  REFL_END                                                                      \

/**
 * @brief 需要在有运行时反射的类中增加这个宏，来自动生成信息
 */
#define TDF_REFLECTABLE                                                          \
  virtual const TypeInfo& GetTypeInfo() const override {                         \
    return TypeInfo::Get<::refl::trait::remove_qualifiers_t<decltype(*this)>>(); \
  }                                                                              \
  virtual std::shared_ptr<base::Diagnostics> ToDiagnostics() const override {    \
    auto d = ForeachToDiagnostics(this);                                         \
    return d;                                                                    \
  }
