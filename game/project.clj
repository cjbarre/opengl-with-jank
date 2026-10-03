(defproject opengl-with-jank/game "0.1.0-SNAPSHOT"
  :description "Strafe Combat Academy."
  :license {:name "MIT"
            :url "https://opensource.org/license/mit"}
  :plugins [[org.jank-lang/lein-jank "2026.09-7"]]
  :middleware [leiningen.jank/middleware]
  :dependencies [[opengl-with-jank/engine "0.1.0-SNAPSHOT"]]
  :source-paths ["src"]
  :main sca.core
  :jank {:name "sca"
         :target-dir "target/debug"
         :optimization-level 0
         :runtime :static
         :static? false}
  :profiles {:release {:jank {:target-dir "target/release"
                              :optimization-level 3}}})
