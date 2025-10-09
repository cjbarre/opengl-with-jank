# Jank C++ Interop Documentation

## Overview

Jank provides seamless C++ interoperability, allowing you to directly use C++ code, types, and functions from within Jank programs. This documentation covers only the interop patterns actually used in this codebase.

## Core Interop Forms

### 1. `cpp/raw` - Embedding Raw C++ Code

The `cpp/raw` form allows you to embed raw C++ code directly into your Jank program:

```clojure
(cpp/raw "#include <GLFW/glfw3.h>")

(cpp/raw "#include <glm/glm.hpp>
          #include <glm/gtc/matrix_transform.hpp>
          #include <glm/gtc/type_ptr.hpp>")

(cpp/raw
 "void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
      (void) window;
      glViewport(0, 0, width, height);
    }")
```

**Usage patterns:**
- Include C/C++ headers
- Define C functions and structs
- Declare global variables and constants
- Define helper functions for type conversions

### 2. Accessing C++ Values

#### Global Functions and Constants

Access C++ global functions and constants using the `cpp/` prefix with dot notation:

```clojure
; Call C functions
(cpp/glfwInit)
(cpp/glfwCreateWindow width height name cpp/nullptr cpp/nullptr)
(cpp/glGetUniformLocation shader "view")

; Call C++ namespace functions
(cpp/glm.vec3 (cpp/float. 0.0) (cpp/float. 1.0) (cpp/float. 0.0))
(cpp/glm.lookAt camera-pos target up)
(cpp/glm.radians (cpp/float. 45.0))

; Access global variables defined in cpp/raw
cpp/textured_rectangle_2D_vertices
cpp/float_size
```

#### Member Access with `cpp/.-`

Access instance members (fields) of C++ objects:

```clojure
(let [attribute (cpp/unbox cpp/cgltf_attribute* attribute-box)]
  (cpp/.-type attribute)
  (cpp/.-data attribute)
  (cpp/.-count attribute))
```

#### Method Calls with `cpp/.`

Call methods on C++ objects:

```clojure
; Call methods on C++ containers
(cpp/.push_back vertices vertex-data)
(cpp/.size vertices)
(cpp/.data array)
```

### 3. Creating C++ Objects

#### Stack-Allocated Objects (Constructors)

Create C++ objects on the stack using constructor syntax with a trailing dot:

```clojure
; Built-in types
(cpp/int)        ; Default initialized int
(cpp/int. 42)    ; int with value 42
(cpp/float)      ; Default initialized float
(cpp/float. 3.14) ; float with value 3.14
(cpp/double)
(cpp/double. 2.718)
(cpp/char)
(cpp/char. 65)   ; char 'A'

; C++ library types
(cpp/glm.vec3 (cpp/float. 0.0) (cpp/float. 1.0) (cpp/float. 0.0))
(cpp/cgltf_options)
```

#### Heap-Allocated Objects with `cpp/new`

Allocate objects on the heap, returning a pointer:

```clojure
; Allocate on heap
(cpp/new cpp/float (cpp/float. 0.0))
(cpp/new cpp/glm.vec3 vec-expr)
(cpp/new (cpp/type "std::array<glm::vec3, 3>"))
```

**Note:** This codebase does not use `cpp/delete`. Objects are managed through boxing and Jank's lifecycle.

### 4. C++ Operators

#### Arithmetic Operators
```clojure
(cpp/+ a b)   ; Addition
(cpp/- a b)   ; Subtraction
(cpp/* ptr)   ; Dereference (unary)
(cpp/* a b)   ; Multiplication (binary)
(cpp// a b)   ; Division
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
(cpp/| a b)   ; Bitwise OR
(cpp/& value) ; Address-of (unary)
```

#### Assignment
```clojure
(cpp/= target value)  ; Assignment (sets target to value, returns void)
```

### 5. Array Operations

#### `cpp/aget` - Array Element Access

Access elements of C++ arrays or array-like structures:

```clojure
; Access array elements by index
(cpp/aget array (cpp/int 0))
(cpp/aget vertices (cpp/int i))
(cpp/aget direction (cpp/int 2))

; Access translation array from cgltf
(let [translation (cpp/get_node_translation node)]
  [(cpp/aget translation (cpp/int 0))
   (cpp/aget translation (cpp/int 1))
   (cpp/aget translation (cpp/int 2))])
```

