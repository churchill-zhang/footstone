//
// Created by dexterzhao on 2020/7/31.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#pragma once
#include "diagnosticable.h"

namespace tdf {
namespace base {

// MARK: - RenderTreeExporter
template<typename T>
class TreeExporter {
 public:
  virtual std::string Export() = 0;

 protected:
  using MakeDiagnosticsNodeFunc = std::function<std::shared_ptr<DiagnosticsNode < T>>(
      std::shared_ptr<DiagnosticableTarget> sourceObject)>;

  std::shared_ptr<DiagnosticableTarget> source_tree_;

  std::shared_ptr<DiagnosticsNode < T>> diagnostics_tree_;

  explicit TreeExporter(std::shared_ptr<DiagnosticableTarget> &root) : source_tree_(root) {}

  std::shared_ptr<DiagnosticsNode < T>> BuildDiagnosticsTree(
  std::shared_ptr<DiagnosticableTarget> sourceTree, MakeDiagnosticsNodeFunc
  makeNode);

  void ConvertDiagnosticsTreeToListByDepth(
      std::shared_ptr<DiagnosticsNode < T>>
  diagnosticsTree,
  std::vector<std::shared_ptr<DiagnosticsNode < T>>>& targetList);
};

// MARK: - JSON

class TreeJSONExporter : public TreeExporter<std::string> {
 public:
  using JsonDiagnosticsNode = DiagnosticsNode<std::string>;
  virtual ~TreeJSONExporter() = default;

  static std::shared_ptr<TreeJSONExporter> Make(std::shared_ptr<DiagnosticableTarget> root);

  std::string Export() override;

 protected:
  explicit TreeJSONExporter(std::shared_ptr<DiagnosticableTarget> &root);

 private:
  void MakeJsonTree(std::shared_ptr<JsonDiagnosticsNode> root);
};

// MARK: - Mermaid

class TreeMermaidExporter : public TreeExporter<std::string> {
 public:
  class Style {
   public:
    bool verbose;
    bool separatedByDepth;
    Style() : verbose(false), separatedByDepth(true) {}
  };
  virtual ~TreeMermaidExporter() = default;

  static std::shared_ptr<TreeMermaidExporter> Make(std::shared_ptr<DiagnosticableTarget> root,
                                                   const Style &style = Style());

  std::string Export() override;

 private:
  Style style_;

  TreeMermaidExporter(std::shared_ptr<DiagnosticableTarget> &root, const Style &style);

  std::string MakeBriefScript(
      const std::vector<std::shared_ptr<DiagnosticsNode < std::string>>
  >& nodes);

  std::string MakeDetailScript(
      const std::vector<std::shared_ptr<DiagnosticsNode < std::string>>
  >& nodes);

  std::string IntToHexString(int64_t i);

  void ReplaceString(std::string &str, const std::string &src, const std::string &dest);
};

}  // namespace base
}  // namespace tdf
