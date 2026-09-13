# Linux JIT allocator symbol

| Environment | Version |
| --- | --- |
| jank | `aa421eef05d6394fe4d04f30e52da74312778813` (unmodified source) |
| OS | Ubuntu 24.04, x86_64 |
| LLVM | 23.1.2 |
| C++ standard library | libstdc++ 14 |
| Locale | `C.UTF-8` |

`probe.jank`:

```clojure
(ns probe)
(cpp/raw "#include <vector>
struct ProbeVertex { float x; ProbeVertex(float v) : x(v) {} };")

(defn vector-size []
  (let [vertices (#cpp (std.vector ProbeVertex))
        vertices-box (cpp/box (cpp/& vertices))]
    (doseq [i (range 3)]
      (let [v (cpp/unbox (:* (std.vector ProbeVertex)) vertices-box)]
        (cpp/.push_back v (cpp/ProbeVertex (cpp/float 1.0)))))
    (cpp/int (cpp/.size vertices))))

(defn -main [& _]
  (assert (= 3 (vector-size)))
  (println :pass))
```

```sh
jank run-main --module-path . probe
```

Append `template class std::allocator<ProbeVertex>;` inside the `cpp/raw` string.

| Code | Result |
| --- | --- |
| Before instantiation | Exit 1; missing symbol `_ZNSt15__new_allocatorI11ProbeVertexE10deallocateEPS0_m` |
| After instantiation | Prints `:pass`; exit 0 |

The first failing commit has not been determined. This comparison uses the same compiler for both runs.
