;
; MIT License
; Copyright (c) 2021 Defold
; Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
; The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
;

(ns editor.rive
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.attribute :as attribute]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.murmur :as murmur])
  (:import [com.jogamp.opengl GL GL2]
           [editor.gl.shader ShaderLifecycle]
           [java.io IOException]
           [java.nio FloatBuffer IntBuffer]
           [java.awt.image BufferedImage DataBufferByte]
           [javax.vecmath Matrix4d Vector3d Vector4d]
           [org.apache.commons.io IOUtils]))

(set! *warn-on-reflection* true)

(def rive-scene-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveSceneDesc"))
(def rive-model-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc"))
(def blend-mode-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$BlendMode"))
(def coordinate-system-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$CoordinateSystem"))
(def artboard-fit-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$Fit"))
(def artboard-alignment-pb-class (workspace/load-class! "com.dynamo.rive.proto.Rive$RiveModelDesc$Alignment"))

(def rive-file-icon "/defold-rive/editor/resources/icons/32/Icons_17-Rive-file.png")
(def rive-scene-icon "/defold-rive/editor/resources/icons/32/Icons_16-Rive-scene.png")
(def rive-model-icon "/defold-rive/editor/resources/icons/32/Icons_15-Rive-model.png")
(def rive-bone-icon "/defold-rive/editor/resources/icons/32/Icons_18-Rive-bone.png")
(def rive-artboard-icon "/defold-rive/editor/resources/icons/32/Icons_19-Rive-artboard.png")
(def rive-state-machine-icon "/defold-rive/editor/resources/icons/32/Icons_20-Rive-statemachine.png")
(def rive-view-model-icon "/defold-rive/editor/resources/icons/32/Icons_21-Rive-viewmodel.png")
(def rive-view-model-property-icon "/defold-rive/editor/resources/icons/32/Icons_22-Rive-property.png")

;; These should be read from the .proto file
(def default-material-proj-path "/defold-rive/assets/rivemodel.material")
(def default-blit-material-proj-path "/defold-rive/assets/shader-library/rivemodel_blit.material")

;; Used for selection in the editor Scene View.
(def selection-material-proj-path "/defold-rive/editor/resources/materials/rivemodel_selection.material")

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

(defn- plugin-update-file [file dt]
  (plugin-invoke-static rive-plugin-cls "UpdateInternal" (into-array Class [rive-plugin-file-cls Float/TYPE]) [file (float dt)]))

(defn- plugin-set-artboard [file artboard]
  (plugin-invoke-static rive-plugin-cls "SetArtboardInternal" (into-array Class [rive-plugin-file-cls String]) [file artboard]))

(defn- plugin-get-texture [file]
  (plugin-invoke-static rive-plugin-cls "GetTexture" (into-array Class [rive-plugin-file-cls]) [file]))

(defn- plugin-get-fullscreen-quad-vertices []
  (plugin-invoke-static rive-plugin-cls "GetFullscreenQuadVertices" (into-array Class []) []))

;; Helper for optional Java fields (Rive.java may not expose older data).
(defn- get-public-field [^Object obj ^String field-name]
  (when obj
    (try
      (let [field (.getField (.getClass obj) field-name)]
        (.get field obj))
      (catch Exception _ nil))))

(defn- to-state-machines-map [^java.util.Map state-machines]
  (if state-machines
    (into {}
          (map (fn [entry]
                 (let [^java.util.Map$Entry entry entry
                       key (.getKey entry)
                       value (.getValue entry)]
                   [key (if value (vec value) [])]))
               (.entrySet state-machines)))
    {}))

(defn- resolve-artboard-name [artboard artboards]
  (if (and artboard (not-empty artboard))
    artboard
    (first artboards)))

(defn- state-machines-for-artboard [state-machines-by-artboard artboard artboards]
  (let [resolved (resolve-artboard-name artboard artboards)]
    (if (and resolved state-machines-by-artboard)
      (get state-machines-by-artboard resolved [])
      [])))

(def ^:private unknown-value-text "<n/a>")
(def ^:private none-value-text "<none>")

(defn- string-or [value fallback]
  (if (and (string? value) (not (str/blank? value)))
    value
    fallback))

(defn- join-or [values fallback]
  (if (seq values)
    (str/join ", " values)
    fallback))

(defn- bounds->string [bounds]
  (let [min (get-public-field bounds "min")
        max (get-public-field bounds "max")
        min-x (get-public-field min "x")
        min-y (get-public-field min "y")
        max-x (get-public-field max "x")
        max-y (get-public-field max "y")]
    (if (and (number? min-x) (number? min-y) (number? max-x) (number? max-y))
      (let [min-x (double min-x)
            min-y (double min-y)
            max-x (double max-x)
            max-y (double max-y)]
        (format "%.2f, %.2f, %.2f, %.2f" min-x min-y max-x max-y))
      none-value-text)))

(defn- to-view-model-properties [view-model-properties]
  (vec
    (keep (fn [property]
            (let [view-model (get-public-field property "viewModel")
                  name (get-public-field property "name")
                  type-name (get-public-field property "typeName")
                  meta-data (get-public-field property "metaData")
                  value (get-public-field property "value")]
              (when view-model
                {:view-model view-model
                 :name (or name "")
                 :type-name (or type-name "unknown")
                 :meta-data (or meta-data "")
                 :value (or value unknown-value-text)})))
          view-model-properties)))

(defn- view-model-properties-by-name [view-model-properties]
  (reduce (fn [acc property]
            (update acc (:view-model property) (fnil conj []) property))
          {}
          view-model-properties))

(defn- to-view-model-instances [view-model-instance-names]
  (reduce (fn [acc entry]
            (let [view-model (get-public-field entry "viewModel")
                  instances (or (get-public-field entry "instances") [])]
              (if view-model
                (assoc acc view-model (vec instances))
                acc)))
          {}
          view-model-instance-names))

(defn- to-default-view-model-info [default-view-model-info]
  (when default-view-model-info
    {:view-model (get-public-field default-view-model-info "viewModel")
     :instance (get-public-field default-view-model-info "instance")}))

(g/defnk produce-rive-file-outline [_node-id child-outlines]
  {:node-id _node-id
   :node-outline-key "Rive File"
   :label "Rive File"
   :icon rive-file-icon
   :read-only true
   :children child-outlines})

; .rivemodel
(defn load-rive-model [project self resource rive-model-desc]
  {:pre [(map? rive-model-desc)]} ; Rive$RiveModelDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (gu/set-properties-from-pb-map self rive-model-pb-class rive-model-desc
        rive-scene (resolve-resource :scene)
        material (resolve-resource (:material :or default-material-proj-path))
        blit-material (resolve-resource (:blit-material :or default-blit-material-proj-path))
        artboard :artboard
        default-state-machine :default-state-machine
        blend-mode :blend-mode
        auto-bind :auto-bind
        coordinate-system :coordinate-system
        artboard-fit :artboard-fit
        artboard-alignment :artboard-alignment))))

(g/defnk produce-transform [position rotation scale]
  (math/->mat4-non-uniform (Vector3d. (double-array position))
                           (math/euler-z->quat rotation)
                           (Vector3d. (double-array scale))))

;; Outline nodes for artboards, state machines, and view models.
(g/defnode RiveArtboardNode
  (inherits outline/OutlineNode)

  (property name g/Str
            (dynamic label (g/constantly "Name"))
            (dynamic read-only? (g/constantly true)))
  (property item-type g/Str
            (value (g/constantly "Artboard"))
            (dynamic label (g/constantly "Type"))
            (dynamic read-only? (g/constantly true)))
  (property state-machines g/Str
            (dynamic label (g/constantly "State Machines"))
            (dynamic visible (g/constantly false))
            (dynamic read-only? (g/constantly true)))

  (input nodes g/Any :array)
  (input child-outlines g/Any :array)

  (output node-outline outline/OutlineData
          (g/fnk [_node-id name child-outlines]
            {:node-id _node-id
             :node-outline-key (str "artboard:" name)
             :label name
             :icon rive-artboard-icon
             :children child-outlines
             :read-only true})))

(g/defnode RiveStateMachineNode
  (inherits outline/OutlineNode)

  (property name g/Str
            (dynamic label (g/constantly "Name"))
            (dynamic read-only? (g/constantly true)))
  (property item-type g/Str
            (value (g/constantly "State Machine"))
            (dynamic label (g/constantly "Type"))
            (dynamic read-only? (g/constantly true)))
  (property artboard g/Str
            (dynamic visible (g/constantly false))
            (dynamic read-only? (g/constantly true)))

  (output node-outline outline/OutlineData
          (g/fnk [_node-id name artboard]
            {:node-id _node-id
             :node-outline-key (str "state-machine:" artboard ":" name)
             :label name
             :icon rive-state-machine-icon
             :read-only true})))

(g/defnode RiveViewModelNode
  (inherits outline/OutlineNode)

  (property name g/Str
            (dynamic label (g/constantly "Name"))
            (dynamic read-only? (g/constantly true)))
  (property item-type g/Str
            (value (g/constantly "View Model"))
            (dynamic label (g/constantly "Type"))
            (dynamic read-only? (g/constantly true)))
  (property default-instance g/Str
            (dynamic label (g/constantly "Default Instance"))
            (dynamic visible (g/constantly false))
            (dynamic read-only? (g/constantly true)))
  (property instances g/Str
            (dynamic label (g/constantly "Instances"))
            (dynamic visible (g/constantly false))
            (dynamic read-only? (g/constantly true)))

  (input nodes g/Any :array)
  (input child-outlines g/Any :array)

  (output node-outline outline/OutlineData
          (g/fnk [_node-id name child-outlines]
            {:node-id _node-id
             :node-outline-key (str "view-model:" name)
             :label name
             :icon rive-view-model-icon
             :children child-outlines
             :read-only true})))

(g/defnode RiveViewModelPropertyNode
  (inherits outline/OutlineNode)

  (property name g/Str
            (dynamic label (g/constantly "Name"))
            (dynamic read-only? (g/constantly true)))
  (property item-type g/Str
            (value (g/constantly "View Model Property"))
            (dynamic label (g/constantly "Type"))
            (dynamic read-only? (g/constantly true)))
  (property view-model g/Str
            (dynamic label (g/constantly "View Model"))
            (dynamic visible (g/constantly false))
            (dynamic read-only? (g/constantly true)))
  (property data-type g/Str
            (dynamic label (g/constantly "Data Type"))
            (dynamic read-only? (g/constantly true)))
  (property value g/Str
            (dynamic label (g/constantly "Value"))
            (dynamic read-only? (g/constantly true)))
  (property meta-data g/Str
            (dynamic label (g/constantly "Meta"))
            (dynamic read-only? (g/constantly true)))

  (output node-outline outline/OutlineData
          (g/fnk [_node-id name view-model]
            {:node-id _node-id
             :node-outline-key (str "view-model-property:" view-model ":" name)
             :label name
             :icon rive-view-model-property-icon
             :read-only true})))

;; Represents a single bone inside a .riv file, a proprietary file format.
(g/defnode RiveBone
  (inherits outline/OutlineNode)
  (property name g/Str (dynamic read-only? (g/constantly true)))
  (property position types/Vec3
            (dynamic edit-type (g/constantly {:type types/Vec2}))
            (dynamic read-only? (g/constantly true)))
  (property rotation g/Num (dynamic read-only? (g/constantly true)))
  (property scale types/Vec3
            (dynamic edit-type (g/constantly {:type types/Vec2}))
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

;; Represents a .riv file, a proprietary file format.
(g/defnode RiveFileNode
  (inherits resource-node/ResourceNode)

  (property content g/Any)
  (property rive-handle g/Any) ; The cpp pointer
  (property state-machines g/Any)
  (property aabb g/Any)
  (property vertices g/Any)
  (property bones g/Any)
  (property artboards g/Any)
  (property view-models g/Any)
  (property view-model-properties g/Any)
  (property bounds g/Str
            (dynamic visible (g/constantly false)))

  (input child-bones g/Any :array)

  (output build-targets g/Any :cached produce-rive-file-build-targets)
  (output node-outline outline/OutlineData :cached produce-rive-file-outline))

(set! *warn-on-reflection* false)

(defn- convert-aabb [rive-aabb]
  (let [min (.-min rive-aabb)
        max (.-max rive-aabb)
        min-x (.-x min)
        min-y (.-y min)
        max-x (.-x max)
        max-y (.-y max)
        center-x (* 0.5 (+ min-x max-x))
        center-y (* 0.5 (+ min-y max-y))
        aabb (geom/coords->aabb [(- min-x center-x) (- min-y center-y) 0] [(- max-x center-x) (- max-y center-y) 0])]
    aabb))

(defn- create-bone [parent-id rive-bone]
  ; rive-bone is of type Rive$Bone (Rive.java)
  (let [name (.-name rive-bone)
        x (.-posX rive-bone)
        y (.-posY rive-bone)
        rotation (.-rotation rive-bone)
        scale-x (.-scaleX rive-bone)
        scale-y (.-scaleY rive-bone)
        length (.-length rive-bone)
        parent-graph-id (g/node-id->graph-id parent-id)
        bone-tx-data (g/make-nodes parent-graph-id [bone [RiveBone :name name :position [x y protobuf/float-zero] :rotation rotation :scale [scale-x scale-y protobuf/float-one] :length length]]
                                   ; Hook this node into the parent's lists
                                   (g/connect bone :_node-id parent-id :nodes)
                                   (g/connect bone :bone parent-id :child-bones))]
    bone-tx-data))

(defn- tx-first-created [tx-data]
  (first (g/tx-data-added-node-ids tx-data)))

(defn- create-bone-hierarchy [parent-id bone]
  (let [bone-tx-data (create-bone parent-id bone)
        bone-id (first (g/tx-data-added-node-ids bone-tx-data))
        child-bones (.-children bone)
        children-tx-data (mapcat (fn [child] (create-bone-hierarchy bone-id child)) child-bones)]
    (concat bone-tx-data children-tx-data)))

(defn- create-bones [parent-id bones]
  ; bones is a list of root Rive$Bone (Rive.java)
  (mapcat (fn [bone] (create-bone-hierarchy parent-id bone)) bones))

(defn- create-state-machine-node [parent-id artboard state-machine]
  (when parent-id
    (let [parent-graph-id (g/node-id->graph-id parent-id)]
      (g/make-nodes parent-graph-id [sm [RiveStateMachineNode :name state-machine :artboard artboard]]
        (g/connect sm :_node-id parent-id :nodes)
        (g/connect sm :node-outline parent-id :child-outlines)))))

(defn- create-artboard-node [parent-id artboard state-machines]
  (let [parent-graph-id (g/node-id->graph-id parent-id)
        state-machines (vec (remove nil? (or state-machines [])))
        state-machines-label (join-or state-machines none-value-text)
        artboard-tx-data (g/make-nodes parent-graph-id [artboard-node [RiveArtboardNode :name artboard :state-machines state-machines-label]]
                          (g/connect artboard-node :_node-id parent-id :nodes)
                          (g/connect artboard-node :node-outline parent-id :child-outlines))
        artboard-id (tx-first-created artboard-tx-data)
        state-machine-tx-data (mapcat (fn [sm] (create-state-machine-node artboard-id artboard sm)) state-machines)]
    (concat artboard-tx-data state-machine-tx-data)))

(defn- create-artboard-nodes [parent-id artboards state-machines-by-artboard]
  (mapcat (fn [artboard]
            (create-artboard-node parent-id artboard (get state-machines-by-artboard artboard [])))
          artboards))

(defn- create-view-model-property-node [parent-id view-model property]
  (let [parent-graph-id (g/node-id->graph-id parent-id)
        name (:name property)
        type-name (string-or (:type-name property) "unknown")
        meta-data (string-or (:meta-data property) none-value-text)
        value (string-or (:value property) unknown-value-text)]
    (g/make-nodes parent-graph-id [prop [RiveViewModelPropertyNode :name name
                                         :view-model view-model
                                         :data-type type-name
                                         :value value
                                         :meta-data meta-data]]
      (g/connect prop :_node-id parent-id :nodes)
      (g/connect prop :node-outline parent-id :child-outlines))))

(defn- create-view-model-node [parent-id view-model properties instances default-instance]
  (let [parent-graph-id (g/node-id->graph-id parent-id)
        instances-label (join-or instances none-value-text)
        default-instance-label (string-or default-instance none-value-text)
        view-model-tx-data (g/make-nodes parent-graph-id [view-model-node [RiveViewModelNode :name view-model
                                                                           :default-instance default-instance-label
                                                                           :instances instances-label]]
                             (g/connect view-model-node :_node-id parent-id :nodes)
                             (g/connect view-model-node :node-outline parent-id :child-outlines))
        view-model-id (tx-first-created view-model-tx-data)
        property-tx-data (mapcat (fn [property]
                                   (create-view-model-property-node view-model-id view-model property))
                                 properties)]
    (concat view-model-tx-data property-tx-data)))

(defn- create-view-model-nodes [parent-id view-models view-model-properties view-model-instances default-view-model-info]
  (let [properties-by-name (view-model-properties-by-name view-model-properties)
        default-view-model (:view-model default-view-model-info)
        default-instance (:instance default-view-model-info)]
    (mapcat (fn [view-model]
              (let [properties (get properties-by-name view-model [])
                    instances (get view-model-instances view-model [])
                    view-model-default-instance (when (= view-model default-view-model)
                                                  default-instance)]
                (create-view-model-node parent-id view-model properties instances view-model-default-instance)))
            view-models)))

; Loads the .riv file
(defn- load-rive-file
  [project node-id resource]
  (let [content (resource->bytes resource)
        rive-handle (plugin-load-file content (resource/resource->proj-path resource))
        artboards (or (get-public-field rive-handle "artboards") [])
        view-models (or (get-public-field rive-handle "viewModels") [])
        view-model-properties (or (get-public-field rive-handle "viewModelProperties") [])
        view-model-properties-info (to-view-model-properties view-model-properties)
        view-model-instance-names (or (get-public-field rive-handle "viewModelInstanceNames") [])
        view-model-instances (to-view-model-instances view-model-instance-names)
        default-view-model-info (to-default-view-model-info (get-public-field rive-handle "defaultViewModelInfo"))
        state-machines (to-state-machines-map (get-public-field rive-handle "stateMachines"))

        _ (.Update rive-handle 0.0)
        bounds-obj (or (get-public-field rive-handle "bounds")
                       (get-public-field rive-handle "aabb"))
        bounds-text (if bounds-obj
                      (bounds->string bounds-obj)
                      none-value-text)
        aabb (if bounds-obj
               (convert-aabb bounds-obj)
               geom/null-aabb)
        bones (or (get-public-field rive-handle "bones") [])

        tx-data (concat
                 (g/set-property node-id :content content)
                 (g/set-property node-id :rive-handle rive-handle)
                 (g/set-property node-id :artboards artboards)
                 (g/set-property node-id :view-models view-models)
                 (g/set-property node-id :view-model-properties view-model-properties)
                 (g/set-property node-id :bounds bounds-text)
                 (g/set-property node-id :state-machines state-machines)
                 (g/set-property node-id :aabb aabb)
                 (g/set-property node-id :bones bones))

        artboard-outline-tx-data (create-artboard-nodes node-id artboards state-machines)
        view-model-outline-tx-data (create-view-model-nodes node-id view-models view-model-properties-info view-model-instances default-view-model-info)
        all-tx-data (concat tx-data
                            artboard-outline-tx-data
                            view-model-outline-tx-data
                            (create-bones node-id bones))]
    all-tx-data))

(set! *warn-on-reflection* true)

; Rive Scene
; .rivescene (The "data" file) which in turn points to the .riv file

; Node defs
(g/defnk produce-rivescene-save-value [rive-file-resource]
  ; rive-file-resource may be nil if the :scene isn't set (as seen in the template.rivescene)
  (protobuf/make-map-without-defaults rive-scene-pb-class
    :scene (resource/resource->proj-path rive-file-resource)))

(defn- prop-resource-error [_node-id prop-kw prop-value prop-name]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-missing? prop-value prop-name))

; The properties of the .rivescene (see RiveSceneDesc in rive_ddf.proto)
; The "scene" should point to a .riv file

(defn- validate-rivescene-riv-file [_node-id rive-file]
  (prop-resource-error _node-id :scene rive-file "Riv File"))

(g/defnk produce-rivescene-own-build-errors [_node-id rive-file]
  (g/package-errors _node-id
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
    {:resource resource :content (protobuf/map->bytes rive-scene-pb-class pb)}))


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

(defn- renderable->handle [renderable]
  (get-in renderable [:user-data :rive-file-handle]))

(defn- renderable->texture-set-pb [renderable]
  (get-in renderable [:user-data :texture-set-pb]))

(vtx/defvertex vtx-textured
 (vec2 position)
 (vec2 texcoord0))

(def ^:private fallback-quad-vertices
  (float-array
    [-1.0 -1.0 0.0 0.0
      1.0 -1.0 1.0 0.0
     -1.0  1.0 0.0 1.0
      1.0 -1.0 1.0 0.0
      1.0  1.0 1.0 1.0
     -1.0  1.0 0.0 1.0]))

(defonce ^:private fullscreen-quad-vertices
  (delay
    (or (plugin-get-fullscreen-quad-vertices)
        fallback-quad-vertices)))

(defonce ^:private fullscreen-quad-vertex-buffer
  (delay
    (let [vertices ^floats @fullscreen-quad-vertices]
      (vtx/wrap-vertex-buffer vtx-textured :static (FloatBuffer/wrap vertices)))))

(def ^:private identity-matrix4d
  (doto (Matrix4d.) (.setIdentity)))

(defn- copy-bgra->abgr! [^bytes src ^bytes dst pixel-count]
  (loop [i 0 si 0 di 0]
    (when (< i pixel-count)
      (let [b (aget src si)
            g (aget src (+ si 1))
            r (aget src (+ si 2))
            a (aget src (+ si 3))]
        (aset-byte dst di a)
        (aset-byte dst (+ di 1) b)
        (aset-byte dst (+ di 2) g)
        (aset-byte dst (+ di 3) r)
        (recur (inc i) (+ si 4) (+ di 4))))))

(defn- rive-texture->buffered-image [texture]
  (when texture
    (let [width (get-public-field texture "width")
          height (get-public-field texture "height")
          data (get-public-field texture "data")]
      (when (and data (> width 0) (> height 0))
        (let [image (BufferedImage. width height BufferedImage/TYPE_4BYTE_ABGR)
              raster (.getRaster image)
              ^DataBufferByte buffer (.getDataBuffer raster)
              dst (.getData buffer)
              pixel-count (* width height)]
          (when (>= (alength data) (* pixel-count 4))
            (copy-bgra->abgr! data dst pixel-count)
            image))))))

(defn- rive-texture->gpu-texture [request-id texture default-tex-params]
  (when-let [image (rive-texture->buffered-image texture)]
    (try
      (let [texture-image (tex-gen/make-preview-texture-image image nil)
            gpu-texture (texture/texture-images->gpu-texture request-id [texture-image] {:min-filter gl/nearest
                                                                                         :mag-filter gl/nearest})]
        (if default-tex-params
          (texture/set-params gpu-texture default-tex-params)
          gpu-texture))
      (catch Exception _
        nil))))

(set! *warn-on-reflection* false)

(defn renderable->render-objects [renderable]
  (let [node-id (:node-id renderable)
        handle (renderable->handle renderable)
        texture-set-pb (renderable->texture-set-pb renderable)
        vb-data (get-public-field handle "vertices")
        ib-data (get-public-field handle "indices")
        render-objects (get-public-field handle "renderObjects")]
    (when (and vb-data ib-data render-objects)
      (let [vb-data-float-buffer (FloatBuffer/wrap vb-data)
            vb (vtx/wrap-vertex-buffer vtx-textured :static vb-data-float-buffer)
            ib-data-int-buffer (IntBuffer/wrap ib-data)
            ib-buffer-data (buffers/make-buffer-data ib-data-int-buffer)
            ib (attribute/make-index-buffer node-id ib-buffer-data :static)]
        {:vertex-buffer vb
         :index-buffer ib
         :render-objects render-objects
         :handle handle
         :texture-set-pb texture-set-pb
         :renderable renderable}))))

(set! *warn-on-reflection* true)

(defn collect-render-groups [renderables]
  (keep renderable->render-objects renderables))

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
(defn- stencil-func->gl-func
  ^long [^long func]
  (case func
    0 GL/GL_NEVER
    1 GL/GL_LESS
    2 GL/GL_LEQUAL
    3 GL/GL_GREATER
    4 GL/GL_GEQUAL
    5 GL/GL_EQUAL
    6 GL/GL_NOTEQUAL
    7 GL/GL_ALWAYS))

(defn- stencil-op->gl-op
  ^long [^long op]
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

    (gl/gl-draw-elements gl gl-prim-type GL/GL_UNSIGNED_INT start count)))

  (set! *warn-on-reflection* true)

; Borrowed from gui_clipping.clj
(defn- setup-gl [^GL2 gl]
  (.glEnable gl GL/GL_STENCIL_TEST)
  (.glClear gl GL/GL_STENCIL_BUFFER_BIT))

(defn- restore-gl [^GL2 gl]
  (.glDisable gl GL/GL_STENCIL_TEST)
  (.glColorMask gl true true true true))

(defn- render-rive-quad [^GL2 gl render-args renderable]
  (let [node-id (:node-id renderable)
        user-data (:user-data renderable)
        handle (:rive-file-handle user-data)
        pass (:pass render-args)
        blend-mode (:blend-mode user-data)
        shader (if (= pass/selection pass)
                 (or (:selection-shader user-data)
                     (:blit-shader user-data)
                     (:shader user-data))
                 (or (:blit-shader user-data) (:shader user-data)))
        texture (when handle (plugin-get-texture handle))
        gpu-texture (or (rive-texture->gpu-texture node-id texture (:default-tex-params user-data))
                        (:gpu-texture user-data)
                        texture/white-pixel)
        vb @fullscreen-quad-vertex-buffer
        vertex-binding (vtx/use-with [node-id ::rive-quad] vb shader)]
    (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
      (setup-gl gl)
      (when (= pass/selection pass)
        (shader/set-uniform shader gl "id_color" (:id-color render-args)))
      (shader/set-uniform shader gl "world_view_proj" identity-matrix4d)
      (when (= pass/transparent pass)
        (gl/set-blend-mode gl blend-mode))
      (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 6)
      (restore-gl gl))))

(defn- render-rive-scenes [^GL2 gl render-args renderables _rcount]
  (let [first-renderable (first renderables)
        render-args (assoc render-args :id-color (scene-picking/renderable-picking-id-uniform first-renderable))]
    (doseq [renderable renderables]
      (let [user-data (:user-data renderable)
            handle (:rive-file-handle user-data)]
        (when handle
          (plugin-update-file handle 0.0))
        (render-rive-quad gl render-args renderable)))))

(g/defnk produce-main-scene [_node-id material-shader selection-material-shader rive-file-handle aabb rive-scene-pb texture-set-pb default-tex-params]
  (when rive-file-handle
    (let [blend-mode :blend-mode-alpha]
      (assoc {:node-id _node-id :aabb aabb}
             :renderable {:render-fn render-rive-scenes
                          :tags #{:rive}
                          :batch-key material-shader
                          :select-batch-key _node-id
                          :user-data {:rive-scene-pb rive-scene-pb
                                      :rive-file-handle rive-file-handle
                                      :aabb aabb
                                      :shader material-shader
                                      :selection-shader selection-material-shader
                                      :default-tex-params default-tex-params
                                      :texture-set-pb texture-set-pb
                                      :blend-mode blend-mode}
                          :passes [pass/transparent pass/selection]}))))

(defn- make-rive-outline-scene [_node-id aabb]
  {:aabb aabb
   :node-id _node-id
   :renderable (render-util/make-aabb-outline-renderable #{:rive})})

(g/defnk produce-rivescene [_node-id rive-file-handle aabb main-scene]
  (when rive-file-handle
    (if (some? main-scene)
      (-> main-scene
          (assoc :children [(make-rive-outline-scene _node-id aabb)]))
      {:node-id _node-id :aabb aabb})))

;; Represents a .rivescene file, loaded from a Rive$RiveSceneDesc Protobuf message.
(g/defnode RiveSceneNode
  (inherits resource-node/ResourceNode)

  (property rive-file resource/Resource ; Required protobuf field.
            (value (gu/passthrough rive-file-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :rive-file-resource]
                                            [:content :rive-scene]
                                            [:rive-handle :rive-file-handle]
                                            [:artboards :rive-artboards]
                                            [:state-machines :rive-state-machines]
                                            [:bounds :rive-file-bounds]
                                            [:aabb :aabb]
                                            [:node-outline :source-outline]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext rive-file-ext}))
            (dynamic error (g/fnk [_node-id rive-file]
                                  (validate-rivescene-riv-file _node-id rive-file))))

  (property bounds g/Str
            (value (gu/passthrough rive-file-bounds))
            (dynamic label (g/constantly "Bounds"))
            (dynamic read-only? (g/constantly true)))

  ;; This property isn't visible, but here to allow us to preview the .rivescene
  (property material resource/Resource ; Default assigned in load-fn.
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:shader :material-shader])))
            (dynamic visible (g/constantly false)))

  ;; This property isn't visible, but here to allow us to perform scene view
  ;; picking against the .rivescene
  (property selection-material resource/Resource ; Default assigned in load-fn.
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:shader :selection-material-shader])))
            (dynamic visible (g/constantly false)))

  (input material-shader ShaderLifecycle)
  (input selection-material-shader ShaderLifecycle)
  (input rive-file-resource resource/Resource)
  (input rive-file-handle g/Any)
  (input rive-artboards g/Any)
  (input rive-state-machines g/Any)
  (input rive-file-bounds g/Str)
  (input aabb g/Any)

  (input texture-set-pb g/Any)
  (output texture-set-pb g/Any :cached (gu/passthrough texture-set-pb))

  (input anim-data g/Any)
  (input default-tex-params g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)
  (input rive-scene g/Any)

  (output save-value g/Any :cached produce-rivescene-save-value)
  (output own-build-errors g/Any produce-rivescene-own-build-errors)
  (output build-targets g/Any :cached produce-rivescene-build-targets)
  (output rive-scene-pb g/Any :cached produce-rivescene-pb)
  (output main-scene g/Any :cached produce-main-scene)
  (output scene g/Any :cached produce-rivescene)
  (output rive-file-handle g/Any :cached (gu/passthrough rive-file-handle))
  (output rive-artboards g/Any :cached (gu/passthrough rive-artboards))
  (output rive-state-machines g/Any :cached (gu/passthrough rive-state-machines))
  (output aabb g/Any :cached (gu/passthrough aabb)))

