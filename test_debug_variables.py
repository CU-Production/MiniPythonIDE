# Test script for debugging variable expansion features
# This script tests list, tuple, dict (table), and module display

import test

# Test 1: Simple list
simple_list = [1, 2, 3, 4, 5]
print("Created simple_list")

# Test 2: Nested list (list inside list)
nested_list = [[1, 2], [3, 4], [5, 6]]
print("Created nested_list")

# Test 3: List with mixed types
mixed_list = [1, "hello", 3.14, True, None]
print("Created mixed_list")

# Test 4: Simple tuple
simple_tuple = (10, 20, 30)
print("Created simple_tuple")

# Test 5: Nested tuple
nested_tuple = ((1, 2), (3, 4), (5, 6))
print("Created nested_tuple")

# Test 6: Simple dict (table)
simple_dict = {"name": "Alice", "age": 25, "city": "Beijing"}
print("Created simple_dict")

# Test 7: Nested dict
nested_dict = {
    "person": {"name": "Bob", "age": 30},
    "scores": {"math": 95, "english": 88}
}
print("Created nested_dict")

# Test 8: Dict with list values
dict_with_list = {
    "numbers": [1, 2, 3, 4, 5],
    "names": ["Alice", "Bob", "Charlie"]
}
print("Created dict_with_list")

# Test 9: List with dict items
list_with_dict = [
    {"id": 1, "name": "Item1"},
    {"id": 2, "name": "Item2"},
    {"id": 3, "name": "Item3"}
]
print("Created list_with_dict")

# Test 10: Complex nested structure
complex_data = {
    "users": [
        {"name": "Alice", "scores": [95, 88, 92]},
        {"name": "Bob", "scores": [78, 85, 90]}
    ],
    "settings": {
        "theme": "dark",
        "features": ["feature1", "feature2", "feature3"]
    }
}
print("Created complex_data")

# Test 11: Module object
test_module = test
print("Created test_module reference")

# Test 12: Large list (test truncation)
large_list = list(range(200))
print("Created large_list with 200 items")

# Test 13: Tuple with module
tuple_with_module = (test, "module_test", 42)
print("Created tuple_with_module")

# Set a breakpoint here to inspect all variables
print("All test variables created - set breakpoint on next line")
result = test.add(10, 20)
print(f"Test module add function: 10 + 20 = {result}")
print(f"Test module pi value: {test.pi}")

print("Script completed successfully")

