;; Copyright 2020 The Defold Foundation
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(prn "SPINE EXTENSION PLUGIN!")

(ns editor.spine
  (:require [clojure.java.io :as io]
            [editor.protobuf :as protobuf]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.geom :as geom]
            [editor.material :as material]
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
            [editor.json :as json]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.rig :as rig]
            [editor.system :as system]
            [service.log :as log]
            [internal.util :as util])
  (:import [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]
           [com.dynamo.bob.util BezierUtil RigUtil$Transform]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]
           [com.jogamp.opengl GL GL2]
           [java.util HashSet]
           [java.net URL]
           [javax.vecmath Matrix4d Point2d Point3d Quat4d Vector3d Vector4d Tuple3d Tuple4d]))

; TODO: Centralize this code so it can be updated when plugins are added or rebuilt
(def dcl
  (let [loader (clojure.lang.DynamicClassLoader. (.getContextClassLoader (Thread/currentThread)))
        url (URL. (format "file://%s/shared/java/fmt-spine.jar" (system/defold-unpack-path)))]
    (.addURL loader url)
    loader))

(defn dynamically-load-class!
  [class-loader class-name]
  (Class/forName class-name true class-loader))


(set! *warn-on-reflection* true)

(def spine-scene-icon "icons/32/Icons_16-Spine-scene.png")
(def spine-model-icon "icons/32/Icons_15-Spine-model.png")
(def spine-bone-icon "icons/32/Icons_S_13_radiocircle.png")

(def spine-scene-ext "spinescene")
(def spine-model-ext "spinemodel")

(def slot-signal-unchanged 0x10CCED)

; Helper to do an .indexOf with a type checked first arg.
(defn- index-of
  [^java.util.List list value]
  (.indexOf list value))


; Node defs

(g/defnk produce-save-value [spine-json-resource atlas-resource sample-rate]
  {:spine-json (resource/resource->proj-path spine-json-resource)
   :atlas (resource/resource->proj-path atlas-resource)
   :sample-rate sample-rate})

(defprotocol Interpolator
  (interpolate [v0 v1 t]))

(extend-protocol Interpolator
  Double
  (interpolate [v0 v1 t]
    (+ v0 (* t (- v1 v0))))
  Long
  (interpolate [v0 v1 t]
    (+ v0 (* t (- v1 v0))))
  Point3d
  (interpolate [v0 v1 t]
    (doto (Point3d.) (.interpolate ^Tuple3d v0 ^Tuple3d v1 ^double t)))
  Vector3d
  (interpolate [v0 v1 t]
    (doto (Vector3d.) (.interpolate ^Tuple3d v0 ^Tuple3d v1 ^double t)))
  Vector4d
  (interpolate [v0 v1 t]
    (doto (Vector4d.) (.interpolate ^Tuple4d v0 ^Tuple4d v1 ^double t)))
  Quat4d
  (interpolate [v0 v1 t]
    (doto (Quat4d.) (.interpolate ^Quat4d v0 ^Quat4d v1 ^double t))))

(defn- ->interpolatable [pb-field v]
  (case pb-field
    :positions (doto (Point3d.) (math/clj->vecmath v))
    :rotations (doto (Quat4d. 0 0 0 1) (math/clj->vecmath v))
    :scale     (doto (Vector3d.) (math/clj->vecmath v))
    :slot-colors (doto (Vector4d.) (math/clj->vecmath v))
    :mix       v))

(defn- interpolatable-> [pb-field interpolatable]
  (case pb-field
    (:positions :rotations :scale :slot-colors) (math/vecmath->clj interpolatable)
    :mix                                        interpolatable))

(def default-vals {:positions [0 0 0]
                   :rotations [0 0 0 1]
                   :scale [1 1 1]
                   :slot-colors [1 1 1 1]
                   :attachment true
                   :order-offset slot-signal-unchanged
                   :mix 1.0
                   :positive true}) ;; spine docs say assume false, but implementations actually default to true

(defn- curve [[x0 y0 x1 y1] t]
  (let [t (BezierUtil/findT t 0.0 x0 x1 1.0)]
    (BezierUtil/curve t 0.0 y0 y1 1.0)))

(defn- angle->clj-quat [angle]
  (let [half-rad (/ (* 0.5 angle Math/PI) 180.0)
        c (Math/cos half-rad)
        s (Math/sin half-rad)]
    [0 0 s c]))

(defn- angle->quat [angle]
  (let [[x y z w] (angle->clj-quat angle)]
    (Quat4d. x y z w)))

(defn- hex-pair->color [^String hex-pair]
  (/ (Integer/parseInt hex-pair 16) 255.0))

(defn- hex->color [^String hex]
  (let [n (count hex)]
    (when (and (not= n 6) (not= n 8))
      (throw (ex-info (format "Invalid value for color: '%s'" hex) {:hex hex})))
    [(hex-pair->color (subs hex 0 2))
     (hex-pair->color (subs hex 2 4))
     (hex-pair->color (subs hex 4 6))
     (if (= n 8) (hex-pair->color (subs hex 6 8)) 1.0)]))

(defn- key->value [type key]
  (case type
    "translate"    [(get key "x" 0) (get key "y" 0) 0]
    "rotate"       (angle->clj-quat (get key "angle" 0))
    "scale"        [(get key "x" 1) (get key "y" 1) 1]
    "color"        (hex->color (get key "color" "ffffffff"))
    "drawOrder"    (get key "offset")
    "mix"          (get key "mix")
    "bendPositive" (get key "bendPositive")))

(def timeline-type->pb-field {"translate" :positions
                              "rotate" :rotations
                              "scale" :scale
                              "color" :slot-colors
                              "attachment" :mesh-attachment
                              "drawOrder" :order-offset
                              "mix" :mix
                              "bendPositive" :positive})

(def timeline-type->vector-type {"translate" :double
                                 "rotate" :double
                                 "scale" :double
                                 "color" :double
                                 "attachment" :long
                                 "drawOrder" :long
                                 "mix" :double
                                 "bendPositive" :boolean})

(def timeline-type->key-stride {"translate" 3
                                "rotate" 4
                                "scale" 3
                                "color" 4
                                "attachment" 1
                                "drawOrder" 1
                                "mix" 1
                                "bendPositive" 1})

(defn key->curve-data
  [key]
  ; NOTE:
  ; We've seen Spine scenes produced from non-conforming exporters that would
  ; write a null value in place of "stepped". We treat this the same way the
  ; old editor and the official Spine runtimes do - we consider the value to be
  ; unspecified and fall back on the default of "linear".
  (let [curve (get key "curve")]
    (cond
      (= "stepped" curve) nil
      (and (vector? curve) (= 4 (count curve))) curve
      (number? curve) [curve (get key "c2" 0.0) (get key "c3" 0.0) (get key "c4" 1.0)]
      :else [0 0 1 1])))

