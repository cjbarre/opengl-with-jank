(require '[babashka.fs :as fs])

(println (str "jank-build::include-dir=" (fs/path (:src-dir *input*) "include")))
(println "jank-build::rerun-if-changed=jank-build.bb")
