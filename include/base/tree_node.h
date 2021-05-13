// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by dexterzhao on 2020/7/27.
#pragma once

#include <functional>
#include <memory>
#include "base/macros.h"
#include "base/base_object.h"
#include "reflection.h"

namespace tdf {
namespace core {
class TreeNode : public BaseObject, public std::enable_shared_from_this<TreeNode> {
 public:
  virtual ~TreeNode() = default;

  constexpr int Depth() const { return depth_; }

  virtual void AdoptChild(std::shared_ptr<TreeNode> node);

  virtual void DropChild(std::shared_ptr<TreeNode> node);

  virtual void RedepthChild(std::shared_ptr<TreeNode> child);

  virtual void RedepthChildren() {}

  std::string ToString() const override;

 protected:
  std::weak_ptr<TreeNode> parent_;

 private:
  int depth_ = 0;
};

template<class ContextType>
class AttachableNode : public TreeNode {
 public:
  virtual void Detach() {
    assert(!context_.expired());
    assert_fn([=]() {
      if (parent_.expired())
        return true;
      auto parent = std::dynamic_pointer_cast<AttachableNode<ContextType>>(parent_.lock());
      if (parent == nullptr)
        return false;
      return parent->GetContext() == nullptr;
    });
    context_.reset();
  }

  virtual void Attach(std::shared_ptr<ContextType> context) {
    assert(context != nullptr);
    assert(context_.expired());
    context_ = std::weak_ptr<ContextType>(context);
  }

  inline bool IsAttached() const { return !context_.expired(); }

  constexpr std::shared_ptr<ContextType> GetContext() const { return context_.lock(); }

  void AdoptChild(std::shared_ptr<TreeNode> child) override {
    assert(std::dynamic_pointer_cast<AttachableNode<ContextType>>(child) != nullptr);
    TreeNode::AdoptChild(child);
  }

 protected:
  std::weak_ptr<ContextType> context_;
};
}  // namespace core
}  // namespace tdf
