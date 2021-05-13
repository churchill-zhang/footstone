//
// Created by dexterzhao on 2020/7/27.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#include "base/diagnosticable.h"
#include <sstream>

namespace tdf {
namespace base {

std::shared_ptr<Diagnostics> Diagnostics::Make(std::string name, int64_t hash) {
  return TDF_MAKE_SHARED(Diagnostics, name, hash);
}

Diagnostics::Diagnostics(std::string name, int64_t hash) : name_(name), hash_(hash) {}

std::string Diagnostics::Name() const { return name_; }

int64_t Diagnostics::Hash() const { return hash_; }

const std::map<std::string, std::shared_ptr<Property>>& Diagnostics::Properties() {
  return properties_;
}

void Diagnostics::AddProperty(std::shared_ptr<Property> prop) {
  if (prop->Name().size() > 0) {
    properties_[prop->Name()] = prop;
  }
}

std::string Diagnostics::ToString() const {
  std::stringstream ss = std::stringstream("Diagnostics");
  ss << " {\r"
     << "name: " << name_;
  ss << ",\r"
     << "properties"
     << ": [\r";
  int i = 1;
  for (auto const& pair : properties_) {
    ss << "\t(" << i++ << ") => {\r\t\t" << pair.second->ToString() << "\r\t}\r";
  }
  ss << "]\r}";
  return ss.str();
}

std::string Diagnostics::ToJSONString() {
  std::stringstream stream;
  stream << '{';
  stream << R"("_type": ")" << name_ << R"(",)";
  auto count = 0;

  for (auto& it : properties_) {
    ++count;
    auto p = it.second;
    stream << '"' << p->Name() << R"(":)";
    if (p->Type() == "string" || p->Type() == "std::string") {
      auto valueString = p->ValueToString();
      if (valueString.find('{') == std::string::npos &&
          valueString.find('}') == std::string::npos &&
          valueString.find('[') == std::string::npos &&
          valueString.find(']') == std::string::npos) {
        stream << R"(")" << valueString << R"(")";
      } else {
        stream << valueString;
      }
    } else {
      stream << p->ValueToString();
    }
    if (count < properties_.size()) {
      stream << ',';
    }
  }
  stream << "}";
  return stream.str();
}

std::string GetTypeNameFromFunctionName(const std::string& pretty_func, const std::string& func) {
  // 通过比对__PRETTY_FUNCTION__与__FUNCTION__，找到函数名前面的类名
  size_t end = pretty_func.find(func + "(");
  if (end == std::string::npos) {
    return "";
  }
  size_t begin = pretty_func.rfind(" ", end);
  if (begin == std::string::npos) {
    return "";
  }
  begin = begin + 1;
  if (end <= begin) {
    return "";
  }
  auto name = pretty_func.substr(begin, (end - begin));
  // 去掉name结尾的域作用符
  size_t pos = name.rfind("::");
  return (pos == (name.length() - 2)) ? name.substr(0, name.length() - 2) : name;
}

std::string GetBriefTypeNameFromFunctionName(const std::string& pretty_func,
                                             const std::string& func) {
  // 获取类的全名，去掉全名中的namespace，即为精简类名
  auto name = GetTypeNameFromFunctionName(pretty_func, func);
  size_t pos = name.rfind("::");
  return (pos == std::string::npos) ? name : name.substr(pos + 2, name.length() - pos - 2);
}

}  // namespace base
}  // namespace tdf
