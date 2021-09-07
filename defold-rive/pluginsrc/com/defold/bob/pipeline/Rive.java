//
// License: MIT
//

package com.dynamo.bob.pipeline;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.List;
import java.util.Arrays;

import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;
import com.sun.jna.Structure;

public class Rive {

    static {
        try {
            Native.register("RiveExt");
        } catch (Exception e) {
            System.out.println("FATAL: " + e.getMessage());
        }
    }

    public static class RivePointer extends PointerType {
        public RivePointer() { super(); }
        public RivePointer(Pointer p) { super(p); }
        @Override
        public void finalize() {
            RIVE_Destroy(this);
        }
    }

    // Type Mapping in JNA
    // https://java-native-access.github.io/jna/3.5.1/javadoc/overview-summary.html#marshalling

    public static native Pointer RIVE_LoadFromPath(String path);
    public static native Pointer RIVE_LoadFromBuffer(Buffer buffer, int bufferSize, String path);
    public static native void RIVE_Destroy(RivePointer rive);
    public static native int RIVE_GetNumAnimations(RivePointer rive);
    public static native String RIVE_GetAnimation(RivePointer rive, int index);

    // TODO: Create a jna Structure for this
    // Structures in JNA
    // https://java-native-access.github.io/jna/3.5.1/javadoc/overview-summary.html#structures

