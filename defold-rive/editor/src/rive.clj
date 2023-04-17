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
            [editor.gl.vertex2 :as vtx]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.render :as render]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.gl.pass :as pass]
            [editor.types :as types]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [util.murmur :as murmur])
  (:import [editor.gl.shader ShaderLifecycle]
           [com.jogamp.opengl GL GL2]
           [org.apache.commons.io IOUtils]
           [java.io IOException]
           [java.nio FloatBuffer IntBuffer]
           [javax.vecmath Matrix4d Vector3d Vector4d]))


(set! *warn-on-reflection* true)

(def rive-file-icon "/defold-rive/editor/resources/icons/32/Icons_17-Rive-file.png")
(def rive-scene-icon "/defold-rive/editor/resources/icons/32/Icons_16-Rive-scene.png")
(def rive-model-icon "/defold-rive/editor/resources/icons/32/Icons_15-Rive-model.png")
(def rive-bone-icon "/defold-rive/editor/resources/icons/32/Icons_18-Rive-bone.png")
(def rive-material-path "/defold-rive/assets/rivemodel.material")

(def rive-file-ext "riv")
(def rive-scene-ext "rivescene")
(def rive-model-ext "rivemodel")

; Plugin functions (from Rive.java)

;; (defn- debug-cls [^Class cls]
;;   (doseq [m (.getMethods cls)]
;;     (prn (.toString m))
;;     (println "Method Name: " (.getName m) "(" (.getParameterTypes m) ")")
;;     (println "Return Type: " (.getReturnType m) "\n")))
;; TODO: Support public variables as well

(def rive-plugin-cls (workspace/load-class! "com.dynamo.bob.pipeline.Rive"))
(def rive-plugin-file-cls (workspace/load-class! "com.dynamo.bob.pipeline.Rive$RiveFile"))
(def byte-array-cls (Class/forName "[B"))

(defn- plugin-invoke-static [^Class cls name types args]
  (let [method (.getMethod cls name types)]
    (.invoke method nil (into-array Object args))))

(defn- plugin-load-file [bytes path]
  (plugin-invoke-static rive-plugin-cls "LoadFromBufferInternal" (into-array Class [String byte-array-cls]) [path bytes]))

(defn- plugin-destroy-file [file]
  (plugin-invoke-static rive-plugin-cls "Destroy" (into-array Class [rive-plugin-file-cls]) [file]))

(defn- plugin-update-file [file dt bytes-texture-set]
  (plugin-invoke-static rive-plugin-cls "UpdateInternal" (into-array Class [rive-plugin-file-cls Float/TYPE byte-array-cls]) [file (float dt) bytes-texture-set]))

; .rivemodel
(defn load-rive-model [project self resource content]
  (let [rive-scene-resource (workspace/resolve-resource resource (:scene content))
        material (workspace/resolve-resource resource (:material content))]
    (concat
     (g/connect project :default-tex-params self :default-tex-params)
     (g/set-property self
                     :rive-scene rive-scene-resource
                     :material material
                     ;:atlas atlas
                     :default-animation (:default-animation content)
                     :default-state-machine (:default-state-machine content)
                     :blend-mode (:blend-mode content)
                     :create-go-bones (:create-go-bones content)))))



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
  (property state-machine-ids g/Any)
  (property aabb g/Any)
  (property vertices g/Any)
  (property bones g/Any)

  (input child-bones g/Any :array)

  (output build-targets g/Any :cached produce-rive-file-build-targets))

(set! *warn-on-reflection* false)

(defn- convert-aabb [rive-aabb]
  (let [min (.-min rive-aabb)
        max (.-max rive-aabb)
        aabb (geom/coords->aabb [(.-x min) (.-y min) 0] [(.-x max) (.-y max) 0])]
    aabb))

(defn- create-bone [project parent-id rive-bone]
  ; rive-bone is of type Rive$Bone (Rive.java)
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
        child-bones (.-children bone)
        children-tx-data (mapcat (fn [child] (create-bone-hierarchy project bone-id bones child)) child-bones)]
    (concat bone-tx-data children-tx-data)))

(defn- create-bones [project parent-id bones]
  ; bones is a hierarchy of Rive$Bone (Rive.java)
  ; bones is a list of root bones
  (mapcat (fn [bone] (create-bone-hierarchy project parent-id bones bone)) bones))