(defn- sample [type keys duration sample-rate spf val-fn default-val interpolate?]
  (let [pb-field (timeline-type->pb-field type)
        val-fn (or val-fn key->value)
        default-val (if (nil? default-val) (default-vals pb-field) default-val)
        sample-count (Math/ceil (+ 1 (* duration sample-rate)))
        ; Sort keys
        keys (vec (sort-by #(get % "time" 0.0) keys))
        vals (mapv #(if-some [v (val-fn type %)] v default-val) keys)
        ; Add dummy key for 0
        [keys vals] (if (or (empty? keys) (> (get (first keys) "time" 0.0) 0.0))
                      [(vec (cons {"time" 0.0
                                   "curve" "stepped"} keys))
                       (vec (cons default-val vals))]
                      [keys vals])
        ; Convert keys to vecmath vals
        vals (if interpolate?
               (mapv #(->interpolatable pb-field %) vals)
               vals)
        ; Accumulate key counts into sample slots
        key-counts (reduce (fn [counts key]
                             (let [sample (int (* (get key "time" 0.0) sample-rate))]
                               (update-in counts [sample] inc))) (vec (repeat sample-count 0)) keys)
        ; LUT from sample to key index
        sample->key-idx (loop [key-counts key-counts
                               v (transient [])
                               offset 0]
                          (if-let [key-count (first key-counts)]
                            (recur (rest key-counts) (conj! v offset) (+ (int key-count) offset))
                            (persistent! v)))]
    (reduce (fn [ret sample]
              (let [cursor (* spf sample)
                    idx1 (get sample->key-idx sample)
                    idx0 (dec idx1)
                    k0 (get keys idx0)
                    v0 (get vals idx0)
                    k1 (get keys idx1)
                    v1 (get vals idx1)
                    v (if (and k0 (not interpolate?))
                        v0
                        (if (some? k1)
                          (if (>= cursor (get k1 "time" 0.0))
                            v1
                            (if-let [c (key->curve-data k0)]
                              (let [t (/ (- cursor (get k0 "time" 0.0)) (- (get k1 "time" 0.0) (get k0 "time" 0.0)))
                                    rate (curve c t)]
                                (interpolate v0 v1 rate))
                              v0))
                          v0))
                    v (if interpolate?
                        (interpolatable-> pb-field v)
                        v)]
                (if (vector? v)
                  (reduce conj ret v)
                  (conj ret v))))
            (vector-of (timeline-type->vector-type type))
            (range sample-count))))

; Calls the regular sample function then duplicates the last keyframe so that
; the linear interpolation works correctly in runtime.
(defn- sample-with-dup-frame [type keys duration sample-rate spf val-fn default-val interpolate?]
  (let [keyframes (sample type keys duration sample-rate spf val-fn default-val interpolate?)]
    (into keyframes (take-last (timeline-type->key-stride type) keyframes))))

; This value is used to counter how the key values for rotations are interpreted in spine:
; * All values are modulated into the interval 0 <= x < 360
; * If keys k0 and k1 have a difference > 180, the second key is adjusted with +/- 360 to lessen the difference to < 180
; ** E.g. k0 = 0, k1 = 270 are interpolated as k0 = 0, k1 = -90
(defn- wrap-angles [type keys]
  (if (= type "rotate")
    (loop [prev-key nil
           keys keys
           wrapped []]
      (if-let [key (first keys)]
        (let [key (if prev-key
                    (let [angle (get key "angle" 0.0)
                          diff (double (- angle (get prev-key "angle" 0.0)))]
                      (if (> (Math/abs diff) 180.0)
                        (assoc key "angle" (+ angle (* (Math/signum diff) (- 360.0))))
                        key))
                    key)]
          (recur key (rest keys) (conj wrapped key)))
        wrapped))
    keys))

(defn- build-tracks [timelines duration sample-rate spf bone-id->index]
  (let [tracks-by-bone (reduce-kv (fn [m bone-name timeline]
                                    (let [bone-index (bone-id->index (murmur/hash64 bone-name))]
                                      (reduce-kv (fn [m type keys]
                                                   (if-let [field (timeline-type->pb-field type)]
                                                     (let [pb-track {field (sample-with-dup-frame type (wrap-angles type keys) duration sample-rate spf nil nil true)
                                                                     :bone-index bone-index}]
                                                       (update-in m [bone-index] merge pb-track))
                                                     m))
                                                 m timeline)))
                                  {} timelines)]
    (sort-by :bone-index (vals tracks-by-bone))))

(defn- build-mesh-tracks [slot-timelines do-timelines duration sample-rate spf base-slots]
  (let [; Reshape do-timelines into slot-timelines
        do-by-slot (into {} (map (fn [[slot timeline]]
                                   [slot {"drawOrder" timeline}])
                             (reduce (fn [m timeline]
                                      (let [t (get timeline "time" 0.0)
                                            explicit (reduce (fn [m offset]
                                                               (assoc m (get offset "slot") [{"time" t "offset" (get offset "offset")}]))
                                                             {} (get timeline "offsets"))
                                            ; Supply implicit slots with 0 in offset
                                            ; We set them to the constant slot-signal-unchanged which signal
                                            ; to the runtime that these slots were not changed.
                                            ; (We can't set them to 0 here, since this would mean that they
                                            ;  MUST not change/move at runtime.)
                                            all (reduce (fn [m slot]
                                                          (if (not (contains? m slot))
                                                            (assoc m slot [{"time" t "offset" slot-signal-unchanged}])
                                                            m))
                                                        explicit (keys m))]
                                        (merge-with into m all)))
                                    {} do-timelines)))
        slot-timelines (merge-with merge slot-timelines do-by-slot)
        tracks-by-slot (reduce-kv (fn [m slot timeline]
                                    (let [slot-data (get base-slots slot)
                                          default-attachment-name (:default-attachment slot-data)
                                          ^java.util.List attachment-names (:attachment-names slot-data)
                                          default-attachment-index (index-of attachment-names default-attachment-name)
                                          tracks (reduce-kv (fn [track type keys]
                                                              (let [interpolate? (= type "color")
                                                                    val-fn (when (= type "attachment")
                                                                             (fn [_ key]
                                                                               (index-of attachment-names (get key "name"))))
                                                                    default-val (when (= type "attachment")
                                                                                  default-attachment-index)
                                                                    field (timeline-type->pb-field type)
                                                                    ; Below :mesh-slot should point to the slot index that the track will change,
                                                                    ; this is stored in the :draw-order of a slot. The :draw-order is essentially
                                                                    ; just an increasing integer from 0 created when we read the base slots from
                                                                    ; the input file.
                                                                    pb-track {:mesh-slot (:draw-order slot-data)
                                                                              field (sample-with-dup-frame type keys duration sample-rate spf val-fn default-val interpolate?)}]
                                                                (merge track pb-track)))
                                                            {} timeline)]
                                      (assoc m slot tracks)))
                                  {} slot-timelines)]
    (flatten (vals tracks-by-slot))))

(defn- build-event-tracks
  [events event-name->event-props]
  (mapv (fn [[event-name keys]]
          (let [default (merge {"int" 0 "float" 0 "string" ""} (get event-name->event-props event-name))]
            {:event-id (murmur/hash64 event-name)
             :keys (mapv (fn [k]
                           {:t (get k "time" 0.0)
                            :integer (get k "int" (get default "int"))
                            :float (get k "float" (get default "float"))
                            :string (murmur/hash64 (get k "string" (get default "string")))})
                         keys)}))
        (reduce (fn [m e]
                  (update-in m [(get e "name")] conj e))
                {} events)))

(defn- build-ik-tracks
  [ik-timelines duration sample-rate spf ik-id->index]
  (->> ik-timelines
       (map (fn [[ik-name key-frames]]
              {:ik-index (ik-id->index (murmur/hash64 ik-name))
               :mix      (sample-with-dup-frame "mix" key-frames duration sample-rate spf nil nil true)
               :positive (sample-with-dup-frame "bendPositive" key-frames duration sample-rate spf nil nil false)}))
       (sort-by :ik-index)
       (vec)))

(defn- anim-duration
  [anim]
  (reduce max 0
          (concat (for [[bone-name timelines] (get anim "bones")
                        [timeline-type key-frames] timelines
                        {:strs [time]} key-frames
                        :when (some? time)]
                    time)
                  (for [[slot-name timelines] (get anim "slots")
                        [timeline-type key-frames] timelines
                        {:strs [time]} key-frames
                        :when (some? time)]
                    time)
                  (for [[ik-name key-frames] (get anim "ik")
                        {:strs [time]} key-frames
                        :when (some? time)]
                    time)
                  (for [{:strs [time]} (get anim "events")
                        :when (some? time)]
                    time)
                  (for [{:strs [time]} (get anim "drawOrder")
                        :when (some? time)]
                    time))))

(defn- bone->transform [bone]
  (let [t (RigUtil$Transform.)]
    (math/clj->vecmath (.position t) (:position bone))
    (math/clj->vecmath (.rotation t) (:rotation bone))
    (math/clj->vecmath (.scale t) (:scale bone))
    t))

(defn- normalize-weights [weights]
  (let [total-weight (reduce (fn [total w]
                               (+ total (:weight w)))
                             0 weights)]
    (mapv (fn [w] (update w :weight #(/ % total-weight))) weights)))

(def ^:private ^TextureSetGenerator$UVTransform uv-identity (TextureSetGenerator$UVTransform.))

(defn- attachment->mesh [attachment base-slots anim-data bones-remap bone-index->world]
  (when anim-data
    (let [type (get attachment "type" "region")
          slot-name (get attachment "slot-name")
          slot-data (get base-slots slot-name)
          world ^RigUtil$Transform (:bone-world slot-data)
          anim-id (get attachment "name")
          uv-trans (or ^TextureSetGenerator$UVTransform (first (get-in anim-data [anim-id :uv-transforms])) uv-identity)
          mesh (case type
                 "region"
                 (let [local (doto (RigUtil$Transform.)
                               (-> (.position) (.set (get attachment "x" 0) (get attachment "y" 0) 0))
                               (-> (.rotation) (.set ^Quat4d (angle->quat (get attachment "rotation" 0))))
                               (-> (.scale) (.set (get attachment "scaleX" 1) (get attachment "scaleY" 1) 1)))
                       world (doto (RigUtil$Transform. world)
                               (.mul local))
                       width (get attachment "width" 0)
                       height (get attachment "height" 0)
                       vertices (flatten (for [x [-0.5 0.5]
                                               y [-0.5 0.5]
                                               :let [p (Point3d. (* x width) (* y height) 0)
                                                     uv (Point2d. (+ x 0.5) (- 0.5 y))]]
                                           (do
                                             (.apply world p)
                                             (.apply uv-trans uv)
                                             [(.x p) (.y p) (.z p) (.x uv) (.y uv)])))]
                   {:positions (flatten (partition 3 5 vertices))
                    :texcoord0 (flatten (partition 2 5 (drop 3 vertices)))
                    :position-indices [0 1 2 2 1 3]
                    :weights (take 16 (cycle [1 0 0 0]))
                    :bone-indices (take 16 (cycle [(:bone-index slot-data) 0 0 0]))
                    :mesh-color (hex->color (get attachment "color" "ffffffff"))})
                 ("mesh" "skinnedmesh" "weightedmesh")
                 (let [vertices (get attachment "vertices" [])
                       uvs (get attachment "uvs" [])
                       ; A mesh is weighted if the number of vertices > number of UVs.
                       ; Both are x, y pairs if the mesh is not weighted.
                       weighted? (> (count vertices) (count uvs))
                       ; Use uvs because vertices have a dynamic format
                       vertex-count (/ (count uvs) 2)]
                   (if weighted?
                     (let [[positions
                            bone-indices
                            bone-weights] (loop [vertices vertices
                                                 positions []
                                                 bone-indices []
                                                 bone-weights []]
                                            (if-let [bone-count (first vertices)]
                                              (let [weights (take bone-count (map (fn [[bone-index x y weight]]
                                                                                    {:bone-index (bones-remap bone-index)
                                                                                     :x x
                                                                                     :y y
                                                                                     :weight weight})
                                                                                  (partition 4 (rest vertices))))
                                                    p ^Point3d (reduce (fn [^Point3d p w]
                                                                         (let [wp (Point3d. (:x w) (:y w) 0)
                                                                               world ^RigUtil$Transform (bone-index->world (:bone-index w))]
                                                                           (.apply world wp)
                                                                           (.scaleAdd wp ^double (:weight w) p)
                                                                           (.set p wp)
                                                                           p))
                                                                       (Point3d.) weights)
                                                    weights (normalize-weights (take 4 (sort-by #(- 1.0 (:weight %)) weights)))]
                                                (recur (drop (inc (* bone-count 4)) vertices)
                                                       (conj positions (.x p) (.y p) (.z p))
                                                       (into bone-indices (flatten (partition 4 4 (repeat 0) (mapv :bone-index weights))))
                                                       (into bone-weights (flatten (partition 4 4 (repeat 0) (mapv :weight weights))))))
                                              [positions bone-indices bone-weights]))]
                       {:positions positions
                        :texcoord0 (mapcat (fn [[u v]]
                                             (let [uv (Point2d. u v)]
                                               (.apply uv-trans uv)
                                               [(.x uv) (.y uv)]))
                                           (partition 2 uvs))
                        :position-indices (get attachment "triangles")
                        :weights bone-weights
                        :bone-indices bone-indices
                        :mesh-color (hex->color (get attachment "color" "ffffffff"))})
                     (let [weight-count (* vertex-count 4)]
                       {:positions (mapcat (fn [[x y]]
                                             (let [p (Point3d. x y 0)]
                                               (.apply world p)
                                               [(.x p) (.y p) (.z p)]))
                                           (partition 2 vertices))
                        :texcoord0 (mapcat (fn [[u v]]
                                             (let [uv (Point2d. u v)]
                                               (.apply uv-trans uv)
                                               [(.x uv) (.y uv)]))
                                           (partition 2 uvs))
                        :position-indices (get attachment "triangles")
                        :weights (take weight-count (cycle [1 0 0 0]))
                        :bone-indices (take weight-count (cycle [(:bone-index slot-data) 0 0 0]))
                        :mesh-color (hex->color (get attachment "color" "ffffffff"))})))
                 ; Ignore other types
                 nil)]
      (some-> mesh
              (assoc :slot-color (get slot-data :slot-color [1.0 1.0 1.0 1.0]))))))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- validate-scene-atlas [_node-id atlas]
  (prop-resource-error :fatal _node-id :atlas atlas "Atlas"))

(defn- validate-scene-spine-json [_node-id spine-json]
  (prop-resource-error :fatal _node-id :spine-json spine-json "Spine Json"))

(g/defnk produce-scene-own-build-errors [_node-id atlas spine-json]
  (g/package-errors _node-id
                    (validate-scene-atlas _node-id atlas)
                    (validate-scene-spine-json _node-id spine-json)))

(g/defnk produce-scene-build-targets
  [_node-id own-build-errors resource spine-scene-pb atlas dep-build-targets]
  (g/precluding-errors own-build-errors
    (rig/make-rig-scene-build-targets _node-id
                                      resource
                                      (assoc spine-scene-pb
                                        :texture-set atlas)
                                      dep-build-targets
                                      [:texture-set])))

(defn- read-bones
  [spine-scene]
  (mapv (fn [b]
          {:id (murmur/hash64 (get b "name"))
           :name (get b "name")
           :parent (when (contains? b "parent") (murmur/hash64 (get b "parent")))
           :position [(get b "x" 0) (get b "y" 0) 0]
           :rotation (angle->clj-quat (get b "rotation" 0))
           :scale [(get b "scaleX" 1) (get b "scaleY" 1) 1]
           :inherit-scale (get b "inheritScale" true)
           :length (get b "length")})
        (get spine-scene "bones")))

(defn- read-iks
  [spine-scene bone-id->index]
  (mapv (fn [ik]
          (let [bones (get ik "bones")
                [parent child] (if (= 1 (count bones))
                                 [(first bones) (first bones)]
                                 bones)]
            {:id (murmur/hash64 (get ik "name" "unamed"))
             :parent (some-> parent murmur/hash64 bone-id->index)
             :child (some-> child murmur/hash64 bone-id->index)
             :target (some-> (get ik "target") murmur/hash64 bone-id->index)
             :positive (get ik "bendPositive" true)
             :mix (get ik "mix" 1.0)}))
        (get spine-scene "ik")))

(defn- read-base-slots
  [skins-json slots-json bone-id->index bone-index->world-transform]
  (let [attachment-points-by-slot-name (transduce (comp (mapcat second)
                                                        (map (fn [[slot-name attachment-points]]
                                                               {slot-name (into (sorted-set)
                                                                                (map first)
                                                                                attachment-points)})))
                                                  (partial merge-with into)
                                                  {}
                                                  skins-json)]
    (into {}
          (map-indexed (fn [i slot]
                         (let [slot-name (get slot "name")
                               ^java.util.List attachment-names (into [] (get attachment-points-by-slot-name slot-name))
                               attachment-points (zipmap attachment-names (repeat (count attachment-names) -1))
                               bone-index (bone-id->index (murmur/hash64 (get slot "bone")))
                               default-attachment (get slot "attachment")]
                           [slot-name
                            {:id (murmur/hash64 slot-name)
                             :bone-index bone-index
                             :bone-world (get bone-index->world-transform bone-index)
                             :draw-order i
                             :slot-color (hex->color (get slot "color" "ffffffff"))
                             :attachment-points attachment-points
                             :attachment-names attachment-names
                             :default-attachment default-attachment
                             :active-attachment (index-of attachment-names default-attachment)}])))

          slots-json)))

(defn- attachment-entry->named-attachment [[attachment-name attachment slot-name]]
  (assoc attachment
    "name" (get attachment "name" attachment-name)
    "slot-name" slot-name))

(defn- read-distinct-meshes
  [skins]
  (vec (sort-by util/map->sort-key
                (into #{}
                      (comp (mapcat second)
                            (mapcat (fn [[slot-name attachments]]
                                      (map (fn [[attachment-point attachment]]
                                             [attachment-point attachment slot-name])
                                           attachments)))
                            (map attachment-entry->named-attachment))
                      skins))))

(defn- update-slot-attachment-indices
  [slot-attachments-x slot-name slot-attachments-j meshes]
  (reduce-kv (fn [attachment attachment-id attachment-index]
               (let [mesh-entry (attachment-entry->named-attachment [attachment-id (get slot-attachments-j attachment-id) slot-name])
                     mesh-index (index-of meshes mesh-entry)
                     mesh-index (if (not= mesh-index -1) mesh-index attachment-index)]
                   (assoc attachment attachment-id mesh-index)))
             {}
             slot-attachments-x))

(defn- update-skin-attachment-indices
  [skin-x skin-j meshes]
  (reduce-kv (fn [skin slot-name slot]
               (assoc skin
                 slot-name
                 (update slot :attachment-points update-slot-attachment-indices slot-name (get skin-j slot-name {}) meshes)))
             {}
             skin-x))

(defn- load-default-skin
  [skin-j base-slots meshes]
  (update-skin-attachment-indices base-slots skin-j meshes))

(defn- load-rest-skins
  [skins-j default-skin-x meshes]
  (map (fn [[skin-name skin-j]]
         [skin-name (update-skin-attachment-indices default-skin-x skin-j meshes)])
       skins-j))

(defn- bone-world-transforms
  [bones]
  (loop [bones bones
         wt []]
    (if-let [bone (first bones)]
      (let [local-t ^RigUtil$Transform (bone->transform bone)
            world-t (if (not= 0xffff (:parent bone))
                      (let [world-t (doto (RigUtil$Transform. (get wt (:parent bone)))
                                      (.mul local-t))]
                        ;; Reset scale when not inheriting
                        (when (not (:inherit-scale bone))
                          (doto (.scale world-t)
                            (.set (.scale local-t))))
                        world-t)
                      local-t)]
        (recur (rest bones) (conj wt world-t)))
      wt)))

(defn skins-json [spine-scene]
  ;; The file format changed in Spine 3.8. This returns the skins in a pre-3.8
  ;; format, which is a map of skin names to their attachments.
  (let [skins (get spine-scene "skins")]
    (if (map? skins)
      skins
      (into {}
            (map (fn [entry]
                   [(get entry "name")
                    (get entry "attachments")]))
            skins))))

(g/defnk produce-spine-scene-pb [_node-id spine-json spine-scene anim-data sample-rate]
  (let [spf (/ 1.0 sample-rate)
        ;; Bone data
        bones (read-bones spine-scene)
        indexed-bone-children (reduce (fn [m [i b]] (update-in m [(:parent b)] conj [i b])) {} (map-indexed (fn [i b] [i b]) bones))
        root (first (get indexed-bone-children nil))
        ordered-bones (if root
                        (tree-seq (constantly true) (fn [[i b]] (get indexed-bone-children (:id b))) root)
                        [])
        bones-remap (into {} (map-indexed (fn [i [first-i b]] [first-i i]) ordered-bones))
        bones (mapv second ordered-bones)
        bone-id->index (into {} (map-indexed (fn [i b] [(:id b) i]) bones))
        bones (mapv #(assoc % :parent (get bone-id->index (:parent %) 0xffff)) bones)
        bone-index->world-transform (bone-world-transforms bones)

        ;; IK data
        iks (read-iks spine-scene bone-id->index)
        ik-id->index (zipmap (map :id iks) (range))

        ;; Slot data
        skins-json (skins-json spine-scene)
        slots-json (get spine-scene "slots")
        base-slots (read-base-slots skins-json slots-json bone-id->index bone-index->world-transform)
        slot-count (count slots-json)

        ;; Skin and mesh data
        meshes (read-distinct-meshes skins-json)
        default-skin-x (load-default-skin (get skins-json "default") base-slots meshes)
        skins-x (load-rest-skins (filter (fn [[skin _]] (not= "default" skin)) skins-json) default-skin-x meshes)
        all-skins (conj skins-x ["" default-skin-x])

        ;; Protobuf
        pb (try
             {:skeleton {:bones bones
                         :iks   iks}
              :animation-set (let [event-name->event-props (get spine-scene "events" {})
                                   animations (mapv (fn [[name a]]
                                                      (let [duration (anim-duration a)]
                                                        {:id (murmur/hash64 name)
                                                         :sample-rate sample-rate
                                                         :duration duration
                                                         :tracks (build-tracks (get a "bones") duration sample-rate spf bone-id->index)
                                                         :mesh-tracks (build-mesh-tracks (get a "slots") (get a "drawOrder") duration sample-rate spf base-slots)
                                                         :event-tracks (build-event-tracks (get a "events") event-name->event-props)
                                                         :ik-tracks (build-ik-tracks (get a "ik") duration sample-rate spf ik-id->index)}))
                                                    (get spine-scene "animations"))]
                               {:animations animations})
              :mesh-set {:slot-count slot-count
                         :mesh-attachments (mapv (fn [mesh]
                                                   (attachment->mesh mesh base-slots anim-data bones-remap bone-index->world-transform))
                                                 meshes)
                         :mesh-entries (mapv (fn [[skin-name slots]]
                                               {:id (murmur/hash64 skin-name)
                                                :mesh-slots (mapv (fn [slot]
                                                                    (let [slot-data (val slot)
                                                                          attachment-points (get slot-data :attachment-points)]
                                                                      {:id (:id slot-data)
                                                                       :mesh-attachments (mapv (fn [attachment-name] (get attachment-points attachment-name))
                                                                                               (:attachment-names slot-data))
                                                                       :active-index (get slot-data :active-attachment -1)
                                                                       :slot-color (:slot-color slot-data)}))
                                                                  (sort-by (fn [[_ slot-data]]
                                                                             (:draw-order slot-data))
                                                                           slots))})
                                             all-skins)}}
             (catch Exception e
               (log/error :exception e)
               (g/->error _node-id :spine-json :fatal spine-json (str "Incompatible data found in spine json " (resource/resource->proj-path spine-json)))))]
    pb))

(defn- transform-positions [^Matrix4d transform mesh]
  (let [p (Point3d.)]
    (update mesh :positions (fn [positions]
                              (->> positions
                                (partition 3)
                                (mapcat (fn [[x y z]]
                                          (.set p x y z)
                                          (.transform transform p)
                                          [(.x p) (.y p) (.z p)])))))))

(defn- renderable->meshes [renderable]
  (let [meshes (get-in renderable [:user-data :spine-scene-pb :mesh-set :mesh-attachments])
        skins (get-in renderable [:user-data :spine-scene-pb :mesh-set :mesh-entries])
        skin-id (some-> renderable :user-data :skin murmur/hash64)
        skin (or (first (filter #(= (:id %) skin-id) skins))
                 (first skins))]
    (into []
          (comp (keep #(get meshes (get (get % :mesh-attachments) (get % :active-index) -1)))
                (map (partial transform-positions (:world-transform renderable)))
                (map (fn [mesh]
                       (let [tint-color (get-in renderable [:user-data :color] [1.0 1.0 1.0 1.0])
                             skin-color (:slot-color mesh)
                             mesh-color (:mesh-color mesh)
                             final-color (mapv * skin-color tint-color mesh-color)
                             alpha (final-color 3)
                             final-color (assoc (mapv (partial * alpha) final-color) 3 alpha)]
                         (assoc mesh :color final-color)))))
          (:mesh-slots skin))))

(defn- mesh->verts [mesh]
  (let [verts (mapv concat (partition 3 (:positions mesh)) (partition 2 (:texcoord0 mesh)) (repeat (:color mesh)))]
    (map (partial get verts) (:position-indices mesh))))

(defn gen-vb [renderables]
  (prn "MAWE" "PLUGIN" "gen-vb")
  (let [meshes (mapcat renderable->meshes renderables)
        vcount (reduce + 0 (map (comp count :position-indices) meshes))]
    (when (> vcount 0)
      (let [vb (render/->vtx-pos-tex-col vcount)
            verts (mapcat mesh->verts meshes)]
        (persistent! (reduce conj! vb verts))))))

(def color [1.0 1.0 1.0 1.0])

(defn- skeleton-vs [parent-pos bone vs ^Matrix4d wt]
  (let [t (doto (Matrix4d.)
            (.mul wt ^Matrix4d (:transform bone)))
        pos (math/vecmath->clj (math/translation t))
        vs (if parent-pos
             (conj vs (into parent-pos color) (into pos color))
             vs)]
    (reduce (fn [vs bone] (skeleton-vs pos bone vs wt)) vs (:children bone))))

(defn- gen-skeleton-vb [renderables]
  (let [vs (loop [renderables renderables
                  vs []]
             (if-let [r (first renderables)]
               (let [skeleton (get-in r [:user-data :scene-structure :skeleton])]
                 (recur (rest renderables) (skeleton-vs nil skeleton vs (:world-transform r))))
               vs))
        vcount (count vs)]
    (when (> vcount 0)
      (let [vb (render/->vtx-pos-col vcount)]
        (persistent! (reduce conj! vb vs))))))

(shader/defshader spine-id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader spine-id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D texture_sampler var_texcoord0.xy))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def spine-id-shader (shader/make-shader ::id-shader spine-id-vertex-shader spine-id-fragment-shader {"id" :id}))

(defn- render-spine-scenes [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/transparent
      (when-let [vb (gen-vb renderables)]
        (let [user-data (:user-data (first renderables))
              blend-mode (:blend-mode user-data)
              gpu-texture (:gpu-texture user-data)
              shader (get user-data :shader render/shader-tex-tint)
              vertex-binding (vtx/use-with ::spine-trans vb shader)]
          (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
            (gl/set-blend-mode gl blend-mode)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA))))

      pass/selection
      (when-let [vb (gen-vb renderables)]
        (let [gpu-texture (:gpu-texture (:user-data (first renderables)))
              vertex-binding (vtx/use-with ::spine-selection vb spine-id-shader)]
          (gl/with-gl-bindings gl (assoc render-args :id (scene-picking/renderable-picking-id-uniform (first renderables))) [gpu-texture spine-id-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))))))

(defn- render-spine-skeletons [^GL2 gl render-args renderables rcount]
  (assert (= (:pass render-args) pass/transparent))
  (when-let [vb (gen-skeleton-vb renderables)]
    (let [vertex-binding (vtx/use-with ::spine-skeleton vb render/shader-outline)]
      (gl/with-gl-bindings gl render-args [render/shader-outline vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

(defn- render-spine-outlines [^GL2 gl render-args renderables rcount]
  (assert (= (:pass render-args) pass/outline))
  (render/render-aabb-outline gl render-args ::spine-outline renderables rcount))

(g/defnk produce-main-scene [_node-id aabb gpu-texture default-tex-params spine-scene-pb scene-structure]
  (when (and gpu-texture scene-structure)
    (let [blend-mode :blend-mode-alpha]
      (assoc {:node-id _node-id :aabb aabb}
             :renderable {:render-fn render-spine-scenes
                          :tags #{:spine}
                          :batch-key gpu-texture
                          :select-batch-key _node-id
                          :user-data {:spine-scene-pb spine-scene-pb
                                      :scene-structure scene-structure
                                      :gpu-texture gpu-texture
                                      :tex-params default-tex-params
                                      :blend-mode blend-mode}
                          :passes [pass/transparent pass/selection]}))))

(defn- make-spine-outline-scene [_node-id aabb]
  {:aabb aabb
   :node-id _node-id
   :renderable {:render-fn render-spine-outlines
                :tags #{:spine :outline}
                :batch-key ::outline
                :passes [pass/outline]}})

(defn- make-spine-skeleton-scene [_node-id aabb gpu-texture scene-structure]
  (let [scene {:node-id _node-id :aabb aabb}]
    (if (and gpu-texture scene-structure)
      (let [blend-mode :blend-mode-alpha]
        {:aabb aabb
         :node-id _node-id
         :renderable {:render-fn render-spine-skeletons
                      :tags #{:spine :skeleton :outline}
                      :batch-key gpu-texture
                      :user-data {:scene-structure scene-structure}
                      :passes [pass/transparent]}})
      scene)))

(g/defnk produce-scene [_node-id aabb main-scene gpu-texture scene-structure]
  (if (some? main-scene)
    (assoc main-scene :children [(make-spine-skeleton-scene _node-id aabb gpu-texture scene-structure)
                                 (make-spine-outline-scene _node-id aabb)])
    {:node-id _node-id :aabb geom/empty-bounding-box}))

(defn- mesh->aabb [aabb mesh]
  (let [positions (partition 3 (:positions mesh))]
    (reduce (fn [aabb pos] (apply geom/aabb-incorporate aabb pos)) aabb positions)))

(g/defnk produce-skin-aabbs [scene-structure spine-scene-pb]
  (let [skin-names (:skins scene-structure)
        skins (get-in spine-scene-pb [:mesh-set :mesh-entries])
        meshes (get-in spine-scene-pb [:mesh-set :mesh-attachments])]
    (into {}
          (map (fn [skin-name]
                 (let [skin-id (murmur/hash64 skin-name)
                       skin (or (some (fn [skin] (when (= (:id skin) skin-id) skin)) skins)
                                (first skins))] ; this is a bit ugly but matches what we do in renderable->meshes while rendering the scene
                   (if-not (some? skin)
                     [skin-name geom/empty-bounding-box]
                     (let [mesh-slots (:mesh-slots skin)
                           skin-meshes (into []
                                             (keep (fn [{:keys [mesh-attachments active-index] :as _mesh-slot}]
                                                     (get meshes (get mesh-attachments active-index -1))))
                                             mesh-slots)
                           skin-meshes-aabb (reduce mesh->aabb geom/empty-bounding-box skin-meshes)]
                       [skin-name skin-meshes-aabb])))))
          skin-names)))

(g/defnode SpineSceneNode
  (inherits resource-node/ResourceNode)

  (property spine-json resource/Resource
            (value (gu/passthrough spine-json-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :spine-json-resource]
                                            [:content :spine-scene]
                                            [:consumer-passthrough :scene-structure]
                                            [:node-outline :source-outline])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "json"}))
            (dynamic error (g/fnk [_node-id spine-json]
                             (validate-scene-spine-json _node-id spine-json))))

  (property atlas resource/Resource
            (value (gu/passthrough atlas-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :atlas-resource]
                                            [:anim-data :anim-data]
                                            [:gpu-texture :gpu-texture]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext "atlas"}))
            (dynamic error (g/fnk [_node-id atlas]
                             (validate-scene-atlas _node-id atlas))))

  (property sample-rate g/Num)

  (input spine-json-resource resource/Resource)
  (input atlas-resource resource/Resource)

  (input anim-data g/Any)
  (input default-tex-params g/Any)
  (input gpu-texture g/Any)
  (input dep-build-targets g/Any :array)
  (input spine-scene g/Any)
  (input scene-structure g/Any)

  (output save-value g/Any produce-save-value)
  (output own-build-errors g/Any produce-scene-own-build-errors)
  (output build-targets g/Any :cached produce-scene-build-targets)
  (output spine-scene-pb g/Any :cached produce-spine-scene-pb)
  (output main-scene g/Any :cached produce-main-scene)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB :cached (g/fnk [spine-scene-pb] (reduce mesh->aabb geom/null-aabb (get-in spine-scene-pb [:mesh-set :mesh-attachments]))))
  (output skin-aabbs g/Any :cached produce-skin-aabbs)
  (output anim-data g/Any (gu/passthrough anim-data))
  (output scene-structure g/Any (gu/passthrough scene-structure))
  (output spine-anim-ids g/Any (g/fnk [scene-structure] (:animations scene-structure))))

(defn load-spine-scene [project self resource spine]
  (let [spine-resource (workspace/resolve-resource resource (:spine-json spine))
        atlas          (workspace/resolve-resource resource (:atlas spine))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (g/set-property self
                      :spine-json spine-resource
                      :atlas atlas
                      :sample-rate (:sample-rate spine)))))

(g/defnk produce-model-pb [spine-scene-resource default-animation skin material-resource blend-mode]
  {:spine-scene (resource/resource->proj-path spine-scene-resource)
   :default-animation default-animation
   :skin skin
   :material (resource/resource->proj-path material-resource)
   :blend-mode blend-mode})

(defn ->skin-choicebox [spine-skins]
  (properties/->choicebox (cons "" (remove (partial = "default") spine-skins))))

(defn validate-skin [node-id prop-kw spine-skins spine-skin]
  (when-not (empty? spine-skin)
    (validation/prop-error :fatal node-id prop-kw
                           (fn [skin skins]
                             (when-not (contains? skins skin)
                               (format "skin '%s' could not be found in the specified spine scene" skin)))
                           spine-skin
                           (disj (set spine-skins) "default"))))

(defn- validate-model-default-animation [node-id spine-scene spine-anim-ids default-animation]
  (when (and spine-scene (not-empty default-animation))
    (validation/prop-error :fatal node-id :default-animation
                           (fn [anim ids]
                             (when-not (contains? ids anim)
                               (format "animation '%s' could not be found in the specified spine scene" anim)))
                           default-animation
                           (set spine-anim-ids))))

(defn- validate-model-material [node-id material]
  (prop-resource-error :fatal node-id :material material "Material"))

(defn- validate-model-skin [node-id spine-scene scene-structure skin]
  (when spine-scene
    (validate-skin node-id :skin (:skins scene-structure) skin)))

(defn- validate-model-spine-scene [node-id spine-scene]
  (prop-resource-error :fatal node-id :spine-scene spine-scene "Spine Scene"))

(g/defnk produce-model-own-build-errors [_node-id default-animation material spine-anim-ids spine-scene scene-structure skin]
  (g/package-errors _node-id
                    (validate-model-material _node-id material)
                    (validate-model-spine-scene _node-id spine-scene)
                    (validate-model-skin _node-id spine-scene scene-structure skin)
                    (validate-model-default-animation _node-id spine-scene spine-anim-ids default-animation)))

(defn- build-spine-model [resource dep-resources user-data]
  (prn "SPINE EXTENSION PLUGIN!" "build-spine-model")
  (let [pb (:proto-msg user-data)
        pb (reduce #(assoc %1 (first %2) (second %2)) pb (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes (dynamically-load-class! dcl "com.dynamo.spine.proto.Spine$SpineModelDesc") pb)}))

(g/defnk produce-model-build-targets [_node-id own-build-errors resource model-pb spine-scene-resource material-resource dep-build-targets]
  (prn "SPINE EXTENSION PLUGIN!" "produce-model-build-targets")
  (g/precluding-errors own-build-errors
    (let [dep-build-targets (flatten dep-build-targets)
          deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
          dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:spine-scene spine-scene-resource] [:material material-resource]])
          model-pb (update model-pb :skin (fn [skin] (or skin "")))]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-spine-model
          :user-data {:proto-msg model-pb
                      :dep-resources dep-resources}
          :deps dep-build-targets})])))

(g/defnode SpineModelNode
  (inherits resource-node/ResourceNode)

  (property spine-scene resource/Resource
            (value (gu/passthrough spine-scene-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :spine-scene-resource]
                                            [:main-scene :spine-main-scene]
                                            [:skin-aabbs :spine-scene-skin-aabbs]
                                            [:spine-anim-ids :spine-anim-ids]
                                            [:build-targets :dep-build-targets]
                                            [:anim-data :anim-data]
                                            [:scene-structure :scene-structure])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext spine-scene-ext}))
            (dynamic error (g/fnk [_node-id spine-scene]
                             (validate-model-spine-scene _node-id spine-scene))))
  (property blend-mode g/Any (default :blend-mode-alpha)
            (dynamic tip (validation/blend-mode-tip blend-mode (dynamically-load-class! dcl "com.dynamo.spine.proto.Spine$SpineModelDesc$BlendMode")))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox (dynamically-load-class! dcl "com.dynamo.spine.proto.Spine$SpineModelDesc$BlendMode")))))
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
            (dynamic error (g/fnk [_node-id spine-anim-ids default-animation spine-scene]
                             (validate-model-default-animation _node-id spine-scene spine-anim-ids default-animation)))
            (dynamic edit-type (g/fnk [spine-anim-ids] (properties/->choicebox spine-anim-ids))))
  (property skin g/Str
            (dynamic error (g/fnk [_node-id skin scene-structure spine-scene]
                             (validate-model-skin _node-id spine-scene scene-structure skin)))
            (dynamic edit-type (g/fnk [scene-structure] (->skin-choicebox (:skins scene-structure)))))

  (input dep-build-targets g/Any :array)
  (input spine-scene-resource resource/Resource)
  (input spine-main-scene g/Any)
  (input spine-scene-skin-aabbs g/Any)
  (input scene-structure g/Any)
  (input spine-anim-ids g/Any)
  (input material-resource resource/Resource)
  (input material-shader ShaderLifecycle)
  (input material-samplers g/Any)
  (input default-tex-params g/Any)
  (input anim-data g/Any)

  (output tex-params g/Any (g/fnk [material-samplers default-tex-params]
                             (or (some-> material-samplers first material/sampler->tex-params)
                                 default-tex-params)))
  (output anim-ids g/Any :cached (g/fnk [anim-data] (vec (sort (keys anim-data)))))
  (output material-shader ShaderLifecycle (gu/passthrough material-shader))
  (output scene g/Any :cached (g/fnk [_node-id spine-main-scene spine-scene-skin-aabbs material-shader tex-params skin]
                                (if (and (some? material-shader) (some? (:renderable spine-main-scene)))
                                  (let [aabb (get spine-scene-skin-aabbs (if (= "" skin) "default" skin) geom/empty-bounding-box)
                                        spine-scene-node-id (:node-id spine-main-scene)]
                                    (-> spine-main-scene
                                        (assoc-in [:renderable :user-data :shader] material-shader)
                                        (update-in [:renderable :user-data :gpu-texture] texture/set-params tex-params)
                                        (assoc-in [:renderable :user-data :skin] skin)
                                        (assoc :aabb aabb)
                                        (assoc :children [(make-spine-outline-scene spine-scene-node-id aabb)])))
                                  (merge {:node-id _node-id
                                          :renderable {:passes [pass/selection]}
                                          :aabb geom/empty-bounding-box}
                                         spine-main-scene))))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id own-build-errors spine-scene]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key "Spine Model"
                                                              :label "Spine Model"
                                                              :icon spine-model-icon
                                                              :outline-error? (g/error-fatal? own-build-errors)}

                                                             (resource/openable-resource? spine-scene)
                                                             (assoc :link spine-scene :outline-reference? false))))
  (output model-pb g/Any produce-model-pb)
  (output save-value g/Any (gu/passthrough model-pb))
  (output own-build-errors g/Any produce-model-own-build-errors)
  (output build-targets g/Any :cached produce-model-build-targets))


