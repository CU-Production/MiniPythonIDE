#include "json_tree_viewer.h"
#include <sstream>
#include <algorithm>

void JsonTreeViewer::RenderTree(int id, const std::string& name, const json& value, int variablesReference) {
    // Use tree version in ID to force reset on debug step
    int uniqueId = m_treeVersion * 100000 + id;
    
    ImGui::PushID(uniqueId);
    
    // Check if this is a DAP variable object (has name, value, type, variablesReference fields)
    bool isDAPVariable = value.is_object() && 
                         value.contains("name") && 
                         value.contains("value") && 
                         value.contains("variablesReference");
    
    if (isDAPVariable) {
        // Render DAP variable with lazy loading support
        RenderDAPVariable(uniqueId, value);
    }
    else if (value.is_object()) {
        RenderObject(uniqueId, name, value, variablesReference);
    }
    else if (value.is_array()) {
        RenderArray(uniqueId, name, value, variablesReference);
    }
    else {
        RenderPrimitive(name, value);
    }
    
    ImGui::PopID();
}

void JsonTreeViewer::RenderDAPVariable(int id, const json& dapVar) {
    // Extract DAP variable fields
    std::string name = dapVar.value("name", "<unnamed>");
    std::string value = dapVar.value("value", "");
    std::string type = dapVar.value("type", "");
    int varRef = dapVar.value("variablesReference", 0);
    bool expandable = dapVar.value("expandable", false);
    
    // Check if has children
    bool hasChildren = varRef > 0 || expandable;
    json children = dapVar.value("children", json::array());
    bool childrenLoaded = !children.empty();
    
    ImGui::AlignTextToFramePadding();
    
    if (hasChildren) {
        // Render as expandable tree node
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
        
        // Build label with type hint
        std::string label = name;
        if (!type.empty()) {
            label += " (" + type + ")";
        }
        
        bool isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
        
        // Show value summary on same line (truncated)
        if (!value.empty() && value.length() < 60) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", TruncateString(value, 40).c_str());
        }
        
        if (isOpen) {
            // Lazy load if needed
            if (!childrenLoaded && varRef > 0 && m_lazyLoadCallback) {
                ImGui::Indent();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Loading...");
                ImGui::Unindent();
                
                // Request lazy load
                m_lazyLoadCallback(varRef);
            }
            else if (childrenLoaded) {
                // Render children
                ImGui::Indent();
                
                // Check if this is a list/array type with many elements
                bool isList = (type == "list" || type == "tuple");
                size_t childCount = children.size();
                const size_t segmentSize = 100;
                
                if (isList && childCount > segmentSize) {
                    // Segmented display for long lists
                    size_t segmentCount = (childCount + segmentSize - 1) / segmentSize;
                    
                    for (size_t seg = 0; seg < segmentCount; seg++) {
                        size_t startIdx = seg * segmentSize;
                        size_t endIdx = std::min(startIdx + segmentSize, childCount);
                        
                        // Create segment label
                        std::string segmentLabel = "[" + std::to_string(startIdx) + 
                                                   " ... " + std::to_string(endIdx - 1) + 
                                                   "] (" + std::to_string(endIdx - startIdx) + " items)";
                        
                        ImGuiTreeNodeFlags segFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                        bool segmentOpen = ImGui::TreeNodeEx(segmentLabel.c_str(), segFlags);
                        
                        if (segmentOpen) {
                            // Render items in this segment
                            for (size_t i = startIdx; i < endIdx; i++) {
                                const auto& child = children[i];
                                int childVarRef = child.value("variablesReference", 0);
                                RenderTree(id * 1000 + (int)i, child.value("name", ""), child, childVarRef);
                            }
                            ImGui::TreePop();
                        }
                    }
                }
                else {
                    // Normal display for short lists or non-list types
                    for (size_t i = 0; i < children.size(); i++) {
                        const auto& child = children[i];
                        int childVarRef = child.value("variablesReference", 0);
                        RenderTree(id * 1000 + (int)i, child.value("name", ""), child, childVarRef);
                    }
                }
                
                ImGui::Unindent();
            }
            
            ImGui::TreePop();
        }
    }
    else {
        // Render as simple value
        ImGui::Text("%s:", name.c_str());
        ImGui::SameLine();
        
        // Determine color based on type
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (type == "int" || type == "float" || type == "number") {
            color = ImVec4(0.6f, 1.0f, 0.6f, 1.0f);  // Green
        }
        else if (type == "str" || type == "string") {
            color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);  // Orange
        }
        else if (type == "bool") {
            color = ImVec4(0.3f, 0.8f, 1.0f, 1.0f);  // Cyan
        }
        else if (type == "NoneType" || value == "None") {
            color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);  // Gray
        }
        
        std::string displayValue = TruncateString(value);
        ImGui::TextColored(color, "%s", displayValue.c_str());
        
        // Hover tooltip for full value if truncated
        if (displayValue != value && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        
        // Type info
        if (!type.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", type.c_str());
        }
    }
}