(def my-atom (atom 0))

; Loads the .riv file
(defn- load-rive-file
  [project node-id resource]
  (let [content (resource->bytes resource)
        rive-handle (plugin-load-file content (resource/resource->proj-path resource))
        _ (reset! my-atom rive-handle)
        animations (.-animations rive-handle)
        state-machines (.-stateMachines rive-handle)
        state-machine-ids (map (fn [state-machine] (.-name state-machine)) state-machines)
        _ (.Update rive-handle 0.0)
        aabb (convert-aabb (.-aabb rive-handle))
        bones (.-bones rive-handle)

        tx-data (concat
                 (g/set-property node-id :content content)
                 (g/set-property node-id :rive-handle rive-handle)
                 (g/set-property node-id :animations animations)
                 (g/set-property node-id :state-machine-ids state-machine-ids)
                 (g/set-property node-id :aabb aabb)
                 (g/set-property node-id :bones bones))

        all-tx-data (concat tx-data (create-bones project node-id bones))]
    all-tx-data))

(set! *warn-on-reflection* true)

; Rive Scene
; .rivescene (The "data" file) which in turn points to the .riv file

; Node defs
(g/defnk produce-rivescene-save-value [rive-file-resource atlas-resource]
  ; rive-file-resource may be nil if the :scene isn't set (as seen in the template.rivescene)
  {:scene (resource/resource->proj-path rive-file-resource)
   :atlas (resource/resource->proj-path atlas-resource)})


(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name))

; The properties of the .rivescene (see RiveSceneDesc in rive_ddf.proto)
; The "scene" should point to a .riv file

(defn- validate-scene-atlas [_node-id atlas]
  (prop-resource-error :fatal _node-id :atlas atlas "Atlas"))

(defn- validate-rivescene-riv-file [_node-id rive-file]
  (prop-resource-error :fatal _node-id :scene rive-file "Riv File"))

(g/defnk produce-rivescene-own-build-errors [_node-id rive-file atlas]
  (g/package-errors _node-id
                    (validate-scene-atlas _node-id atlas)
                    (validate-rivescene-riv-file _node-id rive-file)))

(defn- build-rive-scene [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce
             #(assoc %1 (first %2) (second %2))
             pb
             (map
               (fn [[label res]]
                   (when (not (nil? res))
                         [label (resource/proj-path (get dep-resources res))]))
               (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveSceneDesc") pb)}))


(g/defnk produce-rivescene-build-targets
  [_node-id own-build-errors resource rive-scene-pb rive-file atlas-resource dep-build-targets]
  (g/precluding-errors own-build-errors
                       (let [dep-build-targets (flatten dep-build-targets)
                             deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
                             dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:scene rive-file] [:atlas atlas-resource]])]
                         [(bt/with-content-hash
                            {:node-id _node-id
                             :resource (workspace/make-build-resource resource)
                             :build-fn build-rive-scene
                             :user-data {:proto-msg rive-scene-pb
                                         :dep-resources dep-resources}
                             :deps dep-build-targets})])))

(g/defnk produce-rivescene-pb [_node-id rive-file-resource]
  {:scene (resource/resource->proj-path rive-file-resource)})

(defn- renderable->handle [renderable]
  (get-in renderable [:user-data :rive-file-handle]))

(defn- renderable->texture-set-pb [renderable]
  (get-in renderable [:user-data :texture-set-pb]))


(shader/defshader rive-id-shader-vp
  (attribute vec2 position)
  (uniform vec4 cover)
  (defn void main []
    (setq vec4 pos (vec4 position.xy 0.0 1.0))
    (setq gl_Position (+ (* (* gl_ModelViewProjectionMatrix pos) (- 1.0  cover.x)) (* cover.x pos)))
    ))

(shader/defshader rive-id-shader-fp
  (uniform vec4 color)
  (uniform vec4 id)
  (defn void main []
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))
    ))

(def rive-id-shader (shader/make-shader ::id-shader rive-id-shader-vp rive-id-shader-fp {"id" :id}))

(vtx/defvertex vtx-textured
 (vec2 position)
 (vec2 texcoord0))

