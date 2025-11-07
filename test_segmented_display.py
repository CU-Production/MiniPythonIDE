# Test script for segmented display feature
# This tests the [0-99], [100-199] style segmented display

print("Creating test variables...")

# Test 1: Small list (no segmentation needed)
small_list = list(range(50))
print(f"Created small_list with {len(small_list)} items")

# Test 2: Medium list (requires 2 segments)
medium_list = list(range(150))
print(f"Created medium_list with {len(medium_list)} items - should show [0-99], [100-149]")

# Test 3: Large list (requires 3 segments)
large_list = list(range(250))
print(f"Created large_list with {len(large_list)} items - should show [0-99], [100-199], [200-249]")

# Test 4: Very large list (requires 5 segments)
very_large_list = list(range(450))
print(f"Created very_large_list with {len(very_large_list)} items - should show 5 segments")

# Test 5: Large tuple
large_tuple = tuple(range(200))
print(f"Created large_tuple with {len(large_tuple)} items - should show [0-99], [100-199]")

# Test 6: Large dict
large_dict = {f"key_{i}": i * 2 for i in range(200)}
print(f"Created large_dict with {len(large_dict)} items - should show segments")

# Test 7: Nested structure with large list
nested_structure = {
    "small": [1, 2, 3],
    "large": list(range(150)),
    "tuple": tuple(range(120))
}
print(f"Created nested_structure with mixed sizes")

# Test 8: List of dicts (medium size)
list_of_dicts = [{"id": i, "value": i * 10} for i in range(150)]
print(f"Created list_of_dicts with {len(list_of_dicts)} items")

# Test 9: Edge case - exactly 100 items (no segmentation)
exactly_100 = list(range(100))
print(f"Created exactly_100 with {len(exactly_100)} items - should NOT segment")

# Test 10: Edge case - 101 items (should create 2 segments)
exactly_101 = list(range(101))
print(f"Created exactly_101 with {len(exactly_101)} items - should show [0-99], [100-100]")

# Set breakpoint here to inspect variables
print("\nAll test variables created!")
print("Set a breakpoint on the next line to inspect the segmented display")
result = sum(small_list)  # Breakpoint here
print(f"Sum of small_list: {result}")

print("\nTest completed successfully!")
print("Expected behavior:")
print("- Lists/tuples/dicts with <= 100 items: Direct display")
print("- Lists/tuples/dicts with > 100 items: Segmented display [0-99], [100-199], etc.")
print("- Each segment is expandable to show actual items")
print("- Item indices are preserved (e.g., segment [100-199] contains items numbered 100-199)")

