(defproject opengl-with-jank/engine "0.1.0-SNAPSHOT"
  :description "Reusable jank/OpenGL engine library."
  :license {:name "MIT"
            :url "https://opensource.org/license/mit"}
  :plugins [[org.jank-lang/lein-jank "2026.09-7"]]
  :middleware [leiningen.jank/middleware]
  :dependencies [[org.jank-lang.commons/gl-sys "2026.09-4"]
                 [org.jank-lang.commons/glfw-sys "2026.09-4"]]
  :build-dependencies [[org.jank-lang.commons/jank-build-cmake "2026.09-2"]
                       [org.jank-lang.commons/jank-build-pkg-config "2026.09-3"]]
  :verbatim-paths ["native" "include" "libs/glm/glm" "assets"
                   "scripts/embed-assets.clj"
                   "third_party/ozz-animation/CMakeLists.txt"
                   "third_party/ozz-animation/build-utils"
                   "third_party/ozz-animation/src"
                   "third_party/ozz-animation/include"
                   "third_party/ozz-animation/CHANGES.md"
                   "third_party/ozz-animation/LICENSE.md"
                   "third_party/ozz-animation/README.md"]
  :source-paths ["src"])
