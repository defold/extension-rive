;
; MIT License
; Copyright (c) 2021 Defold
; Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
; The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
;

(ns editor.rive
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.math :as math]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex :as vtx]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.render :as render]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.validation :as validation]
            [editor.gl.pass :as pass]
            [editor.types :as types]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.system :as system]
            [service.log :as log]
            [internal.util :as util])
  (:import [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]
           [java.lang.invoke MethodType MethodHandles]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]
           [com.jogamp.opengl GL GL2]
           [org.apache.commons.io IOUtils]
           [java.io IOException]
           [javax.vecmath Matrix4d Point2d Point3d Quat4d Vector3d Vector4d Tuple3d Tuple4d]))


(set! *warn-on-reflection* true)

(def rive-file-icon "/defold-rive/editor/resources/icons/32/Icons_17-Rive-file.png")
(def rive-scene-icon "/defold-rive/editor/resources/icons/32/Icons_16-Rive-scene.png")
(def rive-model-icon "/defold-rive/editor/resources/icons/32/Icons_15-Rive-model.png")
(def rive-bone-icon "/defold-rive/editor/resources/icons/32/Icons_18-Rive-bone.png")

(def rive-file-ext "riv")
(def rive-scene-ext "rivescene")
(def rive-model-ext "rivemodel")

; Plugin functions (from Rive.java)

;; (defn- debug-cls [^Class cls]
;;   (doseq [m (.getMethods cls)]
;;     (prn (.toString m))
;;     (println "Method Name: " (.getName m) "(" (.getParameterTypes m) ")")
;;     (println "Return Type: " (.getReturnType m) "\n")))

; More about JNA + Clojure
; https://nakkaya.com/2009/11/16/java-native-access-from-clojure/

(def rive-plugin-cls (workspace/load-class! "com.dynamo.bob.pipeline.Rive"))
(def rive-plugin-pointer-cls (workspace/load-class! "com.dynamo.bob.pipeline.Rive$RivePointer"))
(def rive-plugin-aabb-cls (workspace/load-class! "com.dynamo.bob.pipeline.Rive$AABB"))
(def byte-array-cls (Class/forName "[B"))
(def float-array-cls (Class/forName "[F"))

(defn- plugin-invoke-static [^Class cls name types args]
  (let [method (.getMethod cls name types)]
    (.invoke method nil (into-array Object args))))

(defn- plugin-load-file [bytes path]
  (plugin-invoke-static rive-plugin-cls "RIVE_LoadFileFromBuffer" (into-array Class [byte-array-cls String]) [bytes path]))

