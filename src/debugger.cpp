#include "debugger.h"

#ifdef ENABLE_DEBUGGER

#ifndef DEBUGGER_VERBOSE_LOGGING
#define DEBUGGER_VERBOSE_LOGGING 0
#endif

#include <sstream>
#include <chrono>
#include <future>
#include <utility>

Debugger* Debugger::s_instance = nullptr;

// Static callback for VM 1 print function
static std::function<void(const std::string&)> s_vm1_logCallback;

Debugger::Debugger()
    : m_debugging(false)
    , m_paused(false)
    , m_currentLine(-1)
{
}

Debugger::~Debugger() {
    // Force stop debugging if still active
    if (m_debugging.load()) {
        m_debugging.store(false);
        
        // Wake up paused thread
        if (m_paused.load()) {
            m_paused.store(false);
            m_pauseCondition.notify_all();
        }
    }
    
    // Ensure thread is joined before destruction
    if (m_executionThread.joinable()) {
        m_executionThread.join();
    }
    
    s_instance = nullptr;
}

bool Debugger::Start(const std::string& code, const std::string& filename,
                     std::function<void(const std::string&)> logCallback) {
    if (m_debugging.load()) {
        return false; // Already debugging
    }
    
    // Ensure previous thread is cleaned up
    if (m_executionThread.joinable()) {
        m_executionThread.join();
    }

    // Clear previous debug state before starting new session
    m_currentFile.clear();
    m_currentLine = -1;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();

    m_logCallback = logCallback;
    m_debugging.store(true);
    m_paused.store(false);
    s_instance = this;
    
#if DEBUGGER_VERBOSE_LOGGING
    if (logCallback) {
        logCallback("[info] Starting debugger in background thread (VM 1)...\n");
    }
#endif
    
    // Start Python execution in background thread using VM 1
    // Main thread continues to use VM 0
    m_executionThread = std::thread(&Debugger::ExecuteInThread, this, code, filename);
    
    return true;
}

void Debugger::Stop() {
    if (!m_debugging.load()) {
        return;
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Stopping debugger...\n");
    }
    
    // Set flag to stop debugging
    m_debugging.store(false);
    
    // If currently paused, wake up the execution thread so it can exit
    if (m_paused.load()) {
        m_paused.store(false);
        m_pauseCondition.notify_all();
    }
    
    // Wait for execution thread to finish with timeout
    if (m_executionThread.joinable()) {
        // Use a future to implement timed join
        auto future = std::async(std::launch::async, [this]() {
            if (m_executionThread.joinable()) {
                m_executionThread.join();
            }
        });
        
        // Wait up to 3 seconds for thread to finish
        if (future.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
            if (m_logCallback) {
                m_logCallback("[warning] Thread did not exit within 3s, detaching...\n");
            }
            // Thread didn't finish in time, detach it to avoid blocking
            if (m_executionThread.joinable()) {
                m_executionThread.detach();
            }
        }
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Debug session ended\n");
    }
    
    // Clear debug state
    m_currentFile.clear();
    m_currentLine = -1;
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
}

void Debugger::AddBreakpoint(const std::string& filename, int line) {
    m_breakpoints[filename].insert(line);
    
    // Note: Breakpoints will be synced when execution thread starts
    // We can't call c11_debugger_setbreakpoint from main thread
}

void Debugger::RemoveBreakpoint(const std::string& filename, int line) {
    auto it = m_breakpoints.find(filename);
    if (it != m_breakpoints.end()) {
        it->second.erase(line);
        
        // Note: Can't modify debugger from main thread
        // Breakpoint changes only take effect for next debug session
    }
}

void Debugger::ClearBreakpoints() {
    m_breakpoints.clear();
    
    // Note: Can't modify debugger from main thread
    // Breakpoint changes only take effect for next debug session
}

bool Debugger::HasBreakpoint(const std::string& filename, int line) const {
    auto it = m_breakpoints.find(filename);
    if (it == m_breakpoints.end()) {
        return false;
    }
    return it->second.count(line) > 0;
}

const std::set<int>& Debugger::GetBreakpoints(const std::string& filename) const {
    static const std::set<int> empty;
    auto it = m_breakpoints.find(filename);
    return (it != m_breakpoints.end()) ? it->second : empty;
}

