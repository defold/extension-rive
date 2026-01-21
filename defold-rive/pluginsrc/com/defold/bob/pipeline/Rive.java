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

    public static native RiveFile LoadFromBufferInternal(String path, byte[] buffer);
    public static native void Destroy(RiveFile rive_file);
    public static native void Update(RiveFile rive_file, float dt);
    public static native void SetArtboard(RiveFile rive_file, String name);
    public static native void SetStateMachine(RiveFile rive_file, String name);
    public static native void SetViewModel(RiveFile rive_file, String name);
    public static native void DebugPrint();

    public static class RiveFile {
        public String           path;
        public long             pointer;
        public String[]         artboards;
        public String[]         stateMachines;
        public String[]         viewModels;

        public void Destroy() {
            Rive.Destroy(this);
        }

        public void Update(float dt) {
            Rive.Update(this, dt);
        }
    }

    public static void UpdateInternal(RiveFile rive_file, float dt)
    {
        Rive.Update(rive_file, dt);
    }

    public static void SetArtboardInternal(RiveFile rive_file, String artboard)
    {
        Rive.SetArtboard(rive_file, artboard);
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

        rive_file.Update(0.0f);

        System.out.printf("--------------------------------\n");

        System.out.printf("Num artboards: %d\n", rive_file.artboards.length);
        for (String name : rive_file.artboards)
        {
            PrintIndent(1);
            System.out.printf("'%s'\n", name);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num state machines: %d\n", rive_file.stateMachines.length);
        for (String name : rive_file.stateMachines)
        {
            PrintIndent(1);
            System.out.printf("'%s'\n", name);
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num view models: %d\n", rive_file.viewModels.length);
        for (String name : rive_file.viewModels)
        {
            PrintIndent(1);
            System.out.printf("'%s'\n", name);
        }

        rive_file.Destroy();

        System.out.printf("--------------------------------\n");
    }
}
