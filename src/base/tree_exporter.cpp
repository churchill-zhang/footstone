//
// Created by dexterzhao on 2020/7/31.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#include "base/tree_exporter.h"
#include <inttypes.h>
#include <stdio.h>
#include <deque>
#include <sstream>
#include <vector>

namespace tdf {
namespace base {

template <typename T>
std::shared_ptr<DiagnosticsNode<T>> TreeExporter<T>::BuildDiagnosticsTree(
    std::shared_ptr<DiagnosticableTarget> sourceTree, MakeDiagnosticsNodeFunc makeNode) {
  if (sourceTree == nullptr) {
    return nullptr;
  }

  auto diagnosticsTree = makeNode(sourceTree);
  std::deque<std::shared_ptr<DiagnosticsNode<T>>> dQueue;
  dQueue.push_back(diagnosticsTree);

  std::deque<std::shared_ptr<DiagnosticableTarget>> sQueue;
  sQueue.push_back(sourceTree);

  while (sQueue.size() > 0) {
    auto source_object = sQueue.front();
    auto node = dQueue.front();
    sQueue.pop_front();
    dQueue.pop_front();
    source_object->VisitDiagnosticableChildren([&](std::shared_ptr<DiagnosticableTarget> child) {
      auto subNode = makeNode(child);
      node->AddChild(subNode);
      dQueue.push_back(subNode);
      sQueue.push_back(child);
    });
  }

  return diagnosticsTree;
}

template <typename T>
void TreeExporter<T>::ConvertDiagnosticsTreeToListByDepth(
    std::shared_ptr<DiagnosticsNode<T>> diagnosticsTree,
    std::vector<std::shared_ptr<DiagnosticsNode<T>>>& targetList) {
  if (diagnosticsTree == nullptr) {
    return;
  }

  int64_t cursor = 0;
  targetList.push_back(diagnosticsTree);
  while (cursor < targetList.size()) {
    auto node = targetList[cursor++];
    node->VisitChildren(
        [&](std::shared_ptr<DiagnosticsNode<T>> child) { targetList.push_back(child); });
  }

  sort(targetList.begin(), targetList.end(),
       [&](auto const& n1, auto const& n2) { return n1->Depth() < n2->Depth(); });
}

// MARK: - JSON
std::shared_ptr<TreeJSONExporter> TreeJSONExporter::Make(
    std::shared_ptr<DiagnosticableTarget> root) {
  return std::shared_ptr<TreeJSONExporter>(new TreeJSONExporter(root));
}

TreeJSONExporter::TreeJSONExporter(std::shared_ptr<DiagnosticableTarget>& root)
    : TreeExporter(root) {}

static inline std::string MakePropertyJsonString(std::shared_ptr<Property> p) {
  std::stringstream ss;
  ss << '"' << p->Name() << R"(":)";
  if (p->Type() == "string" || p->Type() == "std::string") {
    auto valueString = p->ValueToString();
    if (valueString.find('{') == std::string::npos && valueString.find('}') == std::string::npos &&
        valueString.find('[') == std::string::npos && valueString.find(']') == std::string::npos) {
      ss << R"(")" << valueString << R"(")";
    } else {
      ss << valueString;
    }
  } else {
    ss << p->ValueToString();
  }
  return ss.str();
}

void TreeJSONExporter::MakeJsonTree(std::shared_ptr<JsonDiagnosticsNode> root) {
  root->VisitChildren([&](std::shared_ptr<JsonDiagnosticsNode> child) { MakeJsonTree(child); });

  std::stringstream stream;

  // begin
  stream << '{';

  // type
  stream << R"("_type": ")" << root->diagnostics()->Name() << R"(",)";

  // attributes
  stream << R"("attributes": {)";
  auto& p = root->diagnostics()->Properties();
  auto count = 0;

  for (auto& it : p) {
    ++count;
    stream << MakePropertyJsonString(it.second);
    if (count < p.size()) {
      stream << ',';
    }
  }
  stream << "}";

  // children
  if (!root->Children().empty()) {
    stream << ",";
    if (root->Children().size() == 1) {
      stream << R"("child": )" << root->Children()[0]->identifier;
    } else {
      stream << R"("children": [)";
      count = 0;
      for (auto& child : root->Children()) {
        ++count;
        stream << child->identifier;
        if (count < root->Children().size()) {
          stream << ',';
        }
      }
      stream << "]";
    }
  }

  // end
  stream << "}";

  root->identifier = stream.str();
}

std::string TreeJSONExporter::Export() {
  diagnostics_tree_ =
      BuildDiagnosticsTree(source_tree_, [&](std::shared_ptr<DiagnosticableTarget> Diagnosticable) {
        auto diagnostics = Diagnosticable->ToDiagnostics();
        auto node = JsonDiagnosticsNode::Make(diagnostics);
        return node;
      });

  if (diagnostics_tree_ == nullptr) {
    return "";
  }

  MakeJsonTree(diagnostics_tree_);
  return diagnostics_tree_->identifier;
}

// MARK: - Mermaid

std::shared_ptr<TreeMermaidExporter> TreeMermaidExporter::Make(
    std::shared_ptr<DiagnosticableTarget> root, const Style& style) {
  return std::shared_ptr<TreeMermaidExporter>(new TreeMermaidExporter(root, style));
}

TreeMermaidExporter::TreeMermaidExporter(std::shared_ptr<DiagnosticableTarget>& root,
                                         const Style& style)
    : TreeExporter(root), style_(style) {}

std::string TreeMermaidExporter::Export() {
  diagnostics_tree_ =
      BuildDiagnosticsTree(source_tree_, [&](std::shared_ptr<DiagnosticableTarget> Diagnosticable) {
        auto diagnostics = Diagnosticable->ToDiagnostics();
        return DiagnosticsNode<std::string>::Make(diagnostics);
      });

  std::vector<std::shared_ptr<DiagnosticsNode<std::string>>> allNodes;
  ConvertDiagnosticsTreeToListByDepth(diagnostics_tree_, allNodes);
  return style_.verbose ? MakeDetailScript(allNodes) : MakeBriefScript(allNodes);
}

std::string TreeMermaidExporter::MakeBriefScript(
    const std::vector<std::shared_ptr<DiagnosticsNode<std::string>>>& nodes) {
  std::stringstream script;
  script << "graph LR\n";

  int64_t depth = -1;
  int64_t nodeIndex = 0;
  for (auto const& node : nodes) {
    if (style_.separatedByDepth) {
      if (node->Depth() > depth) {
        depth = node->Depth();
        if (depth > 0) {
          script << "end\n";
        }
        script << "subgraph Depth: " << depth << "\n";
      }
    }
    node->identifier = std::to_string(nodeIndex++);
    script << node->identifier << "([" << node->diagnostics()->Name() << ": "
           << IntToHexString(node->diagnostics()->Hash()) << "])\n";
  }

  if (style_.separatedByDepth && (depth > 0)) {
    script << "end\n";
  }

  for (auto const& node : nodes) {
    node->VisitChildren([&](std::shared_ptr<DiagnosticsNode<std::string>> child) {
      script << node->identifier << " --> " << child->identifier << "\n";
    });
  }

  return script.str();
}

std::string TreeMermaidExporter::MakeDetailScript(
    const std::vector<std::shared_ptr<DiagnosticsNode<std::string>>>& nodes) {
  std::stringstream script;
  script << "classDiagram\n";

  for (auto const& node : nodes) {
    node->identifier =
        node->diagnostics()->Name() + "__" + IntToHexString(node->diagnostics()->Hash());
    script << "class " << node->identifier << " {\n";
    for (auto const& pair : node->diagnostics()->Properties()) {
      std::string value = pair.second->ValueToString();
      ReplaceString(value, "{", "[");
      ReplaceString(value, "}", "]");
      script << pair.second->Name() << "(" << value << ")\n";
    }
    script << "}\n";
  }

  for (auto const& node : nodes) {
    node->VisitChildren([&](std::shared_ptr<DiagnosticsNode<std::string>> child) {
      script << node->identifier << " --> " << child->identifier << "\n";
    });
  }

  return script.str();
}

std::string TreeMermaidExporter::IntToHexString(int64_t i) {
  char c[34];
  snprintf(c, sizeof(c), "0x%" PRIx64, i);
  return std::string(c);
}

void TreeMermaidExporter::ReplaceString(std::string& str, const std::string& src,
                                        const std::string& dest) {
  std::string::size_type pos(0);
  while ((pos = str.find(src, pos)) != std::string::npos) {
    str.replace(pos, src.length(), dest);
  }
}

}  // namespace base
}  // namespace tdf
