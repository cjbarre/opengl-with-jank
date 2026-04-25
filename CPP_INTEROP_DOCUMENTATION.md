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
(let [attribute (cpp/unbox (:* cgltf_attribute) attribute-box)]
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
; Allocate on heap - simple types
(cpp/new cpp/float (cpp/float. 0.0))
(cpp/new cpp/glm.vec3 vec-expr)

; Allocate template types using DSL
(cpp/new (std.vector float))
(cpp/new (std.vector glm.vec3))
(cpp/new (std.array glm.vec3 3))
(cpp/new ENetEvent)

; Allocate unsigned types
(cpp/new (:unsigned int))
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

### 6. C++ Type DSL

Jank has a keyword-based DSL for specifying C++ types. The DSL is automatically enabled in contexts that expect a type, such as the first argument to `cpp/new`, `cpp/unbox`, and `cpp/cast`.

#### Type DSL Forms

```clojure
; Pointer types
(:* int)                  ; int*
(:* void)                 ; void*
(:* (:const char))        ; char const*
(:* (:const void))        ; const void*
(:* GLFWwindow)           ; GLFWwindow*
(:* ENetHost)             ; ENetHost*
(:* (:* cgltf_node))      ; cgltf_node** (double pointer)

; Template instantiations
(std.vector float)        ; std::vector<float>
(std.vector int)          ; std::vector<int>
(std.vector Vertex)       ; std::vector<Vertex>
(std.array float 3)       ; std::array<float, 3>
(ozz.vector ozz.sample.Mesh) ; ozz::vector<ozz::sample::Mesh>

; Pointer to template type
(:* (std.vector float))   ; std::vector<float>*
(:* (std.vector Vertex))  ; std::vector<Vertex>*

; Qualifiers
(:unsigned int)           ; unsigned int
(:const char)             ; char const
```

#### Value Initialization with `#cpp`

The `#cpp` reader tag creates a default-initialized C++ value:

```clojure
; Create zero-initialized values (common for OpenGL IDs)
(#cpp (:unsigned int))           ; unsigned int, zero-initialized
(#cpp (std.vector Vertex))       ; std::vector<Vertex>, default-constructed
(#cpp (std.array float 3))       ; std::array<float, 3>

; Access C++ constants and macros
#cpp SEEK_END                    ; the C macro SEEK_END
#cpp true                        ; C++ true
#cpp false                       ; C++ false

; Construct with initial value
(#cpp (:* cgltf_data) cpp/nullptr)  ; cgltf_data* initialized to nullptr
```

#### Usage in `cpp/cast` and `cpp/unbox`

```clojure
; Casting
(cpp/cast (:* void) buffer)
(cpp/cast (:* (:const char)) s)
(cpp/cast (:* (:const void)) data)

; Unboxing
(cpp/unbox (:* GLFWwindow) window)
(cpp/unbox (:* FILE) file)
(cpp/unbox (:* ENetHost) host)
(cpp/unbox (:* (std.vector float)) verts-box)
(cpp/unbox (:* (ozz.vector ozz.sample.Mesh)) meshes)
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
; Unbox takes TWO arguments: type (DSL form) and boxed-value
(cpp/unbox (:* GLFWwindow) window)
(cpp/unbox (:* FILE) file)
(cpp/unbox (:* cgltf_attribute) attribute)
(cpp/unbox (:* cgltf_accessor) accessor-box)
(cpp/unbox (:* cgltf_node) node)
(cpp/unbox (:* (:* cgltf_node)) nodes)    ; double pointer
(cpp/unbox (:* cgltf_scene) scenes)
(cpp/unbox (:* float) ptr)
(cpp/unbox (:* glm.vec3) ptr)
(cpp/unbox (:* AnimationContext) context)

; For template pointer types
(cpp/unbox (:* (std.vector float)) verts-box)
(cpp/unbox (:* (std.vector Vertex)) vertices-box)
(cpp/unbox (:* (std.vector int)) indices-box)
(cpp/unbox (:* (ozz.vector ozz.sample.Mesh)) meshes)
```

**Note:** `cpp/unbox` always takes exactly two arguments - the type specification first, then the boxed value. The type uses the DSL syntax (see section 6).

### 8. Working with C++ Constants

#### `#cpp` Reader Tag - Access C++ Constants and Macros

Access C preprocessor macros and compile-time constants using the `#cpp` reader tag:

```clojure
; C standard library constants
#cpp SEEK_END
#cpp true
#cpp false
```

#### Inline Function Wrappers for Constants

This project wraps OpenGL/GLFW constants as inline C++ functions (defined in `engine.gl.constants`) to avoid symbol redefinition issues with `cpp/value` in AOT compilation:

```clojure
; In engine/gl/constants.jank:
(cpp/raw "inline int gl_triangles() { return GL_TRIANGLES; }")

; Usage throughout the codebase:
(require '[engine.gl.constants :as gl])
gl/GL_TRIANGLES
gl/GL_FLOAT
gl/GLFW_KEY_ESCAPE
```

#### `cpp/value` - Direct C++ Expression Evaluation

For one-off C++ constant access (prefer inline wrappers for frequently used values):

```clojure
(cpp/value "SEEK_END")
(cpp/value "std::numeric_limits<long long>::max()")
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
(cpp/free (cpp/cast (:* void) sources))
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
                  (cpp/cast (:* void)
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

**Locations:** `engine.shaders.core`, `engine.gfx3d.textures.core`, `engine.gfx3d.geometry.core`, `engine.gfx3d.gltf.core`, `engine.io.core`, `engine.math.core`, `engine.networking.core`, `examples.demo.core`

## Best Practices

1. **Helper Functions**: Create small C++ helper functions for repetitive type conversions and null checks
2. **Constants as Inline Functions**: Wrap frequently used constants in inline C++ functions (see `engine.gl.constants`)
3. **Boxing for Persistence**: Use `cpp/box` to store C++ pointers in Jank data structures
4. **DSL Types**: Use the type DSL for `cpp/new`, `cpp/unbox`, `cpp/cast` — e.g., `(:* void)`, `(std.vector float)`, `(#cpp (:unsigned int))`
5. **Memory Management**: Always free allocated memory with `cpp/free` when done
6. **Null Checks**: Check for `cpp/nullptr` after operations that can fail (malloc, fopen, etc.)
7. **cpp/raw Organization**: Group related C++ code together in `cpp/raw` blocks at the top of namespaces
8. **Name Collisions**: C++ functions in `cpp/raw` must not match jank `defn` names after hyphen-to-underscore munging. Use `_impl` or `_helper` suffixes for C++ functions that wrap jank functions of the same name
9. **Void Method Calls in let**: Void-returning `cpp/.method` calls bound to `_` in `let` bindings still need `(do (cpp/.method ...) nil)` wrapping
