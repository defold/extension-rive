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
import java.util.HashMap;

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
    public static native Texture GetTexture(RiveFile rive_file);
    public static native void DebugPrint();

    public static class RiveFile {
        public String           path;
        public long             pointer;        // The C++ pointer
        public String[]         artboards;
        public HashMap<String, String[]> stateMachines; // artboard -> state machines
        public String[]         viewModels;

        public ViewModelProperty[] viewModelProperties;
        public ViewModelEnum[]     viewModelEnums;
        public ViewModelInstanceNames[] viewModelInstanceNames;
        public DefaultViewModelInfo defaultViewModelInfo;

        public void Destroy() {
            Rive.Destroy(this);
        }

        public void Update(float dt) {
            Rive.Update(this, dt);
        }
    }

    public static class Texture {
        public int              width;
        public int              height;
        public int              format;
        public byte[]           data;
    }

    public static class ViewModelProperty {
        public String           viewModel;
        public String           name;
        public int              type;
        public String           typeName;
        public String           metaData;
        public String           value;
    }

    public static class ViewModelEnum {
        public String           name;
        public String[]         enumerants;
    }

    public static class ViewModelInstanceNames {
        public String           viewModel;
        public String[]         instances;
    }

    public static class DefaultViewModelInfo {
        public String           viewModel;
        public String           instance;
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

    private static boolean WriteTgaFile(String path, Texture texture) throws IOException {
        if (texture == null || texture.data == null || texture.width == 0 || texture.height == 0) {
            return false;
        }

        try (BufferedOutputStream output = new BufferedOutputStream(new FileOutputStream(path))) {
            byte[] header = new byte[18];
            header[2] = 2; // Uncompressed true-color image.
            header[12] = (byte)(texture.width & 0xFF);
            header[13] = (byte)((texture.width >> 8) & 0xFF);
            header[14] = (byte)(texture.height & 0xFF);
            header[15] = (byte)((texture.height >> 8) & 0xFF);
            header[16] = 32; // Bits per pixel.
            header[17] = (byte)(8 | 0x20); // 8-bit alpha, top-left origin.

            output.write(header);
            output.write(texture.data);
        }
        return true;
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

        Texture texture = Rive.GetTexture(rive_file);
        if (texture != null) {
            String screenshotPath = "screenshot.tga";
            if (WriteTgaFile(screenshotPath, texture)) {
                System.out.printf("Wrote screenshot %s (%dx%d)\n", screenshotPath, texture.width, texture.height);
            } else {
                System.out.printf("Failed to write screenshot %s\n", screenshotPath);
            }
        } else {
            System.out.printf("Failed to capture screenshot\n");
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("--------------------------------\n");

        System.out.printf("Num artboards: %d\n", rive_file.artboards.length);
        for (String name : rive_file.artboards)
        {
            PrintIndent(1);
            System.out.printf("'%s'\n", name);
        }

        System.out.printf("--------------------------------\n");

        if (rive_file.stateMachines != null) {
            System.out.printf("State machines by artboard: %d\n", rive_file.stateMachines.size());
            for (String artboard : rive_file.stateMachines.keySet()) {
                PrintIndent(1);
                System.out.printf("Artboard '%s'\n", artboard);
                String[] machines = rive_file.stateMachines.get(artboard);
                if (machines != null) {
                    for (String name : machines) {
                        PrintIndent(2);
                        System.out.printf("State Machine: '%s'\n", name);
                    }
                }
            }
        } else {
            System.out.printf("State machines by artboard: <none>\n");
        }

        System.out.printf("--------------------------------\n");

        System.out.printf("Num view models: %d\n", rive_file.viewModels.length);
        for (String name : rive_file.viewModels)
        {
            PrintIndent(1);
            System.out.printf("'%s'\n", name);
        }

        System.out.printf("--------------------------------\n");

        if (rive_file.defaultViewModelInfo != null) {
            System.out.printf("Default view model info:\n");
            PrintIndent(1);
            System.out.printf("viewModel='%s'\n", rive_file.defaultViewModelInfo.viewModel);
            PrintIndent(1);
            System.out.printf("instance='%s'\n", rive_file.defaultViewModelInfo.instance);
        } else {
            System.out.printf("Default view model info: <none>\n");
        }

        System.out.printf("--------------------------------\n");

        int propertyCount = rive_file.viewModelProperties != null ? rive_file.viewModelProperties.length : 0;
        System.out.printf("View model properties: %d\n", propertyCount);
        if (rive_file.viewModels != null && rive_file.viewModelProperties != null) {
            for (String viewModel : rive_file.viewModels) {
                PrintIndent(1);
                System.out.printf("ViewModel '%s'\n", viewModel);
                for (ViewModelProperty property : rive_file.viewModelProperties) {
                    if (property == null || property.viewModel == null || !property.viewModel.equals(viewModel)) {
                        continue;
                    }
                    PrintIndent(2);
                    String typeName = property.typeName != null && property.typeName.length() > 0 ? property.typeName : "unknown";
                    String valueText = property.value != null && property.value.length() > 0 ? property.value : "<n/a>";
                    System.out.printf("'%s' type=%s value=%s", property.name, typeName, valueText);
                    if (property.metaData != null && property.metaData.length() > 0) {
                        System.out.printf(" meta='%s'", property.metaData);
                    }
                    System.out.printf("\n");
                }
            }
        }

        System.out.printf("--------------------------------\n");

        int enumCount = rive_file.viewModelEnums != null ? rive_file.viewModelEnums.length : 0;
        System.out.printf("View model enums: %d\n", enumCount);
        if (rive_file.viewModelEnums != null) {
            for (ViewModelEnum viewEnum : rive_file.viewModelEnums) {
                if (viewEnum == null) {
                    continue;
                }
                PrintIndent(1);
                System.out.printf("'%s'\n", viewEnum.name);
                if (viewEnum.enumerants != null) {
                    for (String enumerant : viewEnum.enumerants) {
                        PrintIndent(2);
                        System.out.printf("'%s'\n", enumerant);
                    }
                }
            }
        }

        System.out.printf("--------------------------------\n");

        int instanceCount = rive_file.viewModelInstanceNames != null ? rive_file.viewModelInstanceNames.length : 0;
        System.out.printf("View model instance names: %d\n", instanceCount);
        if (rive_file.viewModelInstanceNames != null) {
            for (ViewModelInstanceNames instances : rive_file.viewModelInstanceNames) {
                if (instances == null) {
                    continue;
                }
                PrintIndent(1);
                System.out.printf("ViewModel '%s'\n", instances.viewModel);
                if (instances.instances != null) {
                    for (String instance : instances.instances) {
                        PrintIndent(2);
                        System.out.printf("'%s'\n", instance);
                    }
                }
            }
        }

        rive_file.Destroy();

        System.out.printf("--------------------------------\n");
    }
}