void Debugger::Continue() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_CONTINUE);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepOver() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_OVER);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepInto() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_IN);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::StepOut() {
    if (!m_debugging.load() || !m_paused.load()) {
        return;
    }
    
    c11_debugger_set_step_mode(C11_STEP_OUT);
    m_paused.store(false);
    m_pauseCondition.notify_one(); // Wake up execution thread
}

void Debugger::SyncBreakpointsToDebugger() {
    for (const auto& pair : m_breakpoints) {
        for (int line : pair.second) {
            c11_debugger_setbreakpoint(pair.first.c_str(), line);
        }
    }
}

// Helper to safely get type name
static std::string GetSimpleTypeName(py_Type type) {
    // Map common types to readable names
    switch(type) {
        case tp_int: return "int";
        case tp_float: return "float";
        case tp_bool: return "bool";
        case tp_str: return "str";
        case tp_list: return "list";
        case tp_tuple: return "tuple";
        case tp_dict: return "dict";
        case tp_function: return "function";
        case tp_type: return "type";
        case tp_module: return "module";
        case tp_range: return "range";
        case tp_slice: return "slice";
        case tp_bytes: return "bytes";
        default: {
            // Try to get the actual type name using py_tpname
            const char* type_name = py_tpname(type);
            if (type_name && type_name[0] != '\0') {
                return std::string(type_name);
            }
            std::ostringstream oss;
            oss << "type_" << type;
            return oss.str();
        }
    }
}

// Helper to check if an object can have custom attributes
static bool IsExpandableObject(py_Ref value) {
    if (!value || !value->is_ptr) {
        return false;  // Not a pointer object
    }
    
    py_Type type = py_typeof(value);
    
    // Don't expand these types (handled specially or shouldn't be expanded)
    if (type == tp_int || type == tp_float || type == tp_bool || 
        type == tp_str || type == tp_bytes ||
        type == tp_function || type == tp_type || type == tp_nil ||
        type == tp_range || type == tp_slice) {
        return false;
    }
    
    // list, tuple, dict, module are handled separately
    if (py_islist(value) || py_istuple(value) || py_isdict(value) || type == tp_module) {
        return false;
    }
    
    // All other pointer objects can potentially have attributes
    return true;
}

// Helper to safely convert value to string representation
static std::string GetValueRepr(py_Ref value) {
    py_Type type = py_typeof(value);
    
    // Handle basic types directly
    if (py_isint(value)) {
        py_i64 val;
        if (py_castint(value, &val)) {
            return std::to_string(val);
        }
    } else if (py_isfloat(value)) {
        py_f64 val;
        if (py_castfloat(value, &val)) {
            return std::to_string(val);
        }
    } else if (py_isbool(value)) {
        return py_tobool(value) ? "True" : "False";
    } else if (py_isstr(value)) {
        return std::string("'") + py_tostr(value) + "'";
    } else if (py_isnil(value)) {
        return "None";
    } else if (type == tp_function) {
        return "<function>";
    } else if (type == tp_type) {
        return "<type>";
    } else if (type == tp_module) {
        return "<module>";
    } else if (py_islist(value)) {
        int len = py_list_len(value);
        return std::string("[...] (") + std::to_string(len) + " items)";
    } else if (py_istuple(value)) {
        int len = py_tuple_len(value);
        return std::string("(...) (") + std::to_string(len) + " items)";
    } else if (py_isdict(value)) {
        int len = py_dict_len(value);
        return std::string("{...} (") + std::to_string(len) + " items)";
    } else if (IsExpandableObject(value)) {
        // For custom objects, show type name
        const char* type_name = py_tpname(type);
        if (type_name && type_name[0] != '\0') {
            return std::string("<") + type_name + " object>";
        }
        return "<object>";
    }
    
    return "<object>";
}

