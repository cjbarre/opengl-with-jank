# Jank C++ Interop Documentation

## Overview

Jank provides seamless C++ interoperability, allowing you to directly use C++ code, types, and functions from within Jank programs. This is a core feature that enables native performance and integration with existing C++ libraries.

## Core Interop Forms

### 1. `cpp/raw` - Embedding Raw C++ Code

The `cpp/raw` form allows you to embed raw C++ code directly into your Jank program:

```clojure
(cpp/raw "namespace jank::example {
            struct foo {
              int value{42};
            };
          }")
```

**Requirements:**
- Must take exactly one argument: a string literal containing valid C++ code
- The C++ code is compiled as part of your program
- Typically used to define types, functions, and variables in C++ namespaces

### 2. Accessing C++ Values and Types

#### Global Values and Static Members
Access C++ global variables, static members, and enum values using the `cpp/` prefix with dot notation:

```clojure
; Access enum values
cpp/jank.example.MyEnum.VALUE1

; Access static members
cpp/jank.example.MyClass.static_member

; Access global variables
cpp/jank.example.global_var
```

#### Member Access with `cpp/.-`
Access instance members (fields) of C++ objects:

```clojure
(let [obj (cpp/jank.example.foo.)]
  (cpp/.-value obj))  ; Access the 'value' field
```

### 3. Creating C++ Objects

#### Stack-Allocated Objects (Constructors)
Create C++ objects on the stack using constructor syntax with a trailing dot:

```clojure
; Default constructor
(cpp/jank.example.foo.)

; Constructor with arguments
(cpp/jank.example.foo. arg1 arg2)

; Built-in types
(cpp/int.)       ; Creates int with default value
(cpp/int. 42)    ; Creates int with value 42
(cpp/long. 100)
```

#### Heap-Allocated Objects with `cpp/new`
Allocate objects on the heap, returning a pointer:

```clojure
(let [ptr (cpp/new cpp/jank.example.foo)]
  ; Use cpp/* to dereference the pointer
  (cpp/.-value (cpp/* ptr)))
```

#### Deleting Heap Objects with `cpp/delete`
Delete heap-allocated objects:

```clojure
(let [ptr (cpp/new cpp/jank.example.foo)]
  ; ... use the object ...
  (cpp/delete ptr))
```

### 4. Calling C++ Functions

#### Global and Namespace-Level Functions
Call C++ functions directly:

```clojure
(cpp/jank.example.my_function arg1 arg2)

; Void functions return nil
(cpp/jank.example.void_function)
```

#### Member Functions
Call member functions on C++ objects:

```clojure
(let [obj (cpp/jank.example.MyClass.)]
  (cpp/.member_function obj arg1 arg2))
```

### 5. C++ Operators

Jank provides access to C++ operators through special forms:

#### Arithmetic Operators
```clojure
(cpp/+ a b)   ; Addition
(cpp/- a b)   ; Subtraction
(cpp/* ptr)   ; Dereference (unary)
(cpp/* a b)   ; Multiplication (binary)
(cpp// a b)   ; Division
(cpp/% a b)   ; Modulo
```

#### Increment/Decrement
```clojure
(cpp/++ ptr)  ; Pre-increment
(cpp/-- ptr)  ; Pre-decrement
```

#### Comparison Operators
```clojure
(cpp/== a b)  ; Equality
(cpp/!= a b)  ; Inequality
(cpp/< a b)   ; Less than
(cpp/<= a b)  ; Less than or equal
(cpp/> a b)   ; Greater than
(cpp/>= a b)  ; Greater than or equal
```

#### Logical Operators
```clojure
(cpp/! value)  ; Logical NOT
```

#### Bitwise Operators
```clojure
(cpp/<< a b)  ; Left shift
(cpp/>> a b)  ; Right shift
(cpp/| a b)   ; Bitwise OR
(cpp/& value) ; Address-of (unary)
(cpp/& a b)   ; Bitwise AND (binary)
```

