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

import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;

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

    public static native void TestPrint(String name);

    // Type Mapping in JNA
    // https://java-native-access.github.io/jna/3.5.1/javadoc/overview-summary.html#marshalling

    public static native Pointer RIVE_LoadFromPath(String path);
    public static native Pointer RIVE_LoadFromBuffer(Buffer buffer, int bufferSize);
    public static native void RIVE_Destroy(RivePointer rive);
    public static native int RIVE_GetNumAnimations(RivePointer rive);
    public static native String RIVE_GetAnimation(RivePointer rive, int index);

    public static RivePointer RIVE_LoadFileFromBuffer(byte[] buffer) {
        Buffer bb = ByteBuffer.wrap(buffer);
        Pointer p = RIVE_LoadFromBuffer(bb, bb.capacity());
        return new RivePointer(p);
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
            String animation = RIVE_GetAnimation(p, i);
            System.out.printf("Java: Animation %d: %s\n", i, animation);
        }
    }
}