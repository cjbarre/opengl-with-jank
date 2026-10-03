(defproject opengl-with-jank/engine "0.1.0-SNAPSHOT"
  :description "Reusable jank/OpenGL runtime for loading loose game source."
  :license {:name "MIT"
            :url "https://opensource.org/license/mit"}
  :plugins [[org.jank-lang/lein-jank "2026.06-1"]]
  :middleware [leiningen.jank/middleware]
  :source-paths ["src"]
  :main engine.runtime.core
  :jank #=(load-file "lein-jank-config.clj"))