// Helper to extract children of a collection variable  
// NOTE: This should only be called when NOT iterating over other collections
// to avoid stack corruption
// max_depth: 1 = extract only current level, 2 = extract current + 1 level deep, etc.
static void ExtractChildVariables(py_Ref value, std::vector<DebugVariable>& children, int max_items = 100, int max_depth = 1) {
    py_Type type = py_typeof(value);
    
    if (py_islist(value)) {
        // Extract list items with segmented display
        int len = py_list_len(value);
        
        if (len <= max_items) {
            // Small list - extract all items directly
            for (int i = 0; i < len; i++) {
                py_ItemRef item = py_list_getitem(value, i);
                if (!item) continue;
                
                DebugVariable child;
                child.name = "[" + std::to_string(i) + "]";
                child.value = GetValueRepr(item);
                child.type = GetSimpleTypeName(py_typeof(item));
                
                // Check if child also has children (nested structures)
                py_Type item_type = py_typeof(item);
                if (item_type == tp_list || item_type == tp_dict || item_type == tp_tuple || IsExpandableObject(item)) {
                    child.has_children = true;
                    // Recursively extract children for nested structures (limited depth)
                    if (max_depth > 1) {
                        ExtractChildVariables(item, child.children, 50, max_depth - 1);
                    }
                } else {
                    // Don't expand nested modules to avoid stack/state issues
                    child.has_children = false;
                }
                
                children.push_back(child);
            }
        } else {
            // Large list - create segments
            int num_segments = (len + max_items - 1) / max_items;
            
            for (int seg = 0; seg < num_segments; seg++) {
                int start = seg * max_items;
                int end = std::min(start + max_items - 1, len - 1);
                
                DebugVariable segment;
                segment.name = "[" + std::to_string(start) + "-" + std::to_string(end) + "]";
                segment.value = "(" + std::to_string(end - start + 1) + " items)";
                segment.type = "segment";
                segment.has_children = true;
                
                // Extract items in this segment
                for (int i = start; i <= end; i++) {
                    py_ItemRef item = py_list_getitem(value, i);
                    if (!item) continue;
                    
                    DebugVariable child;
                    child.name = "[" + std::to_string(i) + "]";
                    child.value = GetValueRepr(item);
                    child.type = GetSimpleTypeName(py_typeof(item));
                    
                    // Check if child also has children (nested structures)
                    py_Type item_type = py_typeof(item);
                    if (item_type == tp_list || item_type == tp_dict || item_type == tp_tuple || IsExpandableObject(item)) {
                        child.has_children = true;
                        // Don't recurse further to avoid stack issues
                        if (max_depth > 1) {
                            ExtractChildVariables(item, child.children, 50, max_depth - 1);
                        }
                    } else {
                        child.has_children = false;
                    }
                    
                    segment.children.push_back(child);
                }
                
                children.push_back(segment);
            }
        }
    }
    else if (py_istuple(value)) {
        // Extract tuple items with segmented display
        int len = py_tuple_len(value);
        
        if (len <= max_items) {
            // Small tuple - extract all items directly
            for (int i = 0; i < len; i++) {
                py_ItemRef item = py_tuple_getitem(value, i);
                if (!item) continue;
                
                DebugVariable child;
                child.name = "[" + std::to_string(i) + "]";
                child.value = GetValueRepr(item);
                child.type = GetSimpleTypeName(py_typeof(item));
                
                // Check if child also has children (nested structures)
                py_Type item_type = py_typeof(item);
                if (item_type == tp_list || item_type == tp_dict || item_type == tp_tuple || IsExpandableObject(item)) {
                    child.has_children = true;
                    ExtractChildVariables(item, child.children, 50);
                } else {
                    child.has_children = false;
                }
                
                children.push_back(child);
            }
        } else {
            // Large tuple - create segments
            int num_segments = (len + max_items - 1) / max_items;
            
            for (int seg = 0; seg < num_segments; seg++) {
                int start = seg * max_items;
                int end = std::min(start + max_items - 1, len - 1);
                
                DebugVariable segment;
                segment.name = "[" + std::to_string(start) + "-" + std::to_string(end) + "]";
                segment.value = "(" + std::to_string(end - start + 1) + " items)";
                segment.type = "segment";
                segment.has_children = true;
                
                // Extract items in this segment
                for (int i = start; i <= end; i++) {
                    py_ItemRef item = py_tuple_getitem(value, i);
                    if (!item) continue;
                    
                    DebugVariable child;
                    child.name = "[" + std::to_string(i) + "]";
                    child.value = GetValueRepr(item);
                    child.type = GetSimpleTypeName(py_typeof(item));
                    
                    // Check if child also has children (nested structures)
                    py_Type item_type = py_typeof(item);
                    if (item_type == tp_list || item_type == tp_dict || item_type == tp_tuple || IsExpandableObject(item)) {
                        child.has_children = true;
                        // Don't recurse further to avoid stack issues
                        if (max_depth > 1) {
                            ExtractChildVariables(item, child.children, 50, max_depth - 1);
                        }
                    } else {
                        child.has_children = false;
                    }
                    
                    segment.children.push_back(child);
                }
                
                children.push_back(segment);
            }
        }
    }
    else if (py_isdict(value)) {
        // Extract dict items with segmented display
        int total_len = py_dict_len(value);
        
        // First, collect all dict items into a temporary vector
        std::vector<DebugVariable> all_items;
        
        struct DictCollectContext {
            std::vector<DebugVariable>* items;
        };
        
        DictCollectContext collect_ctx;
        collect_ctx.items = &all_items;
        
        auto collect_all_items = [](py_Ref key, py_Ref val, void* ctx_ptr) -> bool {
            DictCollectContext* ctx = (DictCollectContext*)ctx_ptr;
            
            DebugVariable child;
            // Format key
            if (py_isstr(key)) {
                child.name = std::string("'") + py_tostr(key) + "'";
            } else {
                child.name = GetValueRepr(key);
            }
            
            child.value = GetValueRepr(val);
            child.type = GetSimpleTypeName(py_typeof(val));
            
            // Check if child also has children (nested structures)
            py_Type val_type = py_typeof(val);
            if (val_type == tp_list || val_type == tp_dict || val_type == tp_tuple || IsExpandableObject(val)) {
                child.has_children = true;
                // Don't extract children during dict iteration to avoid stack issues
            } else {
                child.has_children = false;
            }
            
            ctx->items->push_back(child);
            return true; // Continue iteration
        };
        
        py_dict_apply(value, collect_all_items, &collect_ctx);
        
        // Now organize into segments if needed
        if (total_len <= max_items) {
            // Small dict - add all items directly
            for (const auto& item : all_items) {
                children.push_back(item);
            }
        } else {
            // Large dict - create segments
            int num_segments = (total_len + max_items - 1) / max_items;
            
            for (int seg = 0; seg < num_segments; seg++) {
                int start = seg * max_items;
                int end = std::min(start + max_items - 1, total_len - 1);
                
                DebugVariable segment;
                segment.name = "[" + std::to_string(start) + "-" + std::to_string(end) + "]";
                segment.value = "(" + std::to_string(end - start + 1) + " items)";
                segment.type = "segment";
                segment.has_children = true;
                
                // Add items in this segment
                for (int i = start; i <= end && i < (int)all_items.size(); i++) {
                    segment.children.push_back(all_items[i]);
                }
                
                children.push_back(segment);
            }
        }
    }
    else if (type == tp_module) {
        // Extract module attributes using py_getattr
        // This is safer than using dir() + eval which can modify stack state
        
        // For known modules like 'test', we'll try common attribute names
        const char* common_attrs[] = {
            "pi", "add", "e", "sqrt", "sin", "cos", 
            "Path", "name", "version", "main"
        };
        
        int found_count = 0;
        for (const char* attr_name : common_attrs) {
            if (found_count >= max_items) break;
            
            py_push(value);
            if (py_getattr(value, py_name(attr_name))) {
                py_Ref attr_value = py_retval();
                
                DebugVariable child;
                child.name = attr_name;
                child.value = GetValueRepr(attr_value);
                child.type = GetSimpleTypeName(py_typeof(attr_value));
                
                // Check if attribute has children (but limit recursion depth for modules)
                py_Type val_type = py_typeof(attr_value);
                if (val_type == tp_list || val_type == tp_dict || val_type == tp_tuple) {
                    // Only expand collections, not nested modules to avoid issues
                    child.has_children = true;
                    // Don't recurse further to avoid stack issues
                    if (max_depth > 1) {
                        ExtractChildVariables(attr_value, child.children, 50, max_depth - 1);
                    }
                } else {
                    child.has_children = false;
                }
                
                children.push_back(child);
                found_count++;
            }
            py_clearexc(NULL);
            py_pop();
        }
        
        // If no items were found, show a note
        if (children.empty()) {
            DebugVariable note;
            note.name = "(no public attributes found)";
            note.value = "Module has no common attributes";
            note.type = "";
            note.has_children = false;
            children.push_back(note);
        }
    }
    else if (IsExpandableObject(value)) {
        // Extract object attributes
        // Use py_getattr to access __dict__ safely without directly calling py_applydict
        std::vector<DebugVariable> all_attrs;
        
        // Try to get the __dict__ attribute using py_getattr
        py_push(value);
        bool has_dict = py_getattr(value, py_name("__dict__"));
        if (has_dict) {
            py_Ref dict_obj = py_retval();
            if (py_isdict(dict_obj)) {
                // Extract items from the __dict__ dictionary
                struct DictCollectContext {
                    std::vector<DebugVariable>* items;
                };
                
                DictCollectContext collect_ctx;
                collect_ctx.items = &all_attrs;
                
                auto collect_dict_items = [](py_Ref key, py_Ref val, void* ctx_ptr) -> bool {
                    DictCollectContext* ctx = (DictCollectContext*)ctx_ptr;
                    
                    // Skip private attributes starting with '_'
                    if (py_isstr(key)) {
                        const char* key_str = py_tostr(key);
                        if (key_str && key_str[0] == '_') {
                            return true;  // Continue iteration
                        }
                        
                        DebugVariable child;
                        child.name = key_str;
                        child.value = GetValueRepr(val);
                        child.type = GetSimpleTypeName(py_typeof(val));
                        
                        // Check if attribute has children
                        py_Type val_type = py_typeof(val);
                        if (val_type == tp_list || val_type == tp_dict || val_type == tp_tuple || IsExpandableObject(val)) {
                            child.has_children = true;
                            // Lazy load - don't extract children until user expands
                        } else {
                            child.has_children = false;
                        }
                        
                        ctx->items->push_back(child);
                    }
                    return true;  // Continue iteration
                };
                
                // Use py_dict_apply to iterate over __dict__
                py_dict_apply(dict_obj, collect_dict_items, &collect_ctx);
            }
        }
        // Clear any exception that might have occurred
        py_clearexc(NULL);
        py_pop();
        
        // If no attributes found, show a note
        if (all_attrs.empty()) {
            DebugVariable note;
            note.name = "(no attributes)";
            note.value = "Object has no accessible attributes";
            note.type = "";
            note.has_children = false;
            children.push_back(note);
            return;
        }
        
        // Sort attributes by name for better readability
        std::sort(all_attrs.begin(), all_attrs.end(), 
            [](const DebugVariable& a, const DebugVariable& b) {
                return a.name < b.name;
            });
        
        int total_len = all_attrs.size();
        
        // Apply segmented display for large objects
        if (total_len <= max_items) {
            // Small object - add all attributes directly
            for (auto& attr : all_attrs) {
                // Extract children if they exist (lazy load)
                if (attr.has_children) {
                    // Use py_getattr instead of py_getdict to be safe
                    py_push(value);
                    if (py_getattr(value, py_name(attr.name.c_str()))) {
                        py_Ref attr_value = py_retval();
                        ExtractChildVariables(attr_value, attr.children, 50, 0);
                    }
                    py_clearexc(NULL);
                    py_pop();
                }
                children.push_back(attr);
            }
        } else {
            // Large object - create segments
            int num_segments = (total_len + max_items - 1) / max_items;
            
            for (int seg = 0; seg < num_segments; seg++) {
                int start = seg * max_items;
                int end = std::min(start + max_items - 1, total_len - 1);
                
                DebugVariable segment;
                segment.name = "[" + std::to_string(start) + "-" + std::to_string(end) + "]";
                segment.value = "(" + std::to_string(end - start + 1) + " attributes)";
                segment.type = "segment";
                segment.has_children = true;
                
                // Add attributes in this segment
                for (int i = start; i <= end && i < (int)all_attrs.size(); i++) {
                    auto& attr = all_attrs[i];
                    // Extract children if they exist (lazy load)
                    if (attr.has_children) {
                        // Use py_getattr instead of py_getdict to be safe
                        py_push(value);
                        if (py_getattr(value, py_name(attr.name.c_str()))) {
                            py_Ref attr_value = py_retval();
                            ExtractChildVariables(attr_value, attr.children, 50, 0);
                        }
                        py_clearexc(NULL);
                        py_pop();
                    }
                    segment.children.push_back(attr);
                }
                
                children.push_back(segment);
            }
        }
    }
}

