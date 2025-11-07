import test

b = 10
b += 1
b += 1
b += 1
b += 1

def fibonacci(num):

    # check if num is less than 0 it will return none
    if num < 0:
        print("Incorrect input")
        return

    # check if num between 1, 0 it will return num
    elif num < 2:
        return num

    # return the fibonacci of num - 1 & num - 2
    return fibonacci(num - 1) + fibonacci(num - 2)

print(fibonacci(9))

print("hello world")

a = [1, 2, 3]
print(sum(a))

print(test.pi)         # 3.14
print(test.add(1, 2))  # 3