(set! *warn-on-reflection* false)

(defn renderable->render-objects [renderable]
  (let [handle (renderable->handle renderable)
        texture-set-pb (renderable->texture-set-pb renderable)
        vb-data (.vertices handle)
        vb-data-float-buffer (FloatBuffer/wrap vb-data)
        vb (vtx/wrap-vertex-buffer vtx-textured :static vb-data-float-buffer)

        ib-data (.indices handle)
        ib (IntBuffer/wrap ib-data)
        
        render-objects (.renderObjects handle)]
    {:vertex-buffer vb
     :index-buffer ib
     :render-objects render-objects
     :handle handle
     :texture-set-pb texture-set-pb
     :renderable renderable}))

(set! *warn-on-reflection* true)

(defn collect-render-groups [renderables]
  (map renderable->render-objects renderables))

(def constant-colors (murmur/hash64 "colors"))
(def constant-transform_local (murmur/hash64 "transform_local"))
(def constant-cover (murmur/hash64 "cover"))
(def constant-stops (murmur/hash64 "stops"))
(def constant-gradientLimits (murmur/hash64 "gradientLimits"))
(def constant-properties (murmur/hash64 "properties"))

(defn- constant-hash->name [hash]
  (condp = hash
    constant-colors "colors"
    constant-transform_local "transform_local"
    constant-cover "cover"
    constant-stops "stops"
    constant-gradientLimits "gradientLimits"
    constant-properties "properties"
    "unknown"))

(defn- do-mask [mask count]
  ; Checks if a bit in the mask is set: "(mask & (1<<count)) != 0"
  (not= 0 (bit-and mask (bit-shift-left 1 count))))

; See GetOpenGLCompareFunc in graphics_opengl.cpp
(defn- stencil-func->gl-func [func]
  (case func
    0 GL/GL_NEVER
    1 GL/GL_LESS
    2 GL/GL_LEQUAL
    3 GL/GL_GREATER
    4 GL/GL_GEQUAL
    5 GL/GL_EQUAL
    6 GL/GL_NOTEQUAL
    7 GL/GL_ALWAYS))

(defn- stencil-op->gl-op [op]
  (case op
    0 GL/GL_KEEP
    1 GL/GL_ZERO
    2 GL/GL_REPLACE
    3 GL/GL_INCR
    4 GL/GL_INCR_WRAP
    5 GL/GL_DECR
    6 GL/GL_DECR_WRAP
    7 GL/GL_INVERT))

(set! *warn-on-reflection* false)

(defn- set-stencil-func! [^GL2 gl face-type ref ref-mask state]
  (let [gl-func (stencil-func->gl-func (.func state))
        op-stencil-fail (stencil-op->gl-op (.opSFail state))
        op-depth-fail (stencil-op->gl-op (.opDPFail state))
        op-depth-pass (stencil-op->gl-op (.opDPPass state))]
    (.glStencilFuncSeparate gl face-type gl-func ref ref-mask)
    (.glStencilOpSeparate gl face-type op-stencil-fail op-depth-fail op-depth-pass)))

(defn- to-int [b]
  (bit-and 0xff (int b)))

; See ApplyStencilTest in render.cpp for reference
(defn- set-stencil-test-params! [^GL2 gl params]
  (let [clear (.clearBuffer params)
        mask (to-int (.bufferMask params))
        color-mask (.colorBufferMask params)
        separate-states (.separateFaceStates params)
        ref (to-int (.ref params))
        ref-mask (to-int (.refMask params))
        state-front (.front params)
        state-back (if (not= separate-states 0) (.back params) state-front)]
    (when (not= clear 0)
      (.glStencilMask gl 0xFF)
      (.glClear gl GL/GL_STENCIL_BUFFER_BIT))

    (.glColorMask gl (do-mask color-mask 3) (do-mask color-mask 2) (do-mask color-mask 1) (do-mask color-mask 0))
    (.glStencilMask gl mask)

    (set-stencil-func! gl GL/GL_FRONT ref ref-mask state-front)
    (set-stencil-func! gl GL/GL_BACK ref ref-mask state-back)))

(defn- to-vector4d [v]
  (Vector4d. (.x v) (.y v) (.z v) (.w v)))

