(require 'clojure.string)

(let [detect-platform (fn []
                        (let [os (System/getProperty "os.name")
                              arch (System/getProperty "os.arch")]
                          (cond
                            (.startsWith os "Mac") (str "macos-" (if (= arch "aarch64") "arm64" arch))
                            (.startsWith os "Linux") (str "linux-" (case arch
                                                                     "amd64" "x86_64"
                                                                     "aarch64" "arm64"
                                                                     arch))
                            :else (throw (ex-info (str "Unsupported platform: " os "/" arch) {})))))
      platform (or (System/getenv "PLATFORM_OVERRIDE") (detect-platform))
      platform-os (first (clojure.string/split platform #"-"))
      libs-dir (str "../engine/libs/" platform)
      glfw-lib (if (= platform-os "macos") "glfw.3" "glfw")
      opengl-libs (if (= platform-os "macos") ["OpenGL"] ["GL" "GLEW"])
      optimization-level (some-> (System/getenv "JANK_OPTIMIZATION_LEVEL")
                                 Integer/parseInt)
      runtime (keyword (or (System/getenv "JANK_RUNTIME") "static"))]
  {:name (or (System/getenv "JANK_NAME") "sca")
   :target-dir (or (System/getenv "JANK_TARGET_DIR") "target/debug")
   :optimization-level (or optimization-level 0)
   :runtime runtime
   :include-dirs [(str libs-dir "/glfw/include")
                  "../engine/libs/glm"
                  "../engine/include"
                  (str libs-dir "/ozz-animation/include")
                  "include"]
   :library-dirs [(str libs-dir "/glfw/lib")
                  (str libs-dir "/ozz-animation/lib")
                  (str libs-dir "/stb/lib")
                  (str libs-dir "/enet/lib")
                  (str libs-dir "/cgltf/lib")
                  (str libs-dir "/opengl/lib")
                  (str libs-dir "/engine-assets/lib")]
   :linked-libraries (vec (concat [glfw-lib
                                   "ozz_animation_r"
                                   "ozz_base_r"
                                   "ozz_geometry_r"
                                   "stb_all"
                                   "enet"
                                   "cgltf"
                                   "engine_assets"]
                                  opengl-libs))})