    ////////////////////////////////////////////////////////////////////////////////
    static public class AABB extends Structure {
        public float minX, minY, maxX, maxY;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"minX", "minY", "maxX", "maxY"});
        }
        public AABB() {
            this.minX = this.minY = this.maxX = this.maxY = 0;
        }
    }

    public static native void RIVE_GetAABBInternal(RivePointer rive, AABB aabb);

    public static AABB RIVE_GetAABB(RivePointer rive) {
        AABB aabb = new AABB();
        RIVE_GetAABBInternal(rive, aabb);
        return aabb;
    }

    ////////////////////////////////////////////////////////////////////////////////

    static public class BoneInteral extends Structure {
        public String name;
        public int parent;
        public float posX, posY, rotation, scaleX, scaleY, length;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name", "parent", "posX", "posY", "rotation", "scaleX", "scaleY", "length"});
        }
        public BoneInteral() {
            this.name = "<empty>";
            this.parent = -1;
            this.posX = this.posY = 0.0f;
            this.scaleX = this.scaleY = 1.0f;
            this.rotation = 0.0f;
            this.length = 100.0f;
        }
    }

    static public class Bone {
        public String name;
        public int   index;
        public int   parent;
        public int[] children;
        public float posX, posY, rotation, scaleX, scaleY, length;
    }

    public static native int RIVE_GetNumBones(RivePointer rive);
    public static native String RIVE_GetBoneInternal(RivePointer rive, int index, BoneInteral bone);
    public static native int RIVE_GetNumChildBones(RivePointer rive, int bone);
    public static native int RIVE_GetChildBone(RivePointer rive, int bone, int index);

    protected static Bone RIVE_GetBone(RivePointer rive, int index) {
        Bone bone = new Bone();
        BoneInteral internal = new BoneInteral();
        RIVE_GetBoneInternal(rive, index, internal);
        bone.index = index;
        bone.name = internal.name;
        bone.posX = internal.posX;
        bone.posY = internal.posY;
        bone.scaleX = internal.scaleX;
        bone.scaleY = internal.scaleY;
        bone.rotation = internal.rotation;
        bone.length = internal.length;
        bone.parent = internal.parent;

        int num_children = RIVE_GetNumChildBones(rive, index);
        bone.children = new int[num_children];

        for (int i = 0; i < num_children; ++i) {
            bone.children[i] = RIVE_GetChildBone(rive, index, i);
        }
        return bone;
    }

    public static Bone[] RIVE_GetBones(RivePointer rive) {
        int num_bones = RIVE_GetNumBones(rive);
        Bone[] bones = new Bone[num_bones];
        for (int i = 0; i < num_bones; ++i){
            bones[i] = RIVE_GetBone(rive, i);
        }
        return bones;
    }


    ////////////////////////////////////////////////////////////////////////////////

    static public class StateMachineInput extends Structure {
        public String name;
        public String type;

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"name", "type"});
        }
    }

    static public class StateMachine {
        public String name;
        public int index;
        public StateMachineInput[] inputs;
    }

    public static native int RIVE_GetNumStateMachines(RivePointer rive);
    public static native String RIVE_GetStateMachineName(RivePointer rive, int index);
    public static native int RIVE_GetNumStateMachineInputs(RivePointer rive, int index);
    public static native int RIVE_GetStateMachineInput(RivePointer rive, int index, int input_index, StateMachineInput out);

    protected static StateMachine RIVE_GetStateMachine(RivePointer rive, int index) {
        StateMachine sm = new StateMachine();
        sm.index = index;
        sm.name = RIVE_GetStateMachineName(rive, index);

        int num_inputs = RIVE_GetNumStateMachineInputs(rive, index);
        sm.inputs = new StateMachineInput[num_inputs];

        for (int i = 0; i < num_inputs; ++i) {
            StateMachineInput input = new StateMachineInput();
            RIVE_GetStateMachineInput(rive, index, i, input);
            sm.inputs[i] = input;
        }
        return sm;
    }

    public static StateMachine[] RIVE_GetStateMachines(RivePointer rive) {
        int count = RIVE_GetNumStateMachines(rive);
        StateMachine[] arr = new StateMachine[count];
        for (int i = 0; i < count; ++i){
            arr[i] = RIVE_GetStateMachine(rive, i);
        }
        return arr;
    }

    ////////////////////////////////////////////////////////////////////////////////

    public static native void RIVE_UpdateVertices(RivePointer rive, float dt);
    public static native int RIVE_GetVertexSize(); // size in bytes per vertex
    public static native int RIVE_GetVertexCount(RivePointer rive);
    public static native void RIVE_GetVertices(RivePointer rive, Buffer buffer, int bufferSize); // buffer size must be at least VertexSize * VertexCount bytes long

    public static RivePointer RIVE_LoadFileFromBuffer(byte[] buffer, String path) {
        Buffer b = ByteBuffer.wrap(buffer);
        Pointer p = RIVE_LoadFromBuffer(b, b.capacity(), path);
        return new RivePointer(p);
    }

    public static void RIVE_GetVertices(RivePointer rive, float[] buffer){
        Buffer b = FloatBuffer.wrap(buffer);
        RIVE_GetVertices(rive, b, b.capacity()*4);
    }

    private static void Usage() {
        System.out.printf("Usage: pluginRiveExt.jar <file>\n");
        System.out.printf("\n");
    }

    private static void DebugPrintBone(Bone bone, Bone[] bones, int indent) {
        String tab = " ".repeat(indent * 4);
        System.out.printf("Bone:%s %s: idx: %d parent = %d, pos: %f, %f  scale: %f, %f  rot: %f  length: %f\n",
                tab, bone.name, bone.index, bone.parent, bone.posX, bone.posY, bone.scaleX, bone.scaleY, bone.rotation, bone.length);

        int num_children = bone.children.length;
        for (int i = 0; i < num_children; ++i){
            int child_index = bone.children[i];
            Bone child = bones[child_index];
            DebugPrintBone(child, bones, indent+1);
        }
    }

    private static void DebugPrintBones(Bone[] bones) {
        for (Bone bone : bones) {
            if (bone.parent == -1) {
                DebugPrintBone(bone, bones, 0);
            }
        }
    }

    // Used for testing functions
    // See ./utils/test_plugin.sh
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        String path = args[0];
        Pointer rive_file = RIVE_LoadFromPath(path);

        if (rive_file != null) {
            System.out.printf("Loaded %s\n", path);
        } else {
            System.err.printf("Failed to load %s\n", path);
            return;
        }

        RivePointer p = new RivePointer(rive_file);

        for (int i = 0; i < RIVE_GetNumAnimations(p); ++i) {
            String name = RIVE_GetAnimation(p, i);
            System.out.printf("Animation %d: %s\n", i, name);
        }

        Bone[] bones = RIVE_GetBones(p);
        DebugPrintBones(bones);

        StateMachine[] sms = RIVE_GetStateMachines(p);
        for (StateMachine sm : sms) {
            System.out.printf("StateMachine '%s'\n", sm.name);
            for (StateMachineInput input : sm.inputs) {
                System.out.printf("  input: name: %s  type: %s\n", input.name, input.type);
            }
        }


        RIVE_UpdateVertices(p, 0.0f);
    }
}