// Helper callback for py_dict_apply to collect variables
struct CollectVariablesContext {
    std::vector<DebugVariable>* variables;
    bool filter_builtins;
};

static bool CollectVariablesCallback(py_Ref key, py_Ref val, void* ctx) {
    CollectVariablesContext* context = (CollectVariablesContext*)ctx;
    
    // Key should be string for normal dicts
    if (!py_isstr(key)) {
        return true; // Skip non-string keys, continue iteration
    }
    
    DebugVariable var;
    
    // Get variable name (key is guaranteed to be string now)
    var.name = py_tostr(key);
    
    // Skip built-in functions and modules if filtering is enabled
    if (context->filter_builtins && (!var.name.empty() && var.name[0] == '_')) {
        return true; // Continue iteration
    }
    
    // Get variable value using safe method
    var.value = GetValueRepr(val);
    
    // Get variable type using safe method
    var.type = GetSimpleTypeName(py_typeof(val));
    
    // Check if variable has children (expandable types)
    py_Type val_type = py_typeof(val);
    if (val_type == tp_list || val_type == tp_dict || val_type == tp_tuple || 
        val_type == tp_module || IsExpandableObject(val)) {
        var.has_children = true;
        // Don't extract children during iteration to avoid stack corruption
        // Children will be lazy-loaded when user expands the variable in UI
    } else {
        var.has_children = false;
    }
    
    context->variables->push_back(var);
    return true; // Continue iteration
}