(defn- plugin-get-num-animations [handle]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetNumAnimations" (into-array Class [rive-plugin-pointer-cls]) [handle]))

(defn- plugin-get-animation ^String [handle index]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetAnimation" (into-array Class [rive-plugin-pointer-cls Integer/TYPE]) [handle (int index)]))

;(defn- plugin-get-aabb ^"com.dynamo.bob.pipeline.Rive$AABB" [handle]
(defn- plugin-get-aabb [handle]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetAABB" (into-array Class [rive-plugin-pointer-cls]) [handle]))

(defn- plugin-get-vertex-size ^double []
  (plugin-invoke-static rive-plugin-cls "RIVE_GetVertexSize" (into-array Class []) []))

(defn- plugin-update-vertices [handle dt]
  (plugin-invoke-static rive-plugin-cls "RIVE_UpdateVertices" (into-array Class [rive-plugin-pointer-cls Float/TYPE]) [handle (float dt)]))

(defn- plugin-get-vertex-count [handle]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetVertexCount" (into-array Class [rive-plugin-pointer-cls]) [handle]))

(defn- plugin-get-vertices [handle buffer]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetVertices" (into-array Class [rive-plugin-pointer-cls float-array-cls]) [handle buffer]))

;(defn- plugin-get-bones ^"[Lcom.dynamo.bob.pipeline.Rive$Bone;" [handle]
(defn- plugin-get-bones [handle]
  (plugin-invoke-static rive-plugin-cls "RIVE_GetBones" (into-array Class [rive-plugin-pointer-cls]) [handle]))


; .rivemodel
(defn load-rive-model [project self resource content]
  (let [rive-scene-resource (workspace/resolve-resource resource (:scene content))
        material (workspace/resolve-resource resource (:material content))]
        ;atlas          (workspace/resolve-resource resource (:atlas rivescene))

    (concat
     (g/connect project :default-tex-params self :default-tex-params)
     (g/set-property self
                     :rive-scene rive-scene-resource
                     :material material
                     :default-animation (:default-animation content)))))



(g/defnk produce-transform [position rotation scale]
  (math/->mat4-non-uniform (Vector3d. (double-array position))
                           (math/euler-z->quat rotation)
                           (Vector3d. (double-array scale))))

(g/defnode RiveBone
  (inherits outline/OutlineNode)
  (property name g/Str (dynamic read-only? (g/constantly true)))
  (property position types/Vec3
            (dynamic edit-type (g/constantly (properties/vec3->vec2 0.0)))
            (dynamic read-only? (g/constantly true)))
  (property rotation g/Num (dynamic read-only? (g/constantly true)))
  (property scale types/Vec3
            (dynamic edit-type (g/constantly (properties/vec3->vec2 1.0)))
            (dynamic read-only? (g/constantly true)))
  (property length g/Num
            (dynamic read-only? (g/constantly true)))

  (input nodes g/Any :array)
  (input child-bones g/Any :array)
  (input child-outlines g/Any :array)

  (output transform Matrix4d :cached produce-transform)
  (output bone g/Any (g/fnk [name transform child-bones]
                            {:name name
                             :local-transform transform
                             :children child-bones}))
  (output node-outline outline/OutlineData (g/fnk [_node-id name child-outlines]
                                                  {:node-id _node-id
                                                   :node-outline-key name
                                                   :label name
                                                   :icon rive-bone-icon
                                                   :children child-outlines
                                                   :read-only true})))

(defn- resource->bytes [resource]
  (with-open [in (io/input-stream resource)]
    (IOUtils/toByteArray in)))

(defn- build-rive-file
  [resource dep-resources user-data]
  {:resource resource :content (resource->bytes (:resource resource))})

(g/defnk produce-rive-file-build-targets [_node-id resource]
  (try
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-rive-file
        :user-data {:content-hash (resource/resource->sha1-hex resource)}})]
    (catch IOException e
      (g/->error _node-id :resource :fatal resource (format "Couldn't read rive file %s" (resource/resource->proj-path resource))))))

(g/defnk produce-rive-file-content [_node-id resource]
  (when resource
    (resource->bytes resource)))

(g/defnode RiveFileNode
  (inherits resource-node/ResourceNode)

  (input scene-structure g/Any)
  (output scene-structure g/Any (gu/passthrough scene-structure))

  (property content g/Any)
  (property rive-handle g/Any) ; The cpp pointer
  (property animations g/Any)
  (property aabb g/Any)
  (property vertices g/Any)
  (property bones g/Any)

  (input child-bones g/Any :array)

  (output build-targets g/Any :cached produce-rive-file-build-targets))

(defn- get-animations [handle]
  (let [num-animations (plugin-get-num-animations handle)
        indices (range num-animations)
        animations (map (fn [index] (plugin-get-animation handle index)) indices)]
    animations))

(defn- get-aabb [handle]
  (let [paabb (plugin-get-aabb handle)
        aabb (geom/coords->aabb [(.-minX paabb) (.-minY paabb) 0] [(.-maxX paabb) (.-maxY paabb) 0])]
    aabb))

(defn- rive-file->vertices [rive-handle dt]
  (let [vtx-size-bytes (plugin-get-vertex-size) ; size in bytes
        vtx-size (int (/ vtx-size-bytes 4)) ; number of floats per vertex
        _ (plugin-update-vertices rive-handle dt)
        vtx-count (plugin-get-vertex-count rive-handle)
        vtx-buffer (float-array (* vtx-size vtx-count))
        _ (plugin-get-vertices rive-handle vtx-buffer)
        vertices (partition vtx-size (vec vtx-buffer))]
    vertices))

; Creates the bone hierarcy
(defn- is-root-bone? [bone]
  (= -1 (.-parent bone)))

(defn- create-bone [project parent-id rive-bone]
  (let [name (.-name rive-bone)
        x (.-posX rive-bone)
        y (.-posY rive-bone)
        rotation (.-rotation rive-bone)
        scale-x (.-scaleX rive-bone)
        scale-y (.-scaleY rive-bone)
        length (.-length rive-bone)
        parent-graph-id (g/node-id->graph-id parent-id)
        bone-tx-data (g/make-nodes parent-graph-id [bone [RiveBone :name name :position [x y 0] :rotation rotation :scale [scale-x scale-y 1.0] :length length]]
                                   ; Hook this node into the parent's lists
                                   (g/connect bone :_node-id parent-id :nodes)
                                   (g/connect bone :node-outline parent-id :child-outlines)
                                   (g/connect bone :bone parent-id :child-bones))]
    bone-tx-data))

(defn- tx-first-created [tx-data]
  (get-in (first tx-data) [:node :_node-id]))

(defn- create-bone-hierarchy [project parent-id bones bone]
  (let [bone-tx-data (create-bone project parent-id bone)
        bone-id (tx-first-created bone-tx-data)
        child-bones (map (fn [index] (get bones index)) (.-children bone))
        children-tx-data (mapcat (fn [child] (create-bone-hierarchy project bone-id bones child)) child-bones)]
    (concat bone-tx-data children-tx-data)))

(defn- create-bones [project parent-id bones]
  (let [root-bones (filter is-root-bone? bones)
        tx-data (mapcat (fn [bone] (create-bone-hierarchy project parent-id bones bone)) root-bones)]
    tx-data))


; Loads the .riv file
(defn- load-rive-file
  [project node-id resource]
  (let [content (resource->bytes resource)
        rive-handle (plugin-load-file content (resource/resource->proj-path resource))
        animations (get-animations rive-handle)
        aabb (get-aabb rive-handle)
        vertices (rive-file->vertices rive-handle 0.0)
        bones (plugin-get-bones rive-handle)

        tx-data (concat
                 (g/set-property node-id :content content)
                 (g/set-property node-id :rive-handle rive-handle)
                 (g/set-property node-id :animations animations)
                 (g/set-property node-id :aabb aabb)
                 (g/set-property node-id :vertices vertices)
                 (g/set-property node-id :bones bones))

        all-tx-data (concat tx-data (create-bones project node-id bones))]
    all-tx-data))

; Rive Scene
; .rivescene (The "data" file) which in turn points to the .riv file

; Node defs
(g/defnk produce-rivescene-save-value [rive-file-resource]
  (prn "RIVE" "produce-rivescene-save-value" rive-file-resource)
  {:scene (resource/resource->proj-path rive-file-resource)})
   ;:atlas (resource/resource->proj-path atlas-resource)


(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

; The properties of the .rivescene (see RiveSceneDesc in rive_ddf.proto)
; The "scene" should point to a .riv file

; (defn- validate-scene-atlas [_node-id atlas]
;   (prop-resource-error :fatal _node-id :atlas atlas "Atlas"))

(defn- validate-rivescene-riv-file [_node-id rive-file]
  ;(prn "RIVE" "validate-rivescene-riv-file" rive-file)
  (prop-resource-error :fatal _node-id :scene rive-file "Riv File"))

(g/defnk produce-rivescene-own-build-errors [_node-id rive-file]
  (g/package-errors _node-id
                    ;(validate-scene-atlas _node-id atlas)
                    (validate-rivescene-riv-file _node-id rive-file)))

(defn- build-rive-scene [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveSceneDesc") pb)}))


(g/defnk produce-rivescene-build-targets
  [_node-id own-build-errors resource rive-scene-pb rive-file dep-build-targets]
  (g/precluding-errors own-build-errors
                       (let [dep-build-targets (flatten dep-build-targets)
                             deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
                             dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:scene rive-file]])]
                         [(bt/with-content-hash
                            {:node-id _node-id
                             :resource (workspace/make-build-resource resource)
                             :build-fn build-rive-scene
                             :user-data {:proto-msg rive-scene-pb
                                         :dep-resources dep-resources}
                             :deps dep-build-targets})])))

(g/defnk produce-rivescene-pb [_node-id rive-file-resource]
  {:scene (resource/resource->proj-path rive-file-resource)})

(defn- transform-vertices [^Matrix4d transform mesh]
  (let [p (Point3d.)
        vertices (:vertices mesh)
        transformed (map (fn [vert]
                           (let [[x y z  u v  r g b a] vert]
                             (.set p x y z)
                             (.transform transform p)
                             [(.x p) (.y p) (.z p) u v r g b a])) vertices)]
    (assoc mesh :vertices transformed)))

(defn- renderable->mesh [renderable]
  (let [handle (get-in renderable [:user-data :rive-file-handle])
        vertices (rive-file->vertices handle 0.0)
        mesh {:vertices vertices :indices nil}
        mesh (transform-vertices (:world-transform renderable) mesh)]
    mesh))

(defn generate-vertex-buffer [renderables]
  (let [meshes (map renderable->mesh renderables)
        vcount (reduce + 0 (map (fn [mesh] (count (:vertices mesh))) meshes))]
    (when (> vcount 0)
      ; vertices are vec3-vec2-vec4
      (let [vb (render/->vtx-pos-tex-col vcount)
            verts (into [] (reduce concat (map (fn [mesh] (map vec (:vertices mesh))) meshes)))]
        ; verts should be in the format [[x y x u v r g b a] [x y x u v r g b a] ...]
        (persistent! (reduce conj! vb verts))))))

; TODO: Load shader resources by name from the extension!

(shader/defshader rive-id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader rive-id-fragment-shader
  (varying vec2 var_texcoord0)
  ;(uniform sampler2D texture_sampler)
  (uniform vec4 id)
  (defn void main []
    (setq gl_FragColor id)))
    ;; (setq vec4 color (texture2D texture_sampler var_texcoord0.xy))
    ;; (if (> color.a 0.05)
    ;;   (setq gl_FragColor id)
    ;;   (discard))


(def rive-id-shader (shader/make-shader ::id-shader rive-id-vertex-shader rive-id-fragment-shader {"id" :id}))

(shader/defshader rive-shader-ver-tex-col
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader rive-shader-frag-tint
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def rive-shader-tint (shader/make-shader ::shader rive-shader-ver-tex-col rive-shader-frag-tint))


(defn- render-rive-scenes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/transparent
      (when-let [vb (generate-vertex-buffer renderables)]
        (let [user-data (:user-data (first renderables))
              blend-mode (:blend-mode user-data)
              gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
              ;shader (get user-data :shader render/shader-tex-tint)
              shader (get user-data :shader rive-shader-tint)
              vertex-binding (vtx/use-with ::rive-trans vb shader)]
          (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
            (gl/set-blend-mode gl blend-mode)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))

      pass/selection
      (when-let [vb (generate-vertex-buffer renderables)]
        (let [gpu-texture (:gpu-texture (:user-data (first renderables)))
              vertex-binding (vtx/use-with ::rive-selection vb rive-id-shader)]
          (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [gpu-texture rive-id-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))))))

(defn- render-rive-outlines [^GL2 gl render-args renderables rcount]
  (assert (= (:pass render-args) pass/outline))
  (render/render-aabb-outline gl render-args ::rive-outline renderables rcount))

(g/defnk produce-main-scene [_node-id rive-file-handle rive-anim-ids aabb gpu-texture default-tex-params rive-scene-pb scene-structure]
  (when rive-file-handle
    (let [blend-mode :blend-mode-alpha]
      (assoc {:node-id _node-id :aabb aabb}
             :renderable {:render-fn render-rive-scenes
                          :tags #{:rive}
                          :batch-key gpu-texture
                          :select-batch-key _node-id
                          :user-data {:rive-scene-pb rive-scene-pb
                                      :rive-file-handle rive-file-handle
                                      :aabb aabb
                                      :scene-structure {} ; TODO: Remove this remnant
                                      ;:gpu-texture gpu-texture
                                      :gpu-texture (or gpu-texture texture/white-pixel)
                                      ;:tex-params default-tex-params
                                      :blend-mode blend-mode}
                          :passes [pass/transparent pass/selection]}))))

(defn- make-rive-outline-scene [_node-id aabb]
  {:aabb aabb
   :node-id _node-id
   :renderable {:render-fn render-rive-outlines
                :tags #{:spine :outline}
                :batch-key ::outline
                :passes [pass/outline]}})

(g/defnk produce-rivescene [_node-id aabb main-scene gpu-texture scene-structure]
  (if (some? main-scene)
    (assoc main-scene :children [(make-rive-outline-scene _node-id aabb)])
    {:node-id _node-id :aabb aabb}))

(g/defnode RiveSceneNode
  (inherits resource-node/ResourceNode)

  (property rive-file resource/Resource
            (value (gu/passthrough rive-file-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :rive-file-resource]
                                            [:content :rive-scene]
                                            [:rive-handle :rive-file-handle]
                                            [:structure :scene-structure]
                                            [:animations :rive-anim-ids]
                                            [:aabb :aabb]
                                            [:node-outline :source-outline]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext rive-file-ext}))
            (dynamic error (g/fnk [_node-id rive-file]
                                  (validate-rivescene-riv-file _node-id rive-file))))

  (input rive-file-resource resource/Resource)
  (input rive-file-handle g/Any)
  (input rive-anim-ids g/Any)
  (input aabb g/Any)
  ;(input atlas-resource resource/Resource)

  (input anim-data g/Any)
  (input default-tex-params g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)
  (input rive-scene g/Any)
  (input scene-structure g/Any)

  (output save-value g/Any produce-rivescene-save-value)
  (output own-build-errors g/Any produce-rivescene-own-build-errors)
  (output build-targets g/Any :cached produce-rivescene-build-targets)
  (output rive-scene-pb g/Any :cached produce-rivescene-pb)
  (output main-scene g/Any :cached produce-main-scene)
  (output scene g/Any :cached produce-rivescene)
  (output scene-structure g/Any (gu/passthrough scene-structure))
  (output rive-file-handle g/Any :cached (gu/passthrough rive-file-handle))
  (output rive-anim-ids g/Any :cached (gu/passthrough rive-anim-ids))
  (output aabb g/Any :cached (gu/passthrough aabb)))

; .rivescene
(defn load-rive-scene [project self resource rivescene]
  (let [rive-resource (workspace/resolve-resource resource (:scene rivescene))] ; File/ZipResource type
        ;atlas          (workspace/resolve-resource resource (:atlas rivescene))

    (concat
     (g/connect project :default-tex-params self :default-tex-params)
     (g/set-property self
                     :rive-file rive-resource))))
                      ;:atlas atlas


;
; .rivemodel (The "instance" file)
;

(g/defnk produce-rivemodel-pb [rive-scene-resource default-animation material-resource blend-mode]
  (let [pb {:scene (resource/resource->proj-path rive-scene-resource)
            :default-animation default-animation
            :material (resource/resource->proj-path material-resource)
            :blend-mode blend-mode}]
    pb))

(defn- validate-model-default-animation [node-id rive-scene rive-anim-ids default-animation]
  (when (and rive-scene (not-empty default-animation))
    (validation/prop-error :fatal node-id :default-animation
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "animation '%s' could not be found in the specified rive scene" anim)))
                           default-animation
                           (set rive-anim-ids))))

