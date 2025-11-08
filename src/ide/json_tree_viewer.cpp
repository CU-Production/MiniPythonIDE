#include "json_tree_viewer.h"
#include <sstream>

void JsonTreeViewer::RenderTree(int id, const std::string& name, const json& value, int variablesReference) {
    // Use tree version in ID to force reset on debug step
    int uniqueId = m_treeVersion * 100000 + id;
    
    ImGui::PushID(uniqueId);
    
    if (value.is_object()) {
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