(defn load-spine-model [project self resource spine]
  (let [resolve-fn (partial workspace/resolve-resource resource)
        spine (-> spine
                (update :spine-scene resolve-fn)
                (update :material resolve-fn))]
    (concat
      (g/connect project :default-tex-params self :default-tex-params)
      (for [[k v] spine]
        (g/set-property self k v)))))

(defn register-resource-types [workspace]
  (prn "SPINE register resource types")
  (concat
    (resource-node/register-ddf-resource-type workspace
      :ext spine-scene-ext
      :build-ext "rigscenec"
      :label "Spine Scene plugin"
      :node-type SpineSceneNode
      :ddf-type (dynamically-load-class! dcl "com.dynamo.spine.proto.Spine$SpineSceneDesc")
      :load-fn load-spine-scene
      :icon spine-scene-icon
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}})
    (resource-node/register-ddf-resource-type workspace
      :ext spine-model-ext
      :label "Spine Model plugin!"
      :node-type SpineModelNode
      :ddf-type (dynamically-load-class! dcl "com.dynamo.spine.proto.Spine$SpineModelDesc")
      :load-fn load-spine-model
      :icon spine-model-icon
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{:position :rotation}}})))

(g/defnk produce-transform [position rotation scale]
  (math/->mat4-non-uniform (Vector3d. (double-array position))
                           (math/euler-z->quat rotation)
                           (Vector3d. (double-array scale))))