(defn- vector4d-to-floats [^Vector4d val]
  (list (.x val) (.y val) (.z val) (.w val)))

(defn- set-constant! [^GL2 gl shader constant]

  (let [name-hash (.nameHash constant)
        name (constant-hash->name name-hash)
        values-array (.values constant)
        count (alength values-array)
        ;; TODO: Make a helper function from Java instead!
        values (vec (.values constant))
        vec4-vals (map to-vector4d values)
        dbl-vals (mapcat vector4d-to-floats vec4-vals)
        flt-vals (float-array dbl-vals)]
    (when (not= name-hash 0)
      (let []
        (shader/set-uniform-array shader gl name count flt-vals)))))

(defn- set-constants! [^GL2 gl shader ro]
  (let [constants (.constantBuffer ro)]
    (doall (map (fn [constant] (set-constant! gl shader constant)) constants))))

(defmacro gl-draw-arrays-2 [gl prim-type start count]      `(.glDrawArrays ~(with-meta gl {:tag `GL}) ~prim-type ~start ~count))

(defn- do-render-object! [^GL2 gl render-args shader renderable ro]
  (let [start (.vertexStart ro) ; the name is from the engine, but in this case refers to the index
        count (.vertexCount ro) ; count in number of indices
        start (* start 2) ; offset in bytes
        face-winding (if (not= (.faceWinding ro) 0) GL/GL_CCW GL/GL_CW)

        ro-transform (double-array (.m (.worldTransform ro)))
        renderable-transform (Matrix4d. (:world-transform renderable)) ; make a copy so we don't alter the original
        ro-matrix (doto (Matrix4d. ro-transform) (.transpose))
        shader-world-transform (doto renderable-transform (.mul ro-matrix))
        primitive-type (.primitiveType ro)
        is-tri-strip (= primitive-type 2)
        gl-prim-type (if is-tri-strip GL/GL_TRIANGLE_STRIP GL/GL_TRIANGLES)
        render-args (merge render-args
                           (math/derive-render-transforms shader-world-transform
                                                          (:view render-args)
                                                          (:projection render-args)
                                                          (:texture render-args)))]

    (shader/set-uniform shader gl "world_view_proj" (:world-view-proj render-args))
    (set-constants! gl shader ro)
    (when (.setFaceWinding ro)
      (gl/gl-front-face gl face-winding))
    (when (.setStencilTest ro)
      (set-stencil-test-params! gl (.stencilTestParams ro)))

    (gl/gl-draw-elements gl gl-prim-type start count)))

  (set! *warn-on-reflection* true)

; Borrowed from gui_clipping.clj
(defn- setup-gl [^GL2 gl]
  (.glEnable gl GL/GL_STENCIL_TEST)
  (.glClear gl GL/GL_STENCIL_BUFFER_BIT))

(defn- restore-gl [^GL2 gl]
  (.glDisable gl GL/GL_STENCIL_TEST)
  (.glColorMask gl true true true true))

(defn- render-group-transparent [^GL2 gl render-args override-shader group]
  (let [renderable (:renderable group)
        node-id (:node-id renderable)
        user-data (:user-data renderable)
        blend-mode (:blend-mode user-data)
        gpu-texture (or (get user-data :gpu-texture) texture/white-pixel)
        shader (if (not= override-shader nil) override-shader
                   (:shader user-data))
        vb (:vertex-buffer group)
        ib (:index-buffer group)
        render-objects (:render-objects group)
        vertex-index-binding (vtx/use-with [node-id ::rive-trans] vb ib shader)]

    (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-index-binding]
      (setup-gl gl)
      (gl/set-blend-mode gl blend-mode)
      (doseq [ro render-objects]
        (do-render-object! gl render-args shader renderable ro))
      (restore-gl gl))))

(defn- render-rive-scenes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)
        render-groups (collect-render-groups renderables)]
       (doseq [group render-groups]
         (plugin-update-file (:handle group) 0.0 (:texture-set-pb group)))
    (condp = pass
      pass/transparent
      (doseq [group render-groups]
        (render-group-transparent gl render-args nil group))

      pass/selection
      (doseq [group (collect-render-groups renderables)]
        (render-group-transparent gl render-args rive-id-shader group)))))

