(require '[babashka.fs :as fs]
         '[babashka.process :as proc]
         '[clojure.string :as str]
         '[jank.build.cmake :as cmake]
         '[jank.build.pkg-config :refer [pkg-config]])

(let [{:keys [src-dir out-dir static?] :as input} *input*
      include-dir (str (fs/path out-dir "include"))
      lib-dir (str (fs/path out-dir "lib"))]
  (cmake/build (assoc input :src-dir (str (fs/path src-dir "native")))
               {:defines {"CMAKE_C_FLAGS" (str (System/getenv "CFLAGS") " -ffile-prefix-map=" src-dir "=/engine")
                          "CMAKE_CXX_FLAGS" (str (System/getenv "CXXFLAGS") " -ffile-prefix-map=" src-dir "=/engine")}})
  (proc/shell "bb" (str (fs/path src-dir "scripts/embed-assets.clj"))
              (str (fs/path include-dir "engine_assets.h"))
              (str "shaders:" (fs/path src-dir "assets/shaders"))
              (str "fonts:" (fs/path src-dir "assets/fonts")))
  (println (str "jank-build::include-dir=" include-dir))
  (println (str "jank-build::link-dir=" lib-dir))
  (doseq [lib ["ozz_animation" "ozz_geometry" "ozz_base" "stb_all" "enet" "cgltf"]]
    (println (str (if static? "jank-build::link-static-library=" "jank-build::link-library=") lib)))
  (when (= "linux" (str/lower-case (System/getProperty "os.name")))
    (pkg-config input "glew"))
  (doseq [path ["jank-build.bb" "native" "include" "libs/glm/glm" "assets"
                "scripts/embed-assets.clj" "third_party/ozz-animation/CMakeLists.txt"
                "third_party/ozz-animation/build-utils" "third_party/ozz-animation/src"
                "third_party/ozz-animation/include"]]
    (println (str "jank-build::rerun-if-changed=" path)))
  (doseq [env ["CC" "CXX" "CFLAGS" "CXXFLAGS" "SDKROOT"]]
    (println (str "jank-build::rerun-if-env-changed=" env))))