(defn- validate-model-material [node-id material]
  (prop-resource-error :fatal node-id :material material "Material"))

(defn- validate-model-rive-scene [node-id rive-scene]
  (prop-resource-error :fatal node-id :scene rive-scene "Rive Scene"))

(g/defnk produce-model-own-build-errors [_node-id default-animation material rive-anim-ids rive-scene scene-structure]
  (g/package-errors _node-id
                    (validate-model-material _node-id material)
                    (validate-model-rive-scene _node-id rive-scene)
                    (validate-model-default-animation _node-id rive-scene rive-anim-ids default-animation)))

(defn- build-rive-model [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc") pb)}))

(g/defnk produce-model-build-targets [_node-id own-build-errors resource model-pb rive-scene-resource material-resource dep-build-targets]
  (g/precluding-errors own-build-errors
                       (let [dep-build-targets (flatten dep-build-targets)
                             deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
                             dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:scene rive-scene-resource] [:material material-resource]])]
                         [(bt/with-content-hash
                            {:node-id _node-id
                             :resource (workspace/make-build-resource resource)
                             :build-fn build-rive-model
                             :user-data {:proto-msg model-pb
                                         :dep-resources dep-resources}
                             :deps dep-build-targets})])))

(g/defnode RiveModelNode
  (inherits resource-node/ResourceNode)

  (property rive-scene resource/Resource
            (value (gu/passthrough rive-scene-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :rive-scene-resource]
                                            [:main-scene :rive-main-scene]
                                            [:rive-anim-ids :rive-anim-ids]
                                            [:build-targets :dep-build-targets]
                                            [:anim-data :anim-data]
                                            [:scene-structure :scene-structure])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext rive-scene-ext}))
            (dynamic error (g/fnk [_node-id rive-scene]
                                  (validate-model-rive-scene _node-id rive-scene))))
  (property blend-mode g/Any (default :blend-mode-alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$BlendMode")))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$BlendMode")))))
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:shader :material-shader]
                                            [:samplers :material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"}))
            (dynamic error (g/fnk [_node-id material]
                                  (validate-model-material _node-id material))))
  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id rive-anim-ids default-animation rive-scene]
                                  (validate-model-default-animation _node-id rive-scene rive-anim-ids default-animation)))
            (dynamic edit-type (g/fnk [rive-anim-ids] (properties/->choicebox rive-anim-ids))))

  (input dep-build-targets g/Any :array)
  (input rive-scene-resource resource/Resource)
  (input rive-main-scene g/Any)
  (input scene-structure g/Any)
  (input rive-anim-ids g/Any)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input anim-data g/Any)

  ; (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
  ;                            (or (some-> material-samplers first material/sampler->tex-params)
  ;                                default-tex-params)))
  (output anim-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached (g/fnk [_node-id rive-main-scene material-shader]
                                     (if (and (some? material-shader) (some? (:renderable rive-main-scene)))
                                       (let [aabb (:aabb rive-main-scene)
                                             rive-scene-node-id (:node-id rive-main-scene)]
                                         (-> rive-main-scene
                                             (assoc-in [:renderable :user-data :shader] rive-shader-tint)
                                        ;;;;;;;;;;;(assoc :gpu-texture texture/white-pixel)
                                        ;(update-in [:renderable :user-data :gpu-texture] texture/set-params tex-params)
                                             (assoc :aabb aabb)
                                             (assoc :children [(make-rive-outline-scene rive-scene-node-id aabb)])))

                                       (merge {:node-id _node-id
                                               :renderable {:passes [pass/selection]}
                                               :aabb (:aabb rive-main-scene)}
                                              rive-main-scene))))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id own-build-errors scene]
                                                          (cond-> {:node-id _node-id
                                                                   :node-outline-key "Rive Model"
                                                                   :label "Rive Model"
                                                                   :icon rive-model-icon
                                                                   :outline-error? (g/error-fatal? own-build-errors)}

                                                            (resource/openable-resource? scene)
                                                            (assoc :link scene :outline-reference? false))))
  (output model-pb g/Any produce-rivemodel-pb)
  (output save-value g/Any (gu/passthrough model-pb))
  (output own-build-errors g/Any produce-model-own-build-errors)
  (output build-targets g/Any :cached produce-model-build-targets))


