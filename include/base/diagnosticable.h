//
// Created by dexterzhao on 2020/7/27.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#pragma once
#include <functional>
#include <map>
#include <string>
#include "base_object.h"
#include "memory/heap.h"
#include "property.h"
#include "diagnosticable_reflection.h"

namespace tdf {
namespace base {
class Diagnostics;

#define HASH_FROM_THIS reinterpret_cast<int64_t>(this)

// 此宏将尝试获取调用者的类名（以下为不启用RTTI时的实现）
#define CURRENT_CLASS_NAME \
  tdf::base::GetBriefTypeNameFromFunctionName(__PRETTY_FUNCTION__, __FUNCTION__)

std::string GetTypeNameFromFunctionName(const std::string &pretty_func, const std::string &func);
std::string GetBriefTypeNameFromFunctionName(const std::string &pretty_func,
                                             const std::string &func);

class DiagnosticableTarget {
 public:
  virtual void VisitDiagnosticableChildren(
      std::function<void(std::shared_ptr<DiagnosticableTarget>)> visitor) {}

  virtual std::shared_ptr<base::Diagnostics> ToDiagnostics() const = 0;

  virtual ~DiagnosticableTarget() = default;
};

class DiagnosticableTreeNode : public DiagnosticableTarget, Reflectable {
 public:
  virtual ~DiagnosticableTreeNode() = default;
};

class Diagnostics : public BaseObject {
 public:
  static std::shared_ptr<Diagnostics> Make(std::string name, int64_t hash = 0);

  virtual ~Diagnostics() = default;

  std::string Name() const;

  int64_t Hash() const;

  const std::map<std::string, std::shared_ptr<Property>> &Properties();

  void AddProperty(std::shared_ptr<Property> prop);

  std::string ToString() const override;
  std::string ToJSONString();

 protected:
  std::string name_;

  int64_t hash_;

  std::map<std::string, std::shared_ptr<Property>> properties_;

  explicit Diagnostics(std::string name, int64_t hash = 0);

  FRIEND_OF_TDF_ALLOC;
};

template<typename T>
class DiagnosticsNode : public std::enable_shared_from_this<DiagnosticsNode<T>> {
 public:
  static std::shared_ptr<DiagnosticsNode<T>> Make(std::shared_ptr<Diagnostics> diagnostics) {
    return TDF_MAKE_SHARED(DiagnosticsNode < T >, diagnostics);
  }

  ~DiagnosticsNode() = default;

  const std::shared_ptr<Diagnostics> &diagnostics() { return diagnostics_; }

  std::string identifier;

  T data;

  int64_t Depth() const { return depth_; }

  void SetDepth(int64_t depth) { depth_ = depth; }

  void AddChild(std::shared_ptr<DiagnosticsNode> node) {
    if (node == nullptr) {
      return;
    }

    node->SetDepth(Depth() + 1);
    if (firstChild_ == nullptr) {
      firstChild_ = node;
    } else {
      auto child = firstChild_;
      while (child->nextSibling_ != nullptr) {
        child = child->nextSibling_;
      }
      child->nextSibling_ = node;
    }

    node->parent_ = this->shared_from_this();
  }

  std::vector<std::shared_ptr<DiagnosticsNode>> Children() {
    std::vector<std::shared_ptr<DiagnosticsNode>> nodes;
    if (firstChild_ != nullptr) {
      nodes.push_back(firstChild_);
      auto child = firstChild_;
      while (child->nextSibling_ != nullptr) {
        child = child->nextSibling_;
        nodes.push_back(child);
      }
    }
    return nodes;
  }

  void VisitChildren(std::function<void(std::shared_ptr<DiagnosticsNode>)> visitor) {
    if ((visitor != nullptr) && (firstChild_ != nullptr)) {
      visitor(firstChild_);
      auto child = firstChild_;
      while (child->nextSibling_ != nullptr) {
        child = child->nextSibling_;
        visitor(child);
      }
    }
  }

  std::shared_ptr<DiagnosticsNode> Parent() { return parent_; }

 protected:
  int64_t depth_ = 0;

  std::shared_ptr<Diagnostics> diagnostics_;

  std::shared_ptr<DiagnosticsNode> firstChild_;

  std::shared_ptr<DiagnosticsNode> nextSibling_;

  std::shared_ptr<DiagnosticsNode> parent_;

  explicit DiagnosticsNode(std::shared_ptr<Diagnostics> &diagnostics) : diagnostics_(diagnostics) {}

  FRIEND_OF_TDF_ALLOC;
};

template<class T>
std::shared_ptr<tdf::base::Diagnostics> ForeachToDiagnostics(T *obj) {
  auto type_info = obj->GetTypeInfo();
  auto d = tdf::base::Diagnostics::Make(type_info.GetName(), reinterpret_cast<int64_t>(obj));
  refl::util::for_each(refl::reflect(*obj).members, [&](auto member) {
    d->AddProperty(Property::Make(get_display_name(member), member(*obj)));
  });
  return d;
}
}  // namespace base
}  // namespace tdf
