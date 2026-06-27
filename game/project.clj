(defproject opengl-with-jank/game "0.1.0-SNAPSHOT"
  :description "Strafe Combat Academy."
  :license {:name "MPL 2.0"
            :url "https://www.mozilla.org/en-US/MPL/2.0/"}
  :plugins [[org.jank-lang/lein-jank "2026.06-1"]]
  :middleware [leiningen.jank/middleware]
  :source-paths ["src" "../engine/src"]
  :main sca.core
  :jank #=(load-file "lein-jank-config.clj")
  :profiles {:release {:jank {:target-dir "target/release"
                              :optimization-level 3}}})