(defn register-resource-types [workspace]
  (concat
   (resource-node/register-ddf-resource-type workspace
                                             :ext rive-scene-ext
                                             :label "Rive Scene"
                                             :node-type RiveSceneNode
                                             :ddf-type (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveSceneDesc")
                                             :load-fn load-rive-scene
                                             :icon rive-scene-icon
                                             :view-types [:scene :text]
                                             :view-opts {:scene {:grid true}}
                                             :template "/defold-rive/assets/template.rivescene")
   (resource-node/register-ddf-resource-type workspace
                                             :ext rive-model-ext
                                             :label "Rive Model"
                                             :node-type RiveModelNode
                                             :ddf-type (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc")
                                             :load-fn load-rive-model
                                             :icon rive-model-icon
                                             :view-types [:scene :text]
                                             :view-opts {:scene {:grid true}}
                                             :tags #{:component}
                                             :tag-opts {:component {:transform-properties #{:position :rotation}}}
                                             :template "/defold-rive/assets/template.rivemodel")
   (workspace/register-resource-type workspace
                                     :ext rive-file-ext
                                     :node-type RiveFileNode
                                     :load-fn load-rive-file
                                     :icon rive-file-icon
                                     :view-types [:default]
                                     :tags #{:embeddable})))

; The plugin
(defn load-plugin-rive [workspace]
  (prn "RIVE" "load-plugin-rive")
  (g/transact (concat (register-resource-types workspace))))

(defn return-plugin []
  (fn [x] (load-plugin-rive x)))
(return-plugin)