// Helper to extract variables from a Python object (dict/namedict)
void Debugger::ExtractVariables(py_Ref obj, std::vector<DebugVariable>& variables, bool filter_builtins) {
    // For dict, use py_dict_apply directly
    if (py_isdict(obj)) {
        CollectVariablesContext ctx;
        ctx.variables = &variables;
        ctx.filter_builtins = filter_builtins;
        py_dict_apply(obj, CollectVariablesCallback, &ctx);
        return;
    }
    
    // For namedict or other types, use py_smarteval to convert to list
    // Save current value to a temporary register
    py_push(obj);
    py_Ref temp_obj = py_peek(-1);
    
    const char* eval_code = "[(k,v) for k,v in _0.items()]";
    if (!py_smarteval(eval_code, NULL, temp_obj)) {
        // If items() fails, clear exception and return
        py_clearexc(NULL);
        py_pop(); // Remove temp_obj
        return;
    }
    
    // IMPORTANT: Keep items_list on the stack to prevent GC
    py_Ref items_list = py_retval();
    
    if (!py_islist(items_list)) {
        py_pop(); // Remove temp_obj
        return;
    }
    
    // Push items_list to stack to keep it alive during processing
    py_push(items_list);
    
    // First pass: collect variable info without nested extraction
    // This prevents stack corruption during iteration
    struct VarInfo {
        std::string name;
        std::string value_repr;
        std::string type_name;
        py_Type type;
        bool is_expandable;
    };
    std::vector<VarInfo> temp_vars;
    
    int len = py_list_len(items_list);
    for (int i = 0; i < len; i++) {
        py_ItemRef tuple = py_list_getitem(items_list, i);
        if (!tuple || !py_istuple(tuple)) {
            continue;
        }
        
        py_ItemRef key = py_tuple_getitem(tuple, 0);
        py_ItemRef value = py_tuple_getitem(tuple, 1);
        
        if (!key || !value || !py_isstr(key)) {
            continue;
        }
        
        std::string var_name = py_tostr(key);
        
        // Skip built-in variables if filtering
        if (filter_builtins && !var_name.empty() && var_name[0] == '_') {
            continue;
        }
        
        // Extract basic info without calling nested functions
        VarInfo info;
        info.name = var_name;
        info.value_repr = GetValueRepr(value);
        info.type = py_typeof(value);
        info.type_name = GetSimpleTypeName(info.type);
        info.is_expandable = (info.type == tp_list || info.type == tp_dict || 
                              info.type == tp_tuple || info.type == tp_module || 
                              IsExpandableObject(value));
        
        temp_vars.push_back(info);
    }
    
    // Now safe to pop both items_list and temp_obj
    py_pop(); // items_list
    py_pop(); // temp_obj
    
    // Second pass: create DebugVariable objects
    // Note: We can't extract children here because the original objects are gone
    // Children will be extracted on-demand when the user expands the variable
    for (const auto& info : temp_vars) {
        DebugVariable var;
        var.name = info.name;
        var.value = info.value_repr;
        var.type = info.type_name;
        var.has_children = info.is_expandable;
        // Don't extract children here - they'll be extracted when needed
        
        variables.push_back(var);
    }
}

