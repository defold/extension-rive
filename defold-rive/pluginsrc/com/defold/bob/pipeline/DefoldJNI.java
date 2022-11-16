//
// License: MIT
//

package com.dynamo.bob.pipeline;

public class DefoldJNI {

    public static class Vec4 {
        public float x;
        public float y;
        public float z;
        public float w;
    }

    public static class Matrix4 {
        public float[]  m;
    }

    public static class Aabb {
        public Vec4 min;
        public Vec4 max;
    }
}
