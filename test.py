import test

b = 10
b += 1
b += 1
b += 1
b += 1

def fibonacci(num):
    if num < 0:
        print("Incorrect input")
        return

    elif num < 2:
        return num

    return fibonacci(num - 1) + fibonacci(num - 2)

print(fibonacci(9))

print("hello world")

a = [1, 2, 3]
print(sum(a))

print(test.pi)         # 3.14
print(test.add(1, 2))  # 3