std::vector<DebugVariable> Debugger::GetVariableChildren(const std::string& varName, bool isLocal) const {
    const std::vector<DebugVariable>& vars = isLocal ? m_localVariables : m_globalVariables;
    
    for (const auto& var : vars) {
        if (var.name == varName) {
            return var.children;
        }
    }
    
    // Return empty vector if variable not found
    return std::vector<DebugVariable>();
}

void Debugger::UpdateDebugInfo(py_Frame* frame) {
    // Get current location
    const char* filename = py_Frame_sourceloc(frame, &m_currentLine);
    if (filename) {
        m_currentFile = filename;
    }
    
    // Clear previous data
    m_stackFrames.clear();
    m_localVariables.clear();
    m_globalVariables.clear();
    
    // Get local variables from current frame
    py_Frame_newlocals(frame, py_r0());
    py_Ref locals_obj = py_r0();
    if (locals_obj) {
        ExtractVariables(locals_obj, m_localVariables, false);
    }
    
    // Get global variables from current frame  
    py_Frame_newglobals(frame, py_r1());
    py_Ref globals_obj = py_r1();
    if (globals_obj) {
        ExtractVariables(globals_obj, m_globalVariables, true);
    }
}

void Debugger::TraceCallback(py_Frame* frame, enum py_TraceEvent event) {
    if (!s_instance || !s_instance->m_debugging.load()) {
        return;
    }
    
    // Only process LINE events for stepping
    // PUSH and POP events are for function calls but we handle those in c11_debugger_on_trace
    if (event != TRACE_EVENT_LINE) {
        // Still call internal debugger to update its state
        c11_debugger_on_trace(frame, event);
        return;
    }
    
    // Get current location BEFORE calling debugger
    int current_line;
    const char* current_file = py_Frame_sourceloc(frame, &current_line);
    
    // Check if we've moved to a different line (to avoid multiple pauses on same line)
    static int last_paused_line = -1;
    static std::string last_paused_file;
    
    bool line_changed = (current_line != last_paused_line) || 
                       (current_file && current_file != last_paused_file);
    
    // Call internal debugger trace handler
    C11_DEBUGGER_STATUS status = c11_debugger_on_trace(frame, event);
    
    if (status != C11_DEBUGGER_SUCCESS) {
        // Error occurred, stop debugging
        s_instance->m_debugging.store(false);
        return;
    }
    
    // Check if we should pause
    C11_STOP_REASON reason = c11_debugger_should_pause();
    
    // Only pause if:
    // 1. Debugger says we should pause, AND
    // 2. We've moved to a different line OR it's an exception
    // NOTE: For breakpoints, we ONLY pause if line_changed is true
    // This prevents re-triggering the same breakpoint after Continue
    bool should_pause = false;
    if (reason != C11_DEBUGGER_NOSTOP) {
        if (reason == C11_DEBUGGER_EXCEPTION) {
            // Always pause on exceptions
            should_pause = true;
        } else if (line_changed) {
            // Pause on step or breakpoint only if we've moved to a different line
            should_pause = true;
        } else if (reason == C11_DEBUGGER_BP) {
            // Breakpoint on same line - clear keep_suspend to prevent re-triggering
            // This happens when pressing F5 (Continue) after hitting a breakpoint
            // We need to clear the suspend flag without changing the step mode
            // (c11_debugger_should_pause sets it to true, but we want to ignore this pause)
            c11_debugger_set_step_mode(C11_STEP_CONTINUE);
        }
        // For C11_DEBUGGER_STEP on same line: do nothing, keep current step mode
        // This allows F10/F11 to work correctly - they'll pause on the next line
    }
    
    if (should_pause) {
        // Update last paused location
        last_paused_line = current_line;
        if (current_file) {
            last_paused_file = current_file;
        }
        
        // We should pause
        s_instance->m_paused.store(true);
        s_instance->UpdateDebugInfo(frame);
        
#if DEBUGGER_VERBOSE_LOGGING
        if (s_instance->m_logCallback) {
            std::ostringstream msg;
            msg << "[debug] Paused at " << s_instance->m_currentFile 
                << ":" << s_instance->m_currentLine;
            
            switch (reason) {
                case C11_DEBUGGER_STEP:
                    msg << " (step)";
                    break;
                case C11_DEBUGGER_BP:
                    msg << " (breakpoint)";
                    break;
                case C11_DEBUGGER_EXCEPTION:
                    msg << " (exception)";
                    break;
                default:
                    break;
            }
            msg << "\n";
            s_instance->m_logCallback(msg.str());
        }
#endif
        
        // Wait for user action using condition variable
        // This blocks the Python thread but NOT the main GUI thread
        std::unique_lock<std::mutex> lock(s_instance->m_pauseMutex);
        while (c11_debugger_should_keep_pause() && s_instance->m_debugging.load()) {
            // Wait for Continue/Step/Stop command from main thread
            s_instance->m_pauseCondition.wait(lock);
        }
        
        s_instance->m_paused.store(false);
    }
}

