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
    public static native Pointer RIVE_LoadFromBuffer(Buffer buffer, int bufferSize);
    public static native void RIVE_Destroy(RivePointer rive);
    public static native int RIVE_GetNumAnimations(RivePointer rive);
    public static native String RIVE_GetAnimation(RivePointer rive, int index);

    public static native int RIVE_GetNumBones(RivePointer rive);
    public static native String RIVE_GetBone(RivePointer rive, int index);

    // TODO: Create a jna Structure for this
    // Structures in JNA
    // https://java-native-access.github.io/jna/3.5.1/javadoc/overview-summary.html#structures
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

    public static native void RIVE_UpdateVertices(RivePointer rive, float dt);
    public static native int RIVE_GetVertexSize(); // size in bytes per vertex
    public static native int RIVE_GetVertexCount(RivePointer rive);
    public static native void RIVE_GetVertices(RivePointer rive, Buffer buffer, int bufferSize); // buffer size must be at least VertexSize * VertexCount bytes long

    public static RivePointer RIVE_LoadFileFromBuffer(byte[] buffer) {
        Buffer b = ByteBuffer.wrap(buffer);
        Pointer p = RIVE_LoadFromBuffer(b, b.capacity());
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
            System.out.printf("Java: Animation %d: %s\n", i, name);
        }

        for (int i = 0; i < RIVE_GetNumBones(p); ++i) {
            String name = RIVE_GetBone(p, i);
            System.out.printf("Java: Bone %d: %s\n", i, name);
        }

        RIVE_UpdateVertices(p, 0.0f);
    }
}