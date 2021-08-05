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
import java.nio.ByteBuffer;

// import java.io.File;
// import java.nio.Buffer;

import com.sun.jna.Native;
import com.sun.jna.Pointer;

public class Rive {

    // static void addToPath(String variable, String path) {
    //     String newPath = null;

    //     // Check if jna.library.path is set externally.
    //     if (System.getProperty(variable) != null) {
    //         newPath = System.getProperty(variable);
    //     }

    //     if (newPath == null) {
    //         // Set path where texc_shared library is found.
    //         newPath = path;
    //     } else {
    //         // Append path where texc_shared library is found.
    //         newPath += File.pathSeparator + path;
    //     }

    //     // Set the concatenated jna.library path
    //     System.setProperty(variable, newPath);
    //     Bob.verbose("Set %s to '%s'", variable, newPath);
    // }

    // static {
    //     try {
    //         // Extract and append Bob bundled texc_shared path.
    //         Platform platform = Platform.getJavaPlatform();
    //         File lib = new File(Bob.getLib(platform, "texc_shared"));

    //         String libPath = lib.getParent();
    //         addToPath("jna.library.path", libPath);
    //         addToPath("java.library.path", libPath);
    //         Native.register("texc_shared");
    //     } catch (Exception e) {
    //         System.out.println("FATAL: " + e.getMessage());
    //     }
    // }

    static {
        try {
            // Extract and append Bob bundled texc_shared path.
            // Platform platform = Platform.getJavaPlatform();
            // File lib = new File(Bob.getLib(platform, "texc_shared"));

            // String libPath = lib.getParent();
            // addToPath("jna.library.path", libPath);
            // addToPath("java.library.path", libPath);
            Native.register("RiveExt");
        } catch (Exception e) {
            System.out.println("FATAL: " + e.getMessage());
        }
    }

    // public static native Pointer TEXC_Create(int width, int height, int pixelFormat, int colorSpace, int compressionType, Buffer data);
    // public static native void TEXC_Destroy(Pointer texture);

    // public static native int TEXC_GetDataSizeCompressed(Pointer texture, int minMap);
    // public static native int TEXC_GetDataSizeUncompressed(Pointer texture, int minMap);
    // public static native int TEXC_GetTotalDataSize(Pointer texture);
    // public static native int TEXC_GetData(Pointer texture, Buffer outData, int maxOutDataSize);
    // public static native int TEXC_GetCompressionFlags(Pointer texture);

    public static native void TestPrint(String name);
    public static native Pointer RIVE_Create(String path);
    public static native int RIVE_GetNumAnimations(Pointer rive);
    public static native String RIVE_GetAnimation(Pointer rive, int index);

    private static void Usage() {
        System.out.printf("Usage: pluginRiveExt.jar <file>\n");
        System.out.printf("\n");
    }

    // Used for testing functions
    // See ./utils/test_plugin.sh
    public static void main(String[] args) throws IOException {
        System.setProperty("java.awt.headless", "true");
        System.out.printf("Rive.main\n");

        TestPrint("Rive Plugin");

        if (args.length < 1) {
            Usage();
            return;
        }

        String path = args[0];
        Pointer rive_file = RIVE_Create(path);

        if (rive_file != null) {
            System.out.printf("Loaded %s\n", path);
        } else {
            System.err.printf("Failed to load %s\n", path);
            return;
        }

        for (int i = 0; i < RIVE_GetNumAnimations(rive_file); ++i) {
            String animation = RIVE_GetAnimation(rive_file, i);
            System.out.printf("Java: Animation %d: %s\n", i, animation);
        }

        // try (BufferedInputStream is = new BufferedInputStream(new FileInputStream(args[0]));
        //      BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(args[1]))) {
        //     TextureImage texture = generate(is);
        //     texture.writeTo(os);
        // }
    }
}