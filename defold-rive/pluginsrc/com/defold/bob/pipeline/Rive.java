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
import com.sun.jna.ptr.IntByReference;

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


    ////////////////////////////////////////////////////////////////////////////////
    // Render data

    // Matching the struct in vertices.h
    static public class RiveVertex extends Structure {
        public float x, y;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"x", "y"});
        }
    }

    // The enums come straight from https://github.com/defold/defold/blob/dev/engine/graphics/src/dmsdk/graphics/graphics.h
    public enum CompareFunc
    {
        COMPARE_FUNC_NEVER    (0),
        COMPARE_FUNC_LESS     (1),
        COMPARE_FUNC_LEQUAL   (2),
        COMPARE_FUNC_GREATER  (3),
        COMPARE_FUNC_GEQUAL   (4),
        COMPARE_FUNC_EQUAL    (5),
        COMPARE_FUNC_NOTEQUAL (6),
        COMPARE_FUNC_ALWAYS   (7);

        private final int value;
        private CompareFunc(int v) { this.value = v; }
        public int getValue() { return this.value; }
    };

    public enum FaceWinding
    {
        FACE_WINDING_CCW (0),
        FACE_WINDING_CW  (1);

        private final int value;
        private FaceWinding(int v) { this.value = v; }
        public int getValue() { return this.value; }
    };

    public enum StencilOp
    {
        STENCIL_OP_KEEP      (0),
        STENCIL_OP_ZERO      (1),
        STENCIL_OP_REPLACE   (2),
        STENCIL_OP_INCR      (3),
        STENCIL_OP_INCR_WRAP (4),
        STENCIL_OP_DECR      (5),
        STENCIL_OP_DECR_WRAP (6),
        STENCIL_OP_INVERT    (7);

        private final int value;
        private StencilOp(int v) { this.value = v; }
        public int getValue() { return this.value; }
    };

    static public class StencilTestFunc extends Structure {
        public int m_Func;      // dmGraphics::CompareFunc
        public int m_OpSFail;   // dmGraphics::StencilOp
        public int m_OpDPFail;  // dmGraphics::StencilOp
        public int m_OpDPPass;  // dmGraphics::StencilOp
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"m_Func", "m_OpSFail", "m_OpDPFail", "m_OpDPPass"});
        }
    }

    static public class StencilTestParams extends Structure {
        public StencilTestFunc m_Front;
        public StencilTestFunc m_Back;
        public byte    m_Ref;
        public byte    m_RefMask;
        public byte    m_BufferMask;
        public byte    m_ColorBufferMask; // bool
        public byte    m_ClearBuffer;      // bool
        public byte    m_SeparateFaceStates; // bool
        public byte[]  pad = new byte[32 - 6];
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "m_Front", "m_Back",
                "m_Ref", "m_RefMask", "m_BufferMask",
                "m_ColorBufferMask", "m_ClearBuffer", "m_SeparateFaceStates", "pad"});
        }
    }

    static public class Matrix4 extends Structure {
        public float[] m = new float[16];
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"m"});
        }
    }

    static public class Vector4 extends Structure {
        public float x, y, z, w;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"x","y","z","w"});
        }
    }

    static public class ShaderConstant extends Structure {
        public Vector4 m_Value;
        public long m_NameHash;
        public long pad1;
        protected List getFieldOrder() {
            return Arrays.asList(new String[] {"m_NameHash", "m_Value", "pad1"});
        }
    }

    // Matching the layout 1:1 with the struct in vertices.h
    static public class RenderObject extends Structure {
        public StencilTestParams    m_StencilTestParams;
        public Matrix4              m_WorldTransform; // 16 byte alignment for simd
        public ShaderConstant[]     m_Constants = new ShaderConstant[4];
        public int                  m_NumConstants;
        public int                  m_VertexStart;
        public int                  m_VertexCount;
        public int                  pad1;
        public byte                 m_SetBlendFactors;
        public byte                 m_SetStencilTest;
        public byte                 m_SetFaceWinding;
        public byte                 m_FaceWindingCCW;
        public byte[]               pad2 = new byte[(4*4) - 4];

        protected List getFieldOrder() {
            return Arrays.asList(new String[] {
                "m_StencilTestParams", "m_WorldTransform", "m_Constants",
                "m_NumConstants", "m_VertexStart", "m_VertexCount", "pad1",
                "m_SetBlendFactors", "m_SetStencilTest", "m_SetFaceWinding", "m_FaceWindingCCW", "pad2"});
        }

        public int getOffset(String name) {
            return super.fieldOffset(name);
        }
    }

    public static native RiveVertex RIVE_GetVertexBufferData(RivePointer rive, IntByReference vertexCount);
    public static native IntByReference RIVE_GetIndexBufferData(RivePointer rive, IntByReference indexCount);
    public static native RenderObject RIVE_GetRenderObjectData(RivePointer rive, IntByReference indexCount);

    // idea from https://stackoverflow.com/a/15431595/468516
    public static RiveVertex[] RIVE_GetVertexBuffer(RivePointer rive) {
        IntByReference pcount = new IntByReference();
        RiveVertex first = RIVE_GetVertexBufferData(rive, pcount);
        if (first == null)
        {
            System.out.printf("Vertex buffer is empty!");
            return new RiveVertex[0];
        }
        return (RiveVertex[])first.toArray(pcount.getValue());
    }

    public static int[] RIVE_GetIndexBuffer(RivePointer rive) {
        IntByReference pcount = new IntByReference();
        IntByReference p = RIVE_GetIndexBufferData(rive, pcount);
        if (pcount == null || p == null)
        {
            System.out.printf("Index buffer is empty!");
            return new int[0];
        }
        return p.getPointer().getIntArray(0, pcount.getValue());
    }

    public static RenderObject[] RIVE_GetRenderObjects(RivePointer rive) {
        IntByReference pcount = new IntByReference();
        RenderObject first = RIVE_GetRenderObjectData(rive, pcount);
        if (first == null)
        {
            System.out.printf("Render object buffer is empty!");
            return new RenderObject[0];
        }

        int ro_size = 288;
        if (first.size() != ro_size) {
            System.out.printf("RenderObject size is not %d, it was %d\n", ro_size, first.size());
            return new RenderObject[0];
        }

        return (RenderObject[])first.toArray(pcount.getValue());
    }

    ////////////////////////////////////////////////////////////////////////////////

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

        System.out.printf("Vertices:\n");
        int count = 0;
        for (RiveVertex vertex : RIVE_GetVertexBuffer(p)) {
            if (count > 10) {
                System.out.printf(" ...\n");
                break;
            }
            System.out.printf(" vertex %d: %.4f, %.4f\n", count++, vertex.x, vertex.y);
        }

        System.out.printf("Indices:\n");
        count = 0;
        for (int index : RIVE_GetIndexBuffer(p)) {
            if (count > 10) {
                System.out.printf(" ...\n");
                break;
            }
            System.out.printf(" index %d: %d\n", count++, index);
        }

        System.out.printf("Render Objects:\n");
        count = 0;
        for (RenderObject ro : RIVE_GetRenderObjects(p)) {
            if (count > 10) {
                System.out.printf(" ...\n");
                break;
            }
            System.out.printf(" ro %d: fw(ccw): %b  offset: %d  count: %d \n", count++, ro.m_FaceWindingCCW, ro.m_VertexStart, ro.m_VertexCount);
        }
    }
}