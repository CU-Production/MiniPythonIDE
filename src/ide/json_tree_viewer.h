#pragma once

#include "../../3rd_party/nlohmann/json.hpp"
#include "../../3rd_party/imgui/imgui.h"
#include <functional>
#include <string>

using json = nlohmann::json;

/**
 * ImGui JSON Tree Viewer
 * 
 * A simple, lightweight JSON tree renderer for ImGui that supports:
 * - Expandable tree structure
 * - Lazy loading of child nodes
 * - Tooltips for long values
 * - Custom styling
 */
class JsonTreeViewer {
public:
    // Callback for lazy loading children
    // Called when user expands a node that hasn't been loaded yet
    // Parameters: (variablesReference) -> json object with children
    using LazyLoadCallback = std::function<json(int variablesReference)>;
    
    JsonTreeViewer() : m_lazyLoadCallback(nullptr), m_treeVersion(0) {}
    
    // Set lazy load callback for fetching children on-demand
    void SetLazyLoadCallback(LazyLoadCallback callback) {
        m_lazyLoadCallback = callback;
    }
    
    // Set tree version to reset all expanded states (call after each debug step)
    void SetTreeVersion(int version) {
        m_treeVersion = version;
    }
    
    // Render a JSON value as an ImGui tree
    // id: unique ID for this node (use variablesReference or generate one)
    // name: display name (variable name)
    // value: JSON value to render
    // variablesReference: for lazy loading (0 = no children or already loaded)
    void RenderTree(int id, const std::string& name, const json& value, int variablesReference = 0);
    
private:
    void RenderValue(const std::string& name, const json& value);
    void RenderDAPVariable(int id, const json& dapVar);
    void RenderObject(int id, const std::string& name, const json& obj, int variablesReference);
    void RenderArray(int id, const std::string& name, const json& arr, int variablesReference);
    void RenderPrimitive(const std::string& name, const json& value);
    
    std::string GetTypeString(const json& value);
    ImVec4 GetTypeColor(const json& value);
    std::string TruncateString(const std::string& str, size_t maxLen = 80);
    
    LazyLoadCallback m_lazyLoadCallback;
    int m_treeVersion;
};

