//
// Created by dexterzhao on 2020/7/28.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#pragma once
#include <cxxabi.h>
#include <memory>
#include <sstream>
#include <string>
#include "base_object.h"

#define TYPE_PROPERTY_INSTANTIATION(TYPE)                                    \
    public:                                                                  \
    constexpr const TYPE &Value() const { return value_; }                   \
    TypeProperty(const std::string &name, const TYPE &value) {               \
      name_ = name;                                                          \
      value_ = value;                                                        \
      int status;                                                            \
      char *type = abi::__cxa_demangle(typeid(TYPE).name(), 0, 0, &status);  \
      type_ = *type;                                                         \
      free(type);                                                            \
    }                                                                        \
    virtual std::string ToString() const override {                          \
    return name_ + " " + type_ + ": " + ValueToString();                     \
    }                                                                        \
    protected:                                                               \
    TYPE value_;                                                             \


namespace tdf {

template<typename T>
class TypeProperty;

class Property : public BaseObject {
 public:
  constexpr const std::string &Name() const { return name_; }

  constexpr const std::string &Type() const { return type_; }

  template<class T>
  static std::shared_ptr<Property> Make(const std::string &name, const T &value) {
    auto property = std::make_shared<TypeProperty<T>>(name, value);
    return property;
  }

  virtual ~Property() = default;

  virtual std::string ValueToString() const = 0;

 protected:
  std::string name_;

  std::string type_;
};

template<typename T>
class TypeProperty : public Property {
 public:
  constexpr const T &Value() const { return value_; }

  TypeProperty(const std::string &name, const T &value) {
    name_ = name;
    value_ = value;
    int status;
    char *type = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
    type_ = *type;
    free(type);
  }

  virtual std::string ValueToString() const override {
    std::stringstream ss;
    ss << value_;
    return ss.str();
  }

  virtual std::string ToString() const override {
    return name_ + " " + type_ + ": " + ValueToString();
  }

 protected:
  T value_;
};
}  // namespace tdf