(defn- render-rive-outlines [^GL2 gl render-args renderables rcount]
  (assert (= (:pass render-args) pass/outline))
  (render/render-aabb-outline gl render-args ::rive-outline renderables rcount))

(g/defnk produce-main-scene [_node-id material-shader rive-file-handle rive-anim-ids aabb gpu-texture default-tex-params rive-scene-pb scene-structure texture-set-pb]
  (when rive-file-handle
    (let [blend-mode :blend-mode-alpha]
      (assoc {:node-id _node-id :aabb aabb}
             :renderable {:render-fn render-rive-scenes
                          :tags #{:rive}
                          :batch-key [gpu-texture material-shader]
                          :select-batch-key _node-id
                          :user-data {:rive-scene-pb rive-scene-pb
                                      :rive-file-handle rive-file-handle
                                      :aabb aabb
                                      :scene-structure {} ; TODO: Remove this remnant
                                      :shader material-shader
                                      ;:gpu-texture gpu-texture
                                      :gpu-texture (or gpu-texture texture/white-pixel)
                                      ;:tex-params default-tex-params
                                      :texture-set-pb texture-set-pb
                                      :blend-mode blend-mode}
                          :passes [pass/transparent pass/selection]}))))

(defn- make-rive-outline-scene [_node-id aabb]
  {:aabb aabb
   :node-id _node-id
   :renderable {:render-fn render-rive-outlines
                :tags #{:rive :outline}
                :batch-key ::outline
                :passes [pass/outline]}})

(g/defnk produce-rivescene [_node-id rive-file-handle aabb main-scene gpu-texture scene-structure]
  (when rive-file-handle
    (if (some? main-scene)
      (-> main-scene
          (assoc :children [(make-rive-outline-scene _node-id aabb)]))
      {:node-id _node-id :aabb aabb})))

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
                                            [:state-machine-ids :rive-state-machine-ids]
                                            [:aabb :aabb]
                                            [:node-outline :source-outline]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext rive-file-ext}))
            (dynamic error (g/fnk [_node-id rive-file]
                                  (validate-rivescene-riv-file _node-id rive-file))))

  (property atlas resource/Resource
            (value (gu/passthrough atlas-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :atlas-resource]  ; Is this redundant?
                                            [:texture-set-pb :texture-set-pb]
                                            [:gpu-texture :gpu-texture]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "atlas"}))
            (dynamic error (g/fnk [_node-id atlas]
                                  (validate-scene-atlas _node-id atlas))))

; This property isn't visible, but here to allow us to preview the .spinescene
  (property material resource/Resource
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:shader :material-shader]
                                            [:samplers :material-samplers])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"}))
            (dynamic visible (g/constantly false)))

  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input material-resource resource/Resource) ; Just for being able to preview the asset
  (input rive-file-resource resource/Resource)
  (input rive-file-handle g/Any)
  (input rive-anim-ids g/Any)
  (input rive-state-machine-ids g/Any)
  (input aabb g/Any)
  (input atlas-resource resource/Resource)

  (input texture-set-pb g/Any)
  (output texture-set-pb g/Any :cached (gu/passthrough texture-set-pb))

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
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output rive-file-handle g/Any :cached (gu/passthrough rive-file-handle))
  (output rive-anim-ids g/Any :cached (gu/passthrough rive-anim-ids))
  (output rive-state-machine-ids g/Any :cached (gu/passthrough rive-state-machine-ids))
  (output aabb g/Any :cached (gu/passthrough aabb)))

; .rivescene
(defn load-rive-scene [project self resource rivescene]
  (let [rive-file (workspace/resolve-resource resource (:scene rivescene)) ; File/ZipResource type
        atlas    (workspace/resolve-resource resource (:atlas rivescene))
        material (workspace/resolve-resource resource rive-material-path)]
    (concat
     (g/connect project :default-tex-params self :default-tex-params)
     (g/set-property self
                     :rive-file rive-file
                     :atlas atlas
                     :material material))))


;
; .rivemodel (The "instance" file)
;

(g/defnk produce-rivemodel-pb [rive-scene-resource default-animation default-state-machine material-resource blend-mode create-go-bones]
  (let [pb {:scene (resource/resource->proj-path rive-scene-resource)
            :material (resource/resource->proj-path material-resource)
            :default-animation default-animation
            :default-state-machine default-state-machine
            :blend-mode blend-mode
            :create-go-bones create-go-bones}]
    pb))