; .rivescene
(defn load-rive-scene [project self resource rive-scene-desc]
  {:pre [(map? rive-scene-desc)]} ; Rive$RiveSceneDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (g/set-property self :material (resolve-resource default-blit-material-proj-path))
      (g/set-property self :selection-material (resolve-resource selection-material-proj-path))
      (gu/set-properties-from-pb-map self rive-scene-pb-class rive-scene-desc
        rive-file (resolve-resource :scene)))))


;
; .rivemodel (The "instance" file)
;

(g/defnk produce-rivemodel-save-value [rive-scene-resource artboard default-state-machine material-resource blit-material-resource blend-mode auto-bind coordinate-system artboard-fit artboard-alignment]
  (protobuf/make-map-without-defaults rive-model-pb-class
    :scene (resource/resource->proj-path rive-scene-resource)
    :material (resource/resource->proj-path material-resource)
    :blit-material (resource/resource->proj-path blit-material-resource)
    :artboard artboard
    :default-state-machine default-state-machine
    :blend-mode blend-mode
    :auto-bind auto-bind
    :coordinate-system coordinate-system
    :artboard-fit artboard-fit
    :artboard-alignment artboard-alignment))

(defn- validate-model-artboard [node-id rive-scene rive-artboards artboard]
  (when (and rive-scene (not-empty artboard))
    (validation/prop-error :fatal node-id :artboard
                           (fn [id ids]
                             (when-not (contains? ids id)
                               (format "Artboard '%s' could not be found in the specified rive scene" id)))
                           artboard
                           (set rive-artboards))))

