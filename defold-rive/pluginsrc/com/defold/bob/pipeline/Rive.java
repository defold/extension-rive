//
// License: MIT
//

package com.dynamo.bob.pipeline;

import com.dynamo.bob.pipeline.DefoldJNI.Aabb;
import com.dynamo.bob.pipeline.DefoldJNI.Vec4;
import com.dynamo.bob.pipeline.DefoldJNI.Matrix4;
import com.dynamo.bob.pipeline.RenderJNI.Constant;
import com.dynamo.bob.pipeline.RenderJNI.StencilTestFunc;
import com.dynamo.bob.pipeline.RenderJNI.StencilTestParams;
import com.dynamo.bob.pipeline.RenderJNI.RenderObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.FileNotFoundException;
import java.io.File;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.util.List;
import java.util.Arrays;

import java.lang.reflect.Method;

public class Rive {

    static String getLibrarySuffix() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("mac")) return ".dylib";
        if (os.contains("win")) return ".dll";
        if (os.contains("linux")) return ".so";
        return "";
    }

    static void loadLibrary(String libName) {
        String java_library_path = System.getProperty("java.library.path");
        String [] paths = java_library_path.split(File.pathSeparator);

        for (String path : paths) {
            try {
                File libFile = new File(path, libName);
                if (!libFile.exists())
                    continue;

                System.load(libFile.getAbsolutePath());
                return;
            } catch (Exception e) {
                e.printStackTrace();
                System.err.printf("Native code library failed to load: %s\n", e);
                System.exit(1);
            }
        }
        System.err.printf("Failed to find and load: %s from java.library.path='%s'\n", libName, java_library_path);
        System.exit(1);
    }

    static {
        loadLibrary("libRiveExt" + getLibrarySuffix());
    }


    public static class StateMachineInput {
        public String               name;
        public String               type;
    }

    public static class StateMachine {
        public String               name;
        public StateMachineInput[]  inputs;
    }

    public static class Bone {
        public String   name;
        public int      index;
        public Bone     parent;
        public Bone[]   children;

        public float    posX;
        public float    posY;
        public float    scaleX;
        public float    scaleY;
        public float    rotation;
        public float    length;

    }

    public static native RiveFile LoadFromBufferInternal(String path, byte[] buffer);
    public static native void Destroy(RiveFile rive_file);
    public static native void Update(RiveFile rive_file, float dt, byte[] texture_set_buffer);
    public static native void DebugPrint();

    public static class RiveFile {
        public String           path;
        public long             pointer;

        public Aabb             aabb;
        public float[]          vertices;
        public int[]            indices;
        public String[]         animations;
        public StateMachine[]   stateMachines;
        public Bone[]           bones;
        public RenderObject[]   renderObjects;
        public byte[]           texture_set_bytes;

        public void Destroy() {
            Rive.Destroy(this);
        }

        public void Update(float dt) {
            Rive.Update(this, dt, texture_set_bytes);
        }
    }

    public static void UpdateInternal(RiveFile rive_file, float dt, byte[] texture_set_pb)
    {
        Rive.Update(rive_file, dt, texture_set_pb);
    }

    public static RiveFile LoadFromBuffer(String path, byte[] bytes)
    {
        RiveFile rive_file = Rive.LoadFromBufferInternal(path, bytes);
        return rive_file;
    }

    public static RiveFile LoadFromPath(String path) throws FileNotFoundException, IOException
    {
        InputStream inputStream = new FileInputStream(new File(path));
        byte[] bytes = new byte[inputStream.available()];
        inputStream.read(bytes);

        return LoadFromBuffer(path, bytes);
    }

    public static void DebugPrintTest()
    {
        System.out.printf("MAWE: DebugPrintTest()\n");
        Rive.DebugPrint();
    }

    // ////////////////////////////////////////////////////////////////////////////////

    private static void Usage() {
        System.out.printf("Usage: Rive.class file.riv\n");
        System.out.printf("\n");
    }

    private static void PrintIndent(int indent) {
        for (int i = 0; i < indent; ++i) {
            System.out.printf("  ");
        }
    }

    private static void DebugStateMachine(StateMachine sm)
    {
        PrintIndent(1);
        System.out.printf("StateMachine: %s\n", sm.name);
        PrintIndent(2);
        System.out.printf("inputs\n");
        for (StateMachineInput input : sm.inputs)
        {
            PrintIndent(2);
            System.out.printf("name: '%s'  type: %s\n", input.name, input.type);
        }
    }

    private static void DebugBone(Bone bone, int indent)
    {
        if (bone == null)
            System.out.printf("Bone is null\n");
        PrintIndent(indent+1); System.out.printf("Bone %d: '%s'\n", bone.index, bone.name);
        PrintIndent(indent+2); System.out.printf("parent: %s\n", bone.parent != null ? bone.parent.name : "-");
        PrintIndent(indent+2); System.out.printf("pos:   %f, %f\n", bone.posX, bone.posY);
        PrintIndent(indent+2); System.out.printf("scale: %f, %f\n", bone.scaleX, bone.scaleY);
        PrintIndent(indent+2); System.out.printf("rotation/length: %f, %f\n", bone.rotation, bone.length);

        for (Bone child : bone.children) {
            DebugBone(child, indent+1);
        }
    }

    private static void DebugConstantBuffer(Constant[] constants, int indent)
    {
        for (Constant constant : constants)
        {
            PrintIndent(indent); System.out.printf("constant: name: %d  values:\n", constant.nameHash);
            for (Vec4 value : constant.values)
            {
                PrintIndent(indent+1); System.out.printf("(%f, %f, %f, %f)\n", value.x, value.y, value.z, value.w);
            }
        }
    }

    private static void DebugStencilTestFunc(String name, StencilTestFunc func, int indent)
    {
        PrintIndent(indent+0); System.out.printf("StencilTestFunc %s:\n", name);
        PrintIndent(indent+1); System.out.printf("func:     %d\n", func.func);
        PrintIndent(indent+1); System.out.printf("opSFail:  %d\n", func.opSFail);
        PrintIndent(indent+1); System.out.printf("opDPFail: %d\n", func.opDPFail);
        PrintIndent(indent+1); System.out.printf("opDPPass: %d\n", func.opDPPass);
    }
    private static void DebugStencilTestParams(StencilTestParams params, int indent)
    {
        PrintIndent(indent+0); System.out.printf("StencilTestParams:\n");
        DebugStencilTestFunc("front", params.front, indent+1);
        DebugStencilTestFunc("back", params.back, indent+1);
        PrintIndent(indent+1); System.out.printf("ref:                  %d\n", params.ref);
        PrintIndent(indent+1); System.out.printf("refMask:              %d\n", params.refMask);
        PrintIndent(indent+1); System.out.printf("bufferMask:           %d\n", params.bufferMask);
        PrintIndent(indent+1); System.out.printf("colorBufferMask:      %d\n", params.colorBufferMask);
        PrintIndent(indent+1); System.out.printf("clearBuffer:          %d\n", params.clearBuffer);
        PrintIndent(indent+1); System.out.printf("separateFaceStates:   %d\n", params.separateFaceStates);
    }

    private static void DebugRenderObject(RenderObject ro, int index)
    {
        PrintIndent(1); System.out.printf("RenderObject %d:\n", index);

        PrintIndent(2); System.out.printf("constantBuffer:\n");
        DebugConstantBuffer(ro.constantBuffer, 3);

        // PrintIndent(2); System.out.printf("worldTransform:          %d\n", ro.worldTransform);         // dmVMath::Matrix4
        // PrintIndent(2); System.out.printf("textureTransform:        %d\n", ro.textureTransform);       // dmVMath::Matrix4
        PrintIndent(2); System.out.printf("vertexBuffer:            %d\n", ro.vertexBuffer);           // dmGraphics::HVertexBuffer
        PrintIndent(2); System.out.printf("vertexDeclaration:       %d\n", ro.vertexDeclaration);      // dmGraphics::HVertexDeclaration
        PrintIndent(2); System.out.printf("indexBuffer:             %d\n", ro.indexBuffer);            // dmGraphics::HIndexBuffer
        PrintIndent(2); System.out.printf("material:                %d\n", ro.material);               // HMaterial
        //PrintIndent(2); System.out.printf("textures:                %d\n", ro.textures);               //[MAX_TEXTURE_COUNT]; // dmGraphics::HTexture
        PrintIndent(2); System.out.printf("primitiveType:           %d\n", ro.primitiveType);          // dmGraphics::PrimitiveType
        PrintIndent(2); System.out.printf("indexType:               %d\n", ro.indexType);              // dmGraphics::Type
        PrintIndent(2); System.out.printf("sourceBlendFactor:       %d\n", ro.sourceBlendFactor);      // dmGraphics::BlendFactor
        PrintIndent(2); System.out.printf("destinationBlendFactor:  %d\n", ro.destinationBlendFactor); // dmGraphics::BlendFactor
        PrintIndent(2); System.out.printf("faceWinding:             %d\n", ro.faceWinding);            // dmGraphics::FaceWinding
        PrintIndent(2); System.out.printf("vertexStart:             %d\n", ro.vertexStart);            // uint32_t
        PrintIndent(2); System.out.printf("vertexCount:             %d\n", ro.vertexCount);            // uint32_t
        PrintIndent(2); System.out.printf("setBlendFactors:         %s\n", ro.setBlendFactors?"true":"false");        // uint8_t :1
        PrintIndent(2); System.out.printf("setStencilTest:          %s\n", ro.setStencilTest?"true":"false");         // uint8_t :1
        PrintIndent(2); System.out.printf("setFaceWinding:          %s\n", ro.setFaceWinding?"true":"false");         // uint8_t :1
        DebugStencilTestParams(ro.stencilTestParams, 2);
    }

    private static void DebugVertex(float[] vertices, int index)
    {
        PrintIndent(1); System.out.printf("vtx %d:   %f, %f, %f, %f\n", index,
                                            vertices[index*4+0],
                                            vertices[index*4+1],
                                            vertices[index*4+2],
                                            vertices[index*4+3]);
    }

    private static void DebugTriangle(int[] indices, int index)
    {
        PrintIndent(1); System.out.printf("tri %d:   %d, %d, %d\n", index,
                                            indices[index*3+0],
                                            indices[index*3+1],
                                            indices[index*3+2]);
    }

    private static void DebugVec4(String name, Vec4 v, int indent)
    {
        PrintIndent(indent); System.out.printf("%s: %f, %f, %f, %f\n", name, v.x, v.y, v.z, v.w);
    }

    private static void DebugAabb(String name, Aabb aabb, int indent)
    {
        PrintIndent(indent); System.out.printf("%s\n", name!=null?name:"aabb");
        DebugVec4("min", aabb.min, indent+1);
        DebugVec4("max", aabb.max, indent+1);
    }

    // ./utils/test_plugin.sh <rive scene path>
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        String path = args[0];       // .riv

        long timeStart = System.currentTimeMillis();

        RiveFile rive_file = LoadFromPath(path);

        long timeEnd = System.currentTimeMillis();

        System.out.printf("Loaded %s %s  (hashCode: %d)\n", path, rive_file!=null ? "ok":"failed", rive_file.hashCode());
        System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        System.out.printf("--------------------------------\n");

        System.out.printf("Java: hashCode: %d\n", rive_file.hashCode());

        DebugAabb("AABB", rive_file.aabb, 0);

        rive_file.Update(0.0f);

        System.out.printf("Num animations: %d\n", rive_file.animations.length);
        for (String animation : rive_file.animations)
        {
            PrintIndent(1);
            System.out.printf("%s\n", animation);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num state machines: %d\n", rive_file.stateMachines.length);
        for (StateMachine stateMachine : rive_file.stateMachines)
        {
            DebugStateMachine(stateMachine);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Bones:\n");
        for (Bone bone : rive_file.bones)
        {
            DebugBone(bone, 0);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num render objects: %d\n", rive_file.renderObjects.length);
        int j = 0;
        for (RenderObject ro : rive_file.renderObjects)
        {
            DebugRenderObject(ro, j++);
        }

        System.out.printf("--------------------------------\n");

        int num_vertices = rive_file.vertices.length/4;
        System.out.printf("Num vertices: %d\n", num_vertices);
        for (int i = 0; i < num_vertices; ++i)
        {
            DebugVertex(rive_file.vertices, i);
        }

        System.out.printf("--------------------------------\n");

        int num_triangles = rive_file.indices.length/3;
        System.out.printf("Num triangles: %d\n", num_triangles);
        for (int i = 0; i < num_triangles; ++i)
        {
            DebugTriangle(rive_file.indices, i);
        }

        rive_file.Destroy();

        System.out.printf("--------------------------------\n");
    }
}