#### `aset` - Array Element Assignment

Set elements of arrays:

```clojure
; Set array element
(aset array (cpp/int index) value)

; Example from math.core.jank
(aset (cpp/* arr-sym) (cpp/int idx)
      (cpp/glm.vec3 (cpp/float x) (cpp/float y) (cpp/float z)))

; Example from io.core.jank
(aset (cpp/charify_void buffer) (cpp/int bytes-read) (cpp/char 0))
```

### 6. Type Specifications with `cpp/type`

Explicitly specify C++ types, especially useful for templates and complex type names:

```clojure
; Create instances of templated types
((cpp/type "std::vector<int>"))
((cpp/type "std::vector<Vertex>"))
((cpp/type "std::array<float, 3>"))
((cpp/type "std::array<glm::vec3, 3>"))

; Create unsigned int (common in OpenGL)
((cpp/type "unsigned int"))

; Use for casting and type specifications
(cpp/cast (cpp/type "void*") buffer)
(cpp/unbox (cpp/type "void*") boxed-buffer)
(cpp/unbox (cpp/type "std::vector<Vertex>*") vertices-box)
(cpp/unbox (cpp/type "std::vector<int>*") indices-box)
```

### 7. Boxing and Unboxing

Convert between Jank's boxed types and C++ native types:

#### `cpp/box` - Convert C++ Value to Jank Object
```clojure
(cpp/box pointer)
(cpp/box (cpp/new cpp/float (cpp/float. 0.0)))
(cpp/box (cpp/& accessor))
(cpp/box (cpp/.-primitives mesh))
```

#### `cpp/unbox` - Extract C++ Value from Jank Object

```clojure
; Unbox takes TWO arguments: type and boxed-value
(cpp/unbox cpp/GLFWwindow* window)
(cpp/unbox cpp/FILE* file)
(cpp/unbox cpp/cgltf_attribute* attribute)
(cpp/unbox cpp/cgltf_accessor* accessor-box)
(cpp/unbox cpp/cgltf_node* node)
(cpp/unbox cpp/cgltf_node** nodes)
(cpp/unbox cpp/cgltf_scene* scenes)
(cpp/unbox cpp/float* ptr)
(cpp/unbox cpp/glm.vec3* ptr)

; For complex types, use cpp/type for the first argument
(cpp/unbox (cpp/type "void*") buffer)
(cpp/unbox (cpp/type "std::vector<Vertex>*") vertices-box)
(cpp/unbox (cpp/type "std::vector<int>*") indices-box)
```

**Note:** `cpp/unbox` always takes exactly two arguments - the type specification first, then the boxed value.

### 8. Working with C++ Constants

#### `cpp/value` - Access C++ Constants and Macro Values

Access C preprocessor macros and compile-time constants:

```clojure
; OpenGL constants
(cpp/value "GL_TRIANGLES")
(cpp/value "GL_FLOAT")
(cpp/value "GL_TEXTURE_2D")
(cpp/value "GL_DEPTH_TEST")
(cpp/value "GL_COLOR_BUFFER_BIT")

; GLFW constants
(cpp/value "GLFW_PRESS")
(cpp/value "GLFW_KEY_ESCAPE")
(cpp/value "GLFW_KEY_W")
(cpp/value "GLFW_CONTEXT_VERSION_MAJOR")
(cpp/value "GLFW_OPENGL_CORE_PROFILE")
(cpp/value "GLFW_CURSOR")

; C standard library constants
(cpp/value "SEEK_END")

; Boolean/Special constants
(cpp/value "GL_TRUE")
(cpp/value "GL_FALSE")
```

**Common Pattern:** Bind frequently used constants to Clojure defs:
```clojure
(def GLFW_PRESS (cpp/value "GLFW_PRESS"))
(def GLFW_KEY_ESCAPE (cpp/value "GLFW_KEY_ESCAPE"))
(def GL_TRIANGLES (cpp/value "GL_TRIANGLES"))
```

#### `cpp/nullptr` - Null Pointer Constant

```clojure
cpp/nullptr  ; C++ nullptr value

; Common usage
(cpp/glfwCreateWindow width height name cpp/nullptr cpp/nullptr)
(when (cpp/! window)  ; Check for nullptr
  (println "Window creation failed"))
```

