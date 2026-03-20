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
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadFactory;

import java.lang.reflect.Method;

public class Rive {
    private static final Object NATIVE_INIT_LOCK = new Object();
    private static volatile boolean sNativeInitialized = false;
    private static volatile float[] sFullscreenQuadVertices = null;
    private static final ExecutorService NATIVE_EXECUTOR = Executors.newSingleThreadExecutor(new ThreadFactory() {
        @Override
        public Thread newThread(Runnable runnable) {
            Thread thread = new Thread(runnable, "RiveNativeThread");
            thread.setDaemon(true);
            return thread;
        }
    });

    private static <T> T runOnNativeThread(Callable<T> callable) {
        Future<T> future = NATIVE_EXECUTOR.submit(callable);
        try {
            return future.get();
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new RuntimeException("Interrupted while waiting for Rive native call", e);
        } catch (ExecutionException e) {
            Throwable cause = e.getCause();
            if (cause instanceof RuntimeException) {
                throw (RuntimeException) cause;
            }
            throw new RuntimeException("Rive native call failed", cause);
        }
    }

    private static void runOnNativeThreadVoid(final Runnable runnable) {
        runOnNativeThread(new Callable<Void>() {
            @Override
            public Void call() {
                runnable.run();
                return null;
            }
        });
    }

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

    public static void Initialize() {
        if (sNativeInitialized) {
            return;
        }
        synchronized (NATIVE_INIT_LOCK) {
            if (sNativeInitialized) {
                return;
            }
            loadLibrary("libRiveExt" + getLibrarySuffix());
            sNativeInitialized = true;
        }
    }

    public static native RiveFile LoadFromBufferInternal(String path, byte[] buffer);
    public static native void Destroy(RiveFile rive_file);
    public static native void Update(RiveFile rive_file, float dt);
    public static native void SetArtboard(RiveFile rive_file, String name);
    public static native void SetStateMachine(RiveFile rive_file, String name);
    public static native void SetFitAlignment(RiveFile rive_file, int fit, int alignment);
    public static native void SetViewModel(RiveFile rive_file, String name);
    public static native Texture GetTexture(RiveFile rive_file);
    private static native float[] GetFullscreenQuadVerticesInternal();
    public static native void DebugPrint();

    public static float[] GetFullscreenQuadVertices() {
        return runOnNativeThread(new Callable<float[]>() {
            @Override
            public float[] call() {
                Initialize();
                if (sFullscreenQuadVertices == null) {
                    sFullscreenQuadVertices = GetFullscreenQuadVerticesInternal();
                }
                return sFullscreenQuadVertices;
            }
        });
    }

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
        public DefoldJNI.Aabb       bounds;

        public void Destroy() {
            Rive.DestroyInternal(this);
        }

        public void Update(float dt) {
            Rive.UpdateInternal(this, dt);
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

    private static boolean isValidRiveFile(RiveFile rive_file) {
        return rive_file != null && rive_file.pointer != 0L;
    }

    public static void UpdateInternal(RiveFile rive_file, float dt)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.Update(rive_file, dt);
            }
        });
    }

    public static void DestroyInternal(RiveFile rive_file)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.Destroy(rive_file);
            }
        });
    }

    public static void SetArtboardInternal(RiveFile rive_file, String artboard)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.SetArtboard(rive_file, artboard);
            }
        });
    }

    public static void SetStateMachineInternal(RiveFile rive_file, String stateMachine)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.SetStateMachine(rive_file, stateMachine);
            }
        });
    }

    public static void SetFitAlignmentInternal(RiveFile rive_file, int fit, int alignment)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.SetFitAlignment(rive_file, fit, alignment);
            }
        });
    }

    public static void SetViewModelInternal(RiveFile rive_file, String viewModel)
    {
        if (!isValidRiveFile(rive_file)) {
            return;
        }
        runOnNativeThreadVoid(new Runnable() {
            @Override
            public void run() {
                Initialize();
                Rive.SetViewModel(rive_file, viewModel);
            }
        });
    }

    public static Texture GetTextureInternal(RiveFile rive_file)
    {
        if (!isValidRiveFile(rive_file)) {
            return null;
        }
        return runOnNativeThread(new Callable<Texture>() {
            @Override
            public Texture call() {
                Initialize();
                return Rive.GetTexture(rive_file);
            }
        });
    }

    public static RiveFile LoadFromBuffer(String path, byte[] bytes)
    {
        return runOnNativeThread(new Callable<RiveFile>() {
            @Override
            public RiveFile call() {
                Initialize();
                return Rive.LoadFromBufferInternal(path, bytes);
            }
        });
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
            // Match the Vulkan-oriented native snapshot path by writing a
            // bottom-left origin TGA.
            header[17] = 8;

            output.write(header);
            // Native plugin snapshot data is ABGR for editor upload.
            // TGA expects BGRA bytes, so swizzle before writing.
            byte[] bgra = new byte[texture.data.length];
            for (int i = 0; i + 3 < texture.data.length; i += 4) {
                byte a = texture.data[i + 0];
                byte b = texture.data[i + 1];
                byte g = texture.data[i + 2];
                byte r = texture.data[i + 3];
                bgra[i + 0] = b;
                bgra[i + 1] = g;
                bgra[i + 2] = r;
                bgra[i + 3] = a;
            }
            output.write(bgra);
        }
        return true;
    }

    // ./utils/plugin/test.sh <rive scene path>
    public static void main(String[] args) throws IOException {
        Initialize();
        System.setProperty("java.awt.headless", "true");

        if (args.length < 1) {
            Usage();
            return;
        }

        String path = args[0];       // .riv

        long timeStart = System.currentTimeMillis();

        RiveFile rive_file = LoadFromPath(path);

        long timeEnd = System.currentTimeMillis();

        System.out.printf("%s: %s  (hashCode: %d)\n", rive_file!=null ? "Loaded ok":"Loading failed", path, rive_file!=null?rive_file.hashCode():0);

        if (rive_file == null) {
            System.exit(2);
            return;
        }

        System.out.printf("Loading took %d ms\n", (timeEnd - timeStart));

        System.out.printf("--------------------------------\n");

        System.out.printf("Java: hashCode: %d\n", rive_file.hashCode());

        rive_file.Update(0.0f);

        Texture texture = Rive.GetTextureInternal(rive_file);
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

        if (rive_file.bounds != null && rive_file.bounds.min != null && rive_file.bounds.max != null) {
            float minX = rive_file.bounds.min.x;
            float minY = rive_file.bounds.min.y;
            float maxX = rive_file.bounds.max.x;
            float maxY = rive_file.bounds.max.y;
            System.out.printf("Artboard bounds: min=(%f, %f) max=(%f, %f) size=(%f, %f)\n", minX, minY, maxX, maxY, maxX - minX, maxY - minY);
        } else {
            System.out.printf("Artboard bounds: <none>\n");
        }

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
