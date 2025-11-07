"""
Test case for object attribute display in debugger

Test Scenario 1 - Simple Class Object:
1. Set breakpoint at line 23
2. Run Debug (F9)
3. When it hits the breakpoint, check Variables window
4. Expected: Should see 'person' variable with expandable attributes
5. Expand 'person' to see: name, age, city

Test Scenario 2 - Nested Objects:
1. Continue to line 32 breakpoint
2. Expected: Should see 'company' with expandable attributes
3. Expand 'company' to see: name, employees (which is a list)
4. Expand 'employees' to see list items, each should be an expandable Person object

Test Scenario 3 - Object in Collection:
1. Continue to line 44 breakpoint
2. Expected: Should see 'data' dict with 'user' key
3. Expand data, then expand the 'user' entry
4. Should see the Person object attributes
"""

# Simple class definition
class Person:
    def __init__(self, name, age, city):
        self.name = name
        self.age = age
        self.city = city

# Create a simple object
person = Person("Alice", 30, "New York")
print(f"Created person: {person.name}")  # Set breakpoint here (line 23)

# Nested object test
class Company:
    def __init__(self, name):
        self.name = name
        self.employees = []

company = Company("TechCorp")
company.employees.append(Person("Bob", 25, "San Francisco"))
company.employees.append(Person("Charlie", 35, "Boston"))
print(f"Company: {company.name}")  # Set breakpoint here (line 32)

# Object in collection test
class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

data = {
    "user": Person("David", 28, "Chicago"),
    "location": Point(10, 20),
    "items": [1, 2, 3]
}
print(f"Data keys: {list(data.keys())}")  # Set breakpoint here (line 44)

# Object with many attributes (test segmentation)
class LargeObject:
    def __init__(self):
        # Create many attributes
        for i in range(150):
            setattr(self, f"attr_{i:03d}", i * 2)

large_obj = LargeObject()
print(f"Large object created")  # Set breakpoint here

print("Test completed!")