#### Assignment
```clojure
(cpp/= target value)  ; Assignment
```

### 6. Type Specifications with `cpp/type`

Explicitly specify C++ types, especially useful for templates:

```clojure
; Create a templated type
((cpp/type "std::vector<int>"))

; Use with constructors
((cpp/type "jank::example::foo<int, long>") arg1 arg2)
```

### 7. Boxing and Unboxing

Convert between Jank's boxed types and C++ native types:

#### `cpp/box` - Convert C++ Value to Jank Object
```clojure
(cpp/box cpp-value)
```

#### `cpp/unbox` - Extract C++ Value from Jank Object
```clojure
(cpp/unbox boxed-value)
```

### 8. Working with C++ Values

#### `cpp/value` - Access C++ Values
Access specific C++ values or create value wrappers:

```clojure
(cpp/value "expression")
```

## Type System Integration

### Built-in Type Constructors
Jank provides constructors for common C++ types:

- `cpp/int`, `cpp/int.`
- `cpp/long`, `cpp/long.`
- `cpp/float`, `cpp/float.`
- `cpp/double`, `cpp/double.`
- `cpp/bool`, `cpp/bool.`
- `cpp/char`, `cpp/char.`

### Nullptr Support
```clojure
cpp/nullptr  ; C++ nullptr value
```

## Templates and Generics

Jank supports C++ templates through explicit type specifications:

```clojure
(cpp/raw "namespace example {
            template<typename T>
            struct Container {
              T value;
            };
          }")

; Instantiate template
((cpp/type "example::Container<int>") 42)
```

## Enums

Both C-style and C++11 enum classes are supported:

```clojure
(cpp/raw "namespace example {
            enum Color { RED = 1, GREEN = 2, BLUE = 3 };
            enum class Status { OK, ERROR };
          }")

; Access enum values
cpp/example.RED
cpp/example.Status.OK
```

## Function Overloading

C++ function overloading is supported - the correct overload is selected based on argument types:

```clojure
(cpp/raw "namespace example {
            int process(int x) { return x * 2; }
            float process(float x) { return x * 3.0f; }
          }")

(cpp/example.process (cpp/int. 5))    ; Calls int version
(cpp/example.process (cpp/float. 5))  ; Calls float version
```

## Lifetime Management

- Stack-allocated objects (created with constructors) are automatically destroyed when they go out of scope
- Heap-allocated objects (created with `cpp/new`) must be manually deleted with `cpp/delete`
- References obtained with `cpp/&` do not affect object lifetime

## Error Handling

The Jank compiler provides compile-time errors for:
- Invalid C++ code in `cpp/raw`
- Type mismatches in function calls
- Access to private/protected members
- Missing or ambiguous function overloads
- Incorrect constructor arguments

## Best Practices

1. **Namespace Organization**: Use consistent namespace patterns for C++ code embedded with `cpp/raw`
2. **Memory Management**: Be explicit about ownership - prefer stack allocation where possible
3. **Type Safety**: Use `cpp/type` for complex template instantiations
4. **Interop Boundaries**: Keep interop code localized to specific modules when possible
5. **Documentation**: Document the C++ APIs you're exposing to Jank code

## Examples

### Example 1: Using STL Containers
```clojure
(cpp/raw "#include <vector>")

(let [vec ((cpp/type "std::vector<int>"))
      _ (cpp/.push_back vec (cpp/int. 1))
      _ (cpp/.push_back vec (cpp/int. 2))
      size (cpp/.size vec)]
  size)  ; Returns 2
```

### Example 2: Working with Chrono
```clojure
(defn sleep [ms]
  (let [duration (cpp/std.chrono.milliseconds ms)]
    (cpp/std.this_thread.sleep_for duration)))
```