(defn- validate-model-default-state-machine [node-id rive-scene state-machine-ids default-state-machine]
  (when (and rive-scene (not-empty default-state-machine))
    (validation/prop-error :fatal node-id :default-state-machine
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "state machine '%s' could not be found in the specified artboard or rive scene" anim)))
                           default-state-machine
                           (set state-machine-ids))))

(defn- validate-model-material [node-id material]
  (prop-resource-error node-id :material material "Material"))

(defn- validate-model-rive-scene [node-id rive-scene]
  (prop-resource-error node-id :scene rive-scene "Rive Scene"))

(g/defnk produce-model-own-build-errors [_node-id artboard default-state-machine material rive-artboards rive-state-machines rive-scene]
  (let [state-machine-ids (state-machines-for-artboard rive-state-machines artboard rive-artboards)]
   (g/package-errors _node-id
                    (validate-model-material _node-id material)
                    (validate-model-rive-scene _node-id rive-scene)
                    (validate-model-artboard  _node-id rive-scene rive-artboards artboard)
                    (validate-model-default-state-machine _node-id rive-scene state-machine-ids default-state-machine))))

(defn- build-rive-model [resource dep-resources user-data]
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes rive-model-pb-class pb)}))

(g/defnk produce-model-build-targets [_node-id own-build-errors resource save-value rive-scene-resource material-resource blit-material-resource dep-build-targets]
  (g/precluding-errors own-build-errors
                       (let [dep-build-targets (flatten dep-build-targets)
                             deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
                             dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:scene rive-scene-resource] [:material material-resource] [:blit-material blit-material-resource]])]
                         [(bt/with-content-hash
                            {:node-id _node-id
                             :resource (workspace/make-build-resource resource)
                             :build-fn build-rive-model
                             :user-data {:proto-msg save-value
                                         :dep-resources dep-resources}
                             :deps dep-build-targets})])))

