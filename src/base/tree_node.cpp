// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by dexterzhao on 2020/7/31.

#include "base/tree_node.h"
#include "base/macros.h"

namespace tdf {
namespace core {
std::string TreeNode::ToString() const {
  return "";
}

void TreeNode::AdoptChild(std::shared_ptr<TreeNode> child) {
  assert(child != nullptr);
  assert(child.get() != this);
  assert(child->parent_.expired());
  assert_fn([=]() {
    // 检测是否有循环
    auto node = this;
    auto parent = node->parent_.lock();
    while (parent != nullptr) {
      if (parent == child) {
        return false;
      }
      node = parent.get();
      parent = node->parent_.lock();
    }
    return true;
  });
  child->parent_ = shared_from_this();
  RedepthChild(child);
}

void TreeNode::DropChild(std::shared_ptr<TreeNode> node) {
  assert(node != nullptr);
  assert_fn([&]() {
    auto parent = node->parent_.lock();
    return (parent == nullptr) || (parent.get() == this);
  });
  node->parent_.reset();
}

void TreeNode::RedepthChild(std::shared_ptr<TreeNode> child) {
  if (child->depth_ <= depth_) {
    child->depth_ = depth_ + 1;
    child->RedepthChildren();
  }
}
}  // namespace core
}  // namespace tdf