#### `cpp/false` - Boolean Constant

```clojure
cpp/false  ; C++ false boolean constant

; Common usage
(when (cpp/!= cpp/false (cpp/.-has_translation node))
  (assoc :translation [...]))
```

## Memory Management

### Using malloc and free

```clojure
(cpp/raw "#include <stdlib.h>")

; Allocate memory
(let [buffer (cpp/malloc size)]
  (when (cpp/! buffer)
    (cpp/perror "malloc"))
  ; ... use buffer ...
  (cpp/free buffer))

; From shaders/core.jank
(cpp/free source)
(cpp/free (cpp/cast (cpp/type "void*") sources))
```

## Common Patterns

### C++ Helper Functions for Type Conversions

Define small C++ helper functions in `cpp/raw` blocks to handle type conversions:

```clojure
; From geometry/core.jank
(cpp/raw
 "void* voidify_int (int i) {
    return (void*) i;
  }")

; From io/core.jank
(cpp/raw
 "char* charify_void (void* buffer) {
    return static_cast<char*>(buffer);
  }")

; From textures/core.jank
(cpp/raw
 "void* voidify_stbi_uc (stbi_uc* x) {
    return (void*) x;
  }")

; Usage
(cpp/voidify_int (cpp/int 0))
(cpp/charify_void buffer)
(cpp/voidify_stbi_uc data)
```

### Accessing Struct Fields and Nested Data

Helper functions for cleaner null checks and field access:

```clojure
; From gltf/core.jank
(cpp/raw
 "cgltf_float* get_node_translation (cgltf_node* node) {
    return node->translation;
  }

  cgltf_float* get_node_scale (cgltf_node* node) {
    return node->scale;
  }

  bool cgltf_primitive_has_material(cgltf_primitive* prim) {
    return (prim->material != nullptr);
  }")

; Usage
(when (cpp/cgltf_primitive_has_material primitive)
  (let [translation (cpp/get_node_translation node)]
    ...))
```

### Creating Constant Arrays in C++

For geometry data and other large constant arrays:

```clojure
; From geometry/core.jank
(cpp/raw
 "float textured_rectangle_2D_vertices[] = {
    0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
    // ...
  };

  unsigned int textured_rectangle_2D_indices [] = {
    0, 1, 3,
    1, 2, 3
  };

  size_t textured_rectangle_2D_vertices_size = sizeof(textured_rectangle_2D_vertices);
  size_t textured_rectangle_2D_indices_size = sizeof(textured_rectangle_2D_indices);
  size_t float_size = sizeof(float);")

; Access from Jank
(cpp/glBufferData GL_ARRAY_BUFFER
                  cpp/textured_rectangle_2D_vertices_size
                  (cpp/cast (cpp/type "void*")
                            cpp/textured_rectangle_2D_vertices)
                  GL_STATIC_DRAW)
```

### Helper Functions for Custom Operations

```clojure
; From math/core.jank - Identity matrix helper
(cpp/raw
 "glm::mat4 identity_matrix () {
    return glm::mat4(1.0f);
  }")

(let [model (cpp/identity_matrix)]
  ...)

; From shaders/core.jank - Creating C string arrays
(cpp/raw
 "char **sources(void* source) {
    char **arr = (char **) malloc(sizeof(char *));
    arr[0] = (char*) source;
    return arr;
  }")
```

## Usage Examples from This Codebase

**Locations:** `app.core`, `app.shaders.core`, `app.textures.core`, `app.geometry.core`, `app.gltf.core`, `app.io.core`, `app.math.core`

## Best Practices

1. **Helper Functions**: Create small C++ helper functions for repetitive type conversions and null checks
2. **Constants as Defs**: Bind frequently used `cpp/value` constants to Clojure `def` symbols for readability
3. **Boxing for Persistence**: Use `cpp/box` to store C++ pointers in Jank data structures
4. **Explicit Types**: Use `cpp/type` for template instantiations and unsigned types
5. **Memory Management**: Always free allocated memory with `cpp/free` when done
6. **Null Checks**: Check for `cpp/nullptr` after operations that can fail (malloc, fopen, etc.)
7. **cpp/raw Organization**: Group related C++ code together in `cpp/raw` blocks at the top of namespaces