(g/defnode SpineBone
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

  (input child-bones g/Any :array)

  (output transform Matrix4d :cached produce-transform)
  (output bone g/Any (g/fnk [name transform child-bones]
                            {:name name
                             :local-transform transform
                             :children child-bones}))
  (output node-outline outline/OutlineData (g/fnk [_node-id name child-outlines]
                                                  {:node-id _node-id
                                                   :node-outline-key name
                                                   :label name
                                                   :icon spine-bone-icon
                                                   :children child-outlines
                                                   :read-only true})))

(defn- update-transforms [^Matrix4d parent bone]
  (let [t ^Matrix4d (:local-transform bone)
        t (doto (Matrix4d.)
            (.mul parent t))]
    (-> bone
      (assoc :transform t)
      (assoc :children (mapv #(update-transforms t %) (:children bone))))))

(g/defnode SpineSceneJson
  (inherits outline/OutlineNode)
  (input source-outline outline/OutlineData)
  (output node-outline outline/OutlineData (g/fnk [source-outline] source-outline))
  (input skeleton g/Any)
  (input content g/Any)
  (output structure g/Any :cached (g/fnk [skeleton content]
                                         {:skeleton (update-transforms (math/->mat4) skeleton)
                                          :skins (vec (sort (keys (skins-json content))))
                                          :animations (keys (get content "animations"))})))

(defn accept-spine-scene-json [content]
  (when (or (get-in content ["skeleton" "spine"])
            (and (get content "bones") (get content "animations")))
    content))

(defn accept-resource-json
  [resource]
  ;; This function tries to find the JSON elements that mark a spine scene
  ;; without doing a full parse. This is a performance improvement for startup
  ;; of large projects. A full parse is done if this function succeeds so there
  ;; is no need to check if the content is valid JSON at this point.
  (with-open [rdr (io/reader resource)]
    (let [sb (StringBuilder.)]
      (loop [done? false
             inside-string? false
             escape? false
             strings (HashSet.)
             c (.read rdr)]
        (if done?
          true
          (if (= -1 c)
            false
            (if escape?
              (do
                (.append sb (char c))
                (recur done? inside-string? false strings (.read rdr)))
              (case c
                92 (recur done? inside-string? true strings (.read rdr)) ;; \
                34 (if inside-string? ;; "
                     (do
                       (.add strings (.toString sb))
                       (.setLength sb 0)
                       (recur (or (and (.contains strings "skeleton") (.contains strings "spine"))
                                  (and (.contains strings "bones") (.contains strings "animations")))
                              false escape? strings (.read rdr)))
                     (recur done? true escape? strings (.read rdr)))
                (do
                  (when inside-string?
                    (.append sb (char c)))
                  (recur done? inside-string? escape? strings (.read rdr)))))))))))

(defn- tx-first-created [tx-data]
  (get-in (first tx-data) [:node :_node-id]))

(defn load-spine-scene-json [node-id content]
  (let [bones (get content "bones")
        graph (g/node-id->graph-id node-id)
        scene-tx-data (g/make-nodes graph [scene SpineSceneJson]
                                    (g/connect scene :_node-id node-id :nodes)
                                    (g/connect scene :node-outline node-id :child-outlines)
                                    (g/connect scene :structure node-id :consumer-passthrough)
                                    (g/connect node-id :content scene :content))
        scene-id (tx-first-created scene-tx-data)]
    (concat
      scene-tx-data
      (loop [bones bones
             tx-data []
             bone-ids {}]
        (if-let [bone (first bones)]
          (let [name (get bone "name")
                parent (get bone "parent")
                x (get bone "x" 0)
                y (get bone "y" 0)
                rotation (get bone "rotation" 0)
                scale-x (get bone "scaleX" 1.0)
                scale-y (get bone "scaleY" 1.0)
                length (get bone "length")
                bone-tx-data (g/make-nodes graph [bone [SpineBone :name name :position [x y 0] :rotation rotation :scale [scale-x scale-y 1.0] :length length]]
                                           (g/connect bone :_node-id node-id :nodes)
                                           (if-let [parent (get bone-ids parent)]
                                             (concat
                                               (g/connect bone :node-outline parent :child-outlines)
                                               (g/connect bone :bone parent :child-bones))
                                             (concat
                                               (g/connect bone :node-outline scene-id :source-outline)
                                               (g/connect bone :bone scene-id :skeleton))))
                bone-id (tx-first-created bone-tx-data)]
            (recur (rest bones) (conj tx-data bone-tx-data) (assoc bone-ids name bone-id)))
          tx-data)))))

(json/register-json-loader ::spine-scene accept-spine-scene-json accept-resource-json load-spine-scene-json)

; The plugin
(defn load-plugin-spine [workspace]
  (prn "MAWE" "workspace" workspace)
  (g/transact (concat (register-resource-types workspace)))
  )

(defn return-plugin []
  (fn [x] (load-plugin-spine x)))
(return-plugin)