void Debugger::ExecuteInThread(const std::string& code, const std::string& filename) {
    // NOTE: This runs in a background thread
    // Switch to VM 1 (main thread uses VM 0)
    py_switchvm(1);
    
    // Reset VM 1 to have a clean state for each debug session
    if (m_logCallback) {
        m_logCallback("[info] Resetting Python VM 1 for debugging...\n");
    }
    py_resetvm();
    
    // Setup callbacks for VM 1 (redirect to our log callback)
    // Store callback in static variable so we can use a non-capturing lambda
    s_vm1_logCallback = m_logCallback;
    py_callbacks()->print = [](const char* s) {
        if (s_vm1_logCallback) {
            s_vm1_logCallback(s);
        }
    };
    
    // Create test module for VM 1
    py_GlobalRef mod = py_newmodule("test");
    
    // Set pi attribute
    py_newfloat(py_r0(), 3.14);
    py_setdict(mod, py_name("pi"), py_r0());
    
    // Bind add function
    py_bindfunc(mod, "add", [](int argc, py_StackRef argv) -> bool {
        if (argc != 2) return TypeError("add() requires 2 arguments");
        
        py_i64 a = py_toint(py_offset(argv, 0));
        py_i64 b = py_toint(py_offset(argv, 1));
        
        // Create return value using retval
        py_newint(py_retval(), a + b);
        return true;
    });
    
    // Initialize debugger (must be done AFTER py_switchvm)
    c11_debugger_init();
    
    // Sync breakpoints to internal debugger
    SyncBreakpointsToDebugger();
    
    // Set step mode to continue initially
    c11_debugger_set_step_mode(C11_STEP_CONTINUE);
    
    // Install trace callback
    py_sys_settrace(TraceCallback, true);
    
    // Execute code
    bool success = py_exec(code.c_str(), filename.c_str(), EXEC_MODE, NULL);
    
    // Remove trace callback
    py_sys_settrace(NULL, false);
    
    if (!success) {
        char* exc_msg = py_formatexc();
        if (m_logCallback && exc_msg) {
            std::string error = "[error] ";
            error += exc_msg;
            error += "\n";
            m_logCallback(error);
        }
        py_free(exc_msg);
        py_clearexc(NULL);
    }
    
    if (m_logCallback) {
        m_logCallback("[info] Python execution completed\n");
    }
    
    // Switch back to VM 0 before thread exits
    py_switchvm(0);
    
    m_debugging.store(false);
    m_paused.store(false);
}

#endif // ENABLE_DEBUGGER