void JsonTreeViewer::RenderObject(int id, const std::string& name, const json& obj, int variablesReference) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    
    // Show type and count
    std::string label = name;
    if (variablesReference > 0 && obj.empty()) {
        // Has children but not loaded yet
        label += " {not loaded}";
    } else if (!obj.empty()) {
        label += " {" + std::to_string(obj.size()) + " items}";
    } else {
        label += " {}";
    }
    
    bool isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    
    // Type info
    ImGui::SameLine();
    ImGui::TextColored(GetTypeColor(obj), "object");
    
    if (isOpen) {
        // Lazy load if needed
        if (variablesReference > 0 && obj.empty() && m_lazyLoadCallback) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  Loading...");
            
            // Request lazy load
            json loadedChildren = m_lazyLoadCallback(variablesReference);
            
            // This will be rendered in next frame after children are updated
        }
        else {
            // Render children
            int childId = 0;
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                // Check if this child has variablesReference (for nested lazy loading)
                int childVarRef = 0;
                if (it.value().is_object() && it.value().contains("variablesReference")) {
                    childVarRef = it.value()["variablesReference"].get<int>();
                }
                
                RenderTree(id * 1000 + childId++, it.key(), it.value(), childVarRef);
            }
        }
        
        ImGui::TreePop();
    }
}

void JsonTreeViewer::RenderArray(int id, const std::string& name, const json& arr, int variablesReference) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
    
    // Show type and count
    std::string label = name;
    if (variablesReference > 0 && arr.empty()) {
        label += " [not loaded]";
    } else if (!arr.empty()) {
        label += " [" + std::to_string(arr.size()) + " items]";
    } else {
        label += " []";
    }
    
    bool isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
    
    // Type info
    ImGui::SameLine();
    ImGui::TextColored(GetTypeColor(arr), "array");
    
    if (isOpen) {
        // Lazy load if needed
        if (variablesReference > 0 && arr.empty() && m_lazyLoadCallback) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  Loading...");
            
            // Request lazy load
            json loadedChildren = m_lazyLoadCallback(variablesReference);
        }
        else {
            // Render array items
            for (size_t i = 0; i < arr.size(); ++i) {
                std::string indexName = "[" + std::to_string(i) + "]";
                
                // Check if this item has variablesReference
                int childVarRef = 0;
                if (arr[i].is_object() && arr[i].contains("variablesReference")) {
                    childVarRef = arr[i]["variablesReference"].get<int>();
                }
                
                RenderTree(id * 1000 + (int)i, indexName, arr[i], childVarRef);
            }
        }
        
        ImGui::TreePop();
    }
}

void JsonTreeViewer::RenderPrimitive(const std::string& name, const json& value) {
    ImGui::AlignTextToFramePadding();
    
    // Name
    ImGui::Text("%s:", name.c_str());
    ImGui::SameLine();
    
    // Value (possibly truncated)
    std::string valueStr;
    ImVec4 color = GetTypeColor(value);
    
    if (value.is_string()) {
        valueStr = "\"" + value.get<std::string>() + "\"";
    }
    else if (value.is_null()) {
        valueStr = "null";
    }
    else if (value.is_boolean()) {
        valueStr = value.get<bool>() ? "true" : "false";
    }
    else if (value.is_number()) {
        std::ostringstream oss;
        oss << value;
        valueStr = oss.str();
    }
    else {
        valueStr = value.dump();
    }
    
    std::string displayValue = TruncateString(valueStr);
    ImGui::TextColored(color, "%s", displayValue.c_str());
    
    // Hover tooltip for full value if truncated
    if (displayValue != valueStr && ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
        ImGui::TextUnformatted(valueStr.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    
    // Type info
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", GetTypeString(value).c_str());
}

std::string JsonTreeViewer::GetTypeString(const json& value) {
    if (value.is_null()) return "null";
    if (value.is_boolean()) return "bool";
    if (value.is_number_integer()) return "int";
    if (value.is_number_float()) return "float";
    if (value.is_string()) return "string";
    if (value.is_array()) return "array";
    if (value.is_object()) return "object";
    return "unknown";
}

ImVec4 JsonTreeViewer::GetTypeColor(const json& value) {
    if (value.is_null()) return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);      // Gray
    if (value.is_boolean()) return ImVec4(0.3f, 0.8f, 1.0f, 1.0f);   // Cyan
    if (value.is_number()) return ImVec4(0.6f, 1.0f, 0.6f, 1.0f);    // Green
    if (value.is_string()) return ImVec4(1.0f, 0.8f, 0.6f, 1.0f);    // Orange
    if (value.is_array()) return ImVec4(0.9f, 0.7f, 1.0f, 1.0f);     // Purple
    if (value.is_object()) return ImVec4(0.5f, 0.7f, 0.9f, 1.0f);    // Blue
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);                            // White
}

std::string JsonTreeViewer::TruncateString(const std::string& str, size_t maxLen) {
    if (str.length() <= maxLen) {
        return str;
    }
    return str.substr(0, maxLen) + "...";
}