### Example 3: Custom Class with Operators
```clojure
(cpp/raw "namespace math {
            struct Vector2 {
              float x, y;
              Vector2 operator+(const Vector2& other) const {
                return {x + other.x, y + other.y};
              }
            };
          }")

(let [v1 (cpp/math.Vector2. (cpp/float. 1) (cpp/float. 2))
      v2 (cpp/math.Vector2. (cpp/float. 3) (cpp/float. 4))
      v3 (cpp/+ v1 v2)]
  [(cpp/.-x v3) (cpp/.-y v3)])  ; Returns [4.0 6.0]
```

## Implementation Details

The C++ interop system is implemented through:
- `cpp_raw` expression nodes in the AST (analyze/expr/cpp_raw.hpp)
- Analyzer processing of cpp/* forms (analyze/processor.cpp)
- Codegen that directly emits C++ code or LLVM IR
- Integration with CppInterOp library for runtime type information

## C Language Interop

While Jank's interop system uses the `cpp/` prefix, it fully supports C language features and idioms. Since C++ is largely a superset of C, you can work with pure C code seamlessly.

### Working with C Headers

Include C headers using `extern "C"` blocks to ensure proper linkage:

```clojure
(cpp/raw "#ifdef __cplusplus
          extern \"C\" {
          #endif
          #include <stdio.h>
          #include <stdlib.h>
          #include <string.h>
          #ifdef __cplusplus
          }
          #endif")
```

Or if the headers already have C++ guards:

```clojure
(cpp/raw "#include <math.h>
          #include <time.h>")
```

### C Functions

Call C standard library and custom C functions directly:

```clojure
; Standard library functions
(cpp/printf "Hello, %s!\n" "World")
(cpp/malloc 1024)
(cpp/strlen "test string")
(cpp/sqrt 16.0)

; Custom C functions
(cpp/raw "extern \"C\" {
            int add_numbers(int a, int b) {
              return a + b;
            }
          }")

(cpp/add_numbers 5 7)  ; Returns 12
```

### C Structs

Define and use C-style structs:

```clojure
(cpp/raw "extern \"C\" {
            typedef struct {
              int x;
              int y;
              char label[32];
            } Point;
            
            Point create_point(int x, int y, const char* label) {
              Point p;
              p.x = x;
              p.y = y;
              strncpy(p.label, label, 31);
              p.label[31] = '\\0';
              return p;
            }
          }")

(let [pt (cpp/create_point 10 20 "origin")]
  [(cpp/.-x pt) (cpp/.-y pt)])  ; Returns [10 20]
```

### C Arrays and Pointers

Work with C-style arrays and pointer arithmetic:

```clojure
(cpp/raw "extern \"C\" {
            int sum_array(int* arr, int size) {
              int sum = 0;
              for(int i = 0; i < size; i++) {
                sum += arr[i];
              }
              return sum;
            }
            
            void fill_buffer(char* buffer, size_t size) {
              for(size_t i = 0; i < size - 1; i++) {
                buffer[i] = 'A' + (i % 26);
              }
              buffer[size - 1] = '\\0';
            }
          }")

; Allocate and use C arrays
(let [buffer (cpp/malloc 256)]
  (cpp/fill_buffer buffer 256)
  ; ... use buffer ...
  (cpp/free buffer))
```

### C Enums

C enums work identically to C++ enums:

```clojure
(cpp/raw "extern \"C\" {
            enum FileMode {
              READ_ONLY = 0,
              WRITE_ONLY = 1,
              READ_WRITE = 2
            };
          }")

(let [mode cpp/READ_WRITE]
  (if (= mode 2)
    :read-write-mode))
```

### Function Pointers

Work with C function pointers:

```clojure
(cpp/raw "extern \"C\" {
            typedef int (*operation)(int, int);
            
            int apply_operation(operation op, int a, int b) {
              return op(a, b);
            }
            
            int multiply(int a, int b) {
              return a * b;
            }
          }")

(cpp/apply_operation cpp/multiply 6 7)  ; Returns 42
```

### C Macros

Use C preprocessor macros (they're expanded at compile time):

```clojure
(cpp/raw "#define MAX(a, b) ((a) > (b) ? (a) : (b))
          #define BUFFER_SIZE 1024
          #define DEBUG_MODE 1")

; Use macro constants
(let [size cpp/BUFFER_SIZE]  ; Gets the value 1024
  (cpp/malloc size))
```

### Memory Management

Use C memory management functions:

```clojure
; Allocate memory
(let [ptr (cpp/malloc 100)]
  ; Use the memory
  (cpp/memset ptr 0 100)
  ; Free when done
  (cpp/free ptr))

; Allocate zeroed memory
(let [arr (cpp/calloc 10 (cpp/sizeof cpp/int))]
  ; ... use array ...
  (cpp/free arr))

; Reallocate
(let [ptr (cpp/malloc 100)
      new-ptr (cpp/realloc ptr 200)]
  ; ... use new-ptr ...
  (cpp/free new-ptr))
```

### String Manipulation

Work with C strings:

```clojure
(cpp/raw "#include <string.h>")

(let [dest (cpp/malloc 256)
      src "Hello, C!"]
  (cpp/strcpy dest src)
  (cpp/strcat dest " From Jank")
  (let [len (cpp/strlen dest)]
    (cpp/free dest)
    len))  ; Returns length of concatenated string
```

### File I/O

Use C file operations:

```clojure
(cpp/raw "#include <stdio.h>")

(let [file (cpp/fopen "test.txt" "w")]
  (when (not= file cpp/nullptr)
    (cpp/fprintf file "Hello from Jank via C!\n")
    (cpp/fclose file)))

; Reading
(let [file (cpp/fopen "test.txt" "r")
      buffer (cpp/malloc 256)]
  (when (not= file cpp/nullptr)
    (cpp/fgets buffer 256 file)
    (cpp/fclose file)
    ; ... process buffer ...
    (cpp/free buffer)))
```

### Best Practices for C Interop

1. **Use `extern "C"`**: Always wrap C headers and functions in `extern "C"` blocks to prevent C++ name mangling
2. **Manual Memory Management**: Remember to `free` any memory allocated with `malloc`, `calloc`, or `realloc`
3. **Null Checks**: Always check for NULL/nullptr returns from C functions like `malloc` or `fopen`
4. **Buffer Sizes**: Be careful with buffer sizes and string termination when using C string functions
5. **Type Safety**: C is less type-safe than C++ - be extra careful with void pointers and casts
6. **Error Handling**: Check return codes from C functions (many return -1 or NULL on error)

### Example: Complete C Library Integration

```clojure
; Integrate with a C math library
(cpp/raw "extern \"C\" {
            #include <math.h>
            
            typedef struct {
              double real;
              double imag;
            } Complex;
            
            Complex complex_add(Complex a, Complex b) {
              Complex result;
              result.real = a.real + b.real;
              result.imag = a.imag + b.imag;
              return result;
            }
            
            double complex_magnitude(Complex c) {
              return sqrt(c.real * c.real + c.imag * c.imag);
            }
          }")

(defn complex-arithmetic []
  (let [c1 (cpp/Complex. 3.0 4.0)
        c2 (cpp/Complex. 1.0 2.0)
        sum (cpp/complex_add c1 c2)
        mag (cpp/complex_magnitude sum)]
    {:sum-real (cpp/.-real sum)
     :sum-imag (cpp/.-imag sum)
     :magnitude mag}))
```

## Limitations

- Template metaprogramming must be explicitly instantiated
- Some C++ features like SFINAE may require workarounds
- Virtual function calls work but dynamic dispatch follows C++ rules
- Exception handling integration is implementation-defined
- C variadic functions require careful type handling
- Some C macros that use token pasting or complex preprocessing may not work as expected