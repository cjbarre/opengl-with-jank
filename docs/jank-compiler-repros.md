# jank compiler repros

Each comparison uses adjacent, unmodified jank commits and the same repro code.

## `case` returning strings

| | Commit | Date | Change |
| --- | --- | --- | --- |
| Last working | [7b29eef2a](https://github.com/jank-lang/jank/commit/7b29eef2a5fed7ce38234fc631dc359dcf09694b) | July 22, 2026 | |
| First broken | [12637c9e0](https://github.com/jank-lang/jank/commit/12637c9e03bfc1b78f14020489fb4a6b999a120b) | July 25, 2026 | Switch to native literals by default |

`case_string.jank`:

```clojure
(ns case-string)

(defn choose [x]
  (case x :a "A" :b "B" "default"))

(defn -main [& _]
  (assert (= ["A" "B" "default"] (mapv choose [:a :b :other])))
  (println :pass))
```

| | Commit | Result |
| --- | --- | --- |
| Before | `7b29eef2a` | `run-main` and the compiled executable print `:pass` and exit 0. |
| After | `12637c9e0` | `run-main` and `compile` exit 1: `error: no viable overloaded '='`. |

The generated code tries to assign a C++ string pointer to a jank object reference.

## Conditional throws with a native local

| | Commit | Date | Change |
| --- | --- | --- | --- |
| Last working | [7d185b25](https://github.com/jank-lang/jank/commit/7d185b25dacce7950bf7194830aad90c5b1bb6aa) | July 15, 2026 | |
| First broken | [29b32e63](https://github.com/jank-lang/jank/commit/29b32e636438e3f230684bcd6646df4be534ef65) | July 18, 2026 | Add proper scoping into C++ generation |

`scoped_throw.jank`:

```clojure
(ns scoped-throw)

(defn choose [x]
  (let [v (cpp/int 1)]
    (cond-> {:v v}
      x (assoc :a (throw (ex-info "unsupported" {})))
      x (assoc :b (throw (ex-info "unsupported" {}))))))

(defn -main [& _]
  (assert (= {:v 1} (choose false)))
  (println :pass))
```

| | Commit | Result |
| --- | --- | --- |
| Before | `7d185b25` | `run-main` and the compiled executable print `:pass` and exit 0. |
| After | `29b32e63` | `run-main` and `compile` crash with `SIGSEGV`; no executable is produced. |
