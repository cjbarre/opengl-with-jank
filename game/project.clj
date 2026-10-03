(defproject opengl-with-jank/game "0.1.0-SNAPSHOT"
  :description "Strafe Combat Academy."
  :license {:name "MIT"
            :url "https://opensource.org/license/mit"}
  :plugins [[org.jank-lang/lein-jank "2026.06-1"]]
  :middleware [leiningen.jank/middleware]
  :source-paths ["src" "../engine/src"]
  :main sca.baked
  :jank #=(load-file "lein-jank-config.clj")
  :profiles {:release {:jank {:target-dir "target/release"
                              :optimization-level 3}}})
