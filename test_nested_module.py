# Test script for nested module display in debugger
# This tests module references in lists and dictionaries

import test

print("Testing nested module display...")

# Test 1: Module in a list
modules_list = [test, "string", 123, test]
print(f"Created modules_list with {len(modules_list)} items")

# Test 2: Module in a dictionary
config = {
    "module": test,
    "name": "TestModule",
    "enabled": True,
    "version": 1.0
}
print("Created config dictionary with module reference")

# Test 3: Nested structure with modules
nested = {
    "modules": [test, test],
    "data": {
        "primary": test,
        "values": [1, 2, 3]
    },
    "list_of_lists": [[test, "text"], [test]]
}
print("Created deeply nested structure")

# Test 4: List of dictionaries with modules
items = [
    {"id": 1, "module": test, "name": "item1"},
    {"id": 2, "module": test, "name": "item2"}
]
print("Created list of dictionaries with module references")

# Test 5: Multiple different modules
import sys
multi_modules = {
    "test": test,
    "sys": sys
}
print("Created dictionary with multiple module references")

# Test 6: Module in tuple
module_tuple = (test, "data", 42, test)
print("Created tuple with module references")

# Test 7: Complex nested with large list
large_with_module = {
    "modules": [test] * 150,  # Large list
    "info": "test"
}
print("Created structure with large list of modules (should be segmented)")

# Set breakpoint here
print("\n=== Set breakpoint on the next line ===")
result = test.add(50, 75)
print(f"Result: {result}")

print("\n=== Expected behavior ===")
print("1. modules_list[0] and [3] should be expandable to show pi and add")
print("2. config['module'] should be expandable")
print("3. nested structure should allow full expansion of all module references")
print("4. items[0]['module'] should be expandable")
print("5. multi_modules should show both test and sys modules")
print("6. module_tuple[0] and [3] should be expandable")
print("7. large_with_module['modules'] should be segmented, each segment shows modules")

