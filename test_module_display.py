# Test script for module display in debugger
# This tests the display of module attributes and methods

import test

print("Testing module display in debugger...")

# Test 1: Reference the test module
test_module = test
print(f"Created reference to test module: {test_module}")

# Test 2: Access module attributes
print(f"test.pi = {test.pi}")
result = test.add(10, 20)
print(f"test.add(10, 20) = {result}")

# Test 3: Create a custom module-like object with various types
class CustomModule:
    """A custom module-like class for testing"""
    
    # Simple attributes
    name = "CustomModule"
    version = 1.0
    enabled = True
    
    # Collections
    items = [1, 2, 3, 4, 5]
    config = {"debug": True, "level": 2}
    tags = ("tag1", "tag2", "tag3")
    
    # Methods
    @staticmethod
    def method1():
        return "method1"
    
    @staticmethod
    def method2(x):
        return x * 2

custom = CustomModule()
print(f"Created custom module-like object")

# Test 4: Nested module reference
nested_structure = {
    "module_ref": test,
    "custom_ref": custom,
    "data": [1, 2, 3]
}
print(f"Created nested structure with module references")

# Set breakpoint here to inspect module display
print("\n=== Set breakpoint on the next line ===")
final_result = test.add(100, 200)
print(f"Final result: {final_result}")

print("\n=== Expected behavior ===")
print("1. test_module should be expandable")
print("2. Expanding test_module should show:")
print("   - pi: 3.14 (float)")
print("   - add: <function> (function)")
print("3. Private attributes (starting with _) should be hidden")
print("4. custom object should show its attributes")
print("5. nested_structure should allow expanding module_ref")