(defn- validate-model-default-animation [node-id rive-scene rive-anim-ids default-animation]
  (when (and rive-scene (not-empty default-animation))
    (validation/prop-error :fatal node-id :default-animation
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "animation '%s' could not be found in the specified rive scene" anim)))
                           default-animation
                           (set rive-anim-ids))))

(defn- validate-model-default-state-machine [node-id rive-scene rive-state-machine-ids default-state-machine]
  (when (and rive-scene (not-empty default-state-machine))
    (validation/prop-error :fatal node-id :default-state-machine
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "state machine '%s' could not be found in the specified rive scene" anim)))
                           default-state-machine
                           (set rive-state-machine-ids))))

(defn- validate-model-material [node-id material]
  (prop-resource-error :fatal node-id :material material "Material"))

(defn- validate-model-rive-scene [node-id rive-scene]
  (prop-resource-error :fatal node-id :scene rive-scene "Rive Scene"))

(g/defnk produce-model-own-build-errors [_node-id default-animation default-state-machine material rive-anim-ids rive-state-machine-ids rive-scene scene-structure]
  (g/package-errors _node-id
                    (validate-model-material _node-id material)
                    (validate-model-rive-scene _node-id rive-scene)
                    (validate-model-default-animation _node-id rive-scene rive-anim-ids default-animation)
                    (validate-model-default-state-machine _node-id rive-scene rive-state-machine-ids default-state-machine)))

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
                                            [:atlas-resource :atlas-resource]
                                            [:texture-set-pb :texture-set-pb]
                                            [:main-scene :rive-main-scene]
                                            [:rive-anim-ids :rive-anim-ids]
                                            [:rive-state-machine-ids :rive-state-machine-ids]
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
  (property default-state-machine g/Str
            (dynamic error (g/fnk [_node-id rive-state-machine-ids default-state-machine rive-scene]
                                  (validate-model-default-state-machine _node-id rive-scene rive-state-machine-ids default-state-machine)))
            (dynamic edit-type (g/fnk [rive-state-machine-ids] (properties/->choicebox rive-state-machine-ids))))
  (property default-animation g/Str
            (dynamic error (g/fnk [_node-id rive-anim-ids default-animation rive-scene]
                                  (validate-model-default-animation _node-id rive-scene rive-anim-ids default-animation)))
            (dynamic edit-type (g/fnk [rive-anim-ids] (properties/->choicebox (cons "" rive-anim-ids)))))
  (property create-go-bones g/Bool (default false))

  (input dep-build-targets g/Any :array)
  (input rive-scene-resource resource/Resource)
  (input rive-main-scene g/Any)
  (input scene-structure g/Any)
  (input rive-anim-ids g/Any)
  (input rive-state-machine-ids g/Any)
  (input texture-set-pb g/Any)
  (input atlas-resource resource/Resource)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input anim-data g/Any)

  ; (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
  ;                            (or (some-> material-samplers first material/sampler->tex-params)
  ;                                default-tex-params)))
  (output anim-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output state-machine-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached (g/fnk [_node-id rive-main-scene material-shader]
                                     (if (and (some? material-shader) (some? (:renderable rive-main-scene)))
                                       (let [aabb (:aabb rive-main-scene)
                                             rive-scene-node-id (:node-id rive-main-scene)]
                                         (-> rive-main-scene
                                             (assoc-in [:renderable :user-data :shader] material-shader)
                                        ;;;;;;;;;;;(assoc :gpu-texture texture/white-pixel)
                                        ;(update-in [:renderable :user-data :gpu-texture] texture/set-params tex-params)
                                             (assoc :aabb aabb)
                                             (assoc :children [(make-rive-outline-scene rive-scene-node-id aabb)])))

                                       (merge {:node-id _node-id
                                               :renderable {:passes [pass/selection]}
                                               :aabb (if rive-main-scene (:aabb rive-main-scene) geom/null-aabb)}
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
  (g/transact (concat (register-resource-types workspace))))

(defn return-plugin []
  (fn [x] (load-plugin-rive x)))
(return-plugin)