;; Represents a .rivemodel file, loaded from a Rive$RiveModelDesc Protobuf message.
(g/defnode RiveModelNode
  (inherits resource-node/ResourceNode)

  (input rive-scene-resource resource/Resource)
  (input material-resource resource/Resource)
  (input blit-material-resource resource/Resource)
  (input rive-state-machines g/Any)
  (input rive-artboards g/Any)

  (property rive-scene resource/Resource ; Required protobuf field.
            (value (gu/passthrough rive-scene-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :rive-scene-resource]
                                            [:texture-set-pb :texture-set-pb]
                                            [:main-scene :rive-main-scene]
                                            [:rive-file-handle :rive-file-handle]
                                            [:rive-artboards :rive-artboards]
                                            [:rive-state-machines :rive-state-machines]
                                            [:build-targets :dep-build-targets]
                                            [:anim-data :anim-data])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext rive-scene-ext}))
            (dynamic error (g/fnk [_node-id rive-scene]
                                  (validate-model-rive-scene _node-id rive-scene))))
  (property blend-mode g/Any (default (protobuf/default rive-model-pb-class :blend-mode))
            (dynamic tip (validation/blend-mode-tip blend-mode blend-mode-pb-class))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox blend-mode-pb-class))))
  (property material resource/Resource ; Default assigned in load-fn.
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
  ; not visible/editable
  (property blit-material resource/Resource ; Default assigned in load-fn.
            (value (gu/passthrough blit-material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :blit-material-resource]
                                            [:shader :blit-material-shader]
                                            [:samplers :blit-material-samplers]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "material"}))
            (dynamic error (g/fnk [_node-id material]
                                  (validate-model-material _node-id material)))
            (dynamic visible (g/constantly false)))

  (property default-state-machine g/Str (default (protobuf/default rive-model-pb-class :default-state-machine))
            (dynamic error (g/fnk [_node-id rive-state-machines artboard rive-artboards default-state-machine rive-scene]
                             (validate-model-default-state-machine _node-id rive-scene
                                                                   (state-machines-for-artboard rive-state-machines artboard rive-artboards)
                                                                   default-state-machine)))
            (dynamic edit-type (g/fnk [rive-state-machines artboard rive-artboards]
                                 (properties/->choicebox (cons "" (state-machines-for-artboard rive-state-machines artboard rive-artboards))))))

  (property artboard g/Str (default (protobuf/default rive-model-pb-class :artboard))
          (dynamic error (g/fnk [_node-id rive-artboards artboard rive-scene]
                                (validate-model-artboard _node-id rive-scene rive-artboards artboard)))
          (dynamic edit-type (g/fnk [rive-artboards] (properties/->choicebox (cons "" rive-artboards)))))

  (property auto-bind g/Bool (default (protobuf/default rive-model-pb-class :auto-bind)))

  (property coordinate-system g/Any (default (protobuf/default rive-model-pb-class :coordinate-system))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox coordinate-system-pb-class))))

  (property artboard-fit g/Any (default (protobuf/default rive-model-pb-class :artboard-fit))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox artboard-fit-pb-class))))

  (property artboard-alignment g/Any (default (protobuf/default rive-model-pb-class :artboard-alignment))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox artboard-alignment-pb-class))))

  (input dep-build-targets g/Any :array)
  (input rive-file-handle g/Any)
  (input rive-main-scene g/Any)
  (input texture-set-pb g/Any)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input blit-material-shader ShaderLifecycle)
  (input blit-material-samplers g/Any)
  (input default-tex-params g/Any)
  (input anim-data g/Any)

  ; (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
  ;                            (or (some-> material-samplers first material/sampler->tex-params)
  ;                                default-tex-params)))
  (output anim-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output state-machine-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached (g/fnk [_node-id rive-file-handle artboard rive-main-scene material-shader blit-material-shader]
                                     (if (and (some? material-shader) (some? (:renderable rive-main-scene)))
                                       (let [aabb (:aabb rive-main-scene)
                                             rive-scene-node-id (:node-id rive-main-scene)
                                             _ (plugin-set-artboard rive-file-handle artboard)]
                                         (-> rive-main-scene
                                             (assoc-in [:renderable :user-data :shader] material-shader)
                                             (assoc-in [:renderable :user-data :blit-shader] blit-material-shader)
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

                                                            (resource/resource? scene)
                                                            (assoc :link scene :outline-reference? false))))
  (output save-value g/Any :cached produce-rivemodel-save-value)
  (output own-build-errors g/Any produce-model-own-build-errors)
  (output build-targets g/Any :cached produce-model-build-targets))


(defn register-resource-types [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :ext rive-scene-ext
      :label "Rive Scene"
      :node-type RiveSceneNode
      :ddf-type rive-scene-pb-class
      :load-fn load-rive-scene
      :icon rive-scene-icon
      :category (localization/message "resource.category.resources")
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}}
      :template "/defold-rive/assets/template.rivescene")
    (resource-node/register-ddf-resource-type workspace
      :ext rive-model-ext
      :label "Rive Model"
      :node-type RiveModelNode
      :ddf-type rive-model-pb-class
      :load-fn load-rive-model
      :icon rive-model-icon
      :category (localization/message "resource.category.components")
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
      :view-types [:default])))

; The plugin
(defn load-plugin-rive [workspace]
  (g/transact (concat (register-resource-types workspace))))

(defn return-plugin []
  (fn [x] (load-plugin-rive x)))
(return-plugin)
