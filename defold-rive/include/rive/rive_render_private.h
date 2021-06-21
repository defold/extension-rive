#ifndef _RIVE_RENDER_PRIVATE_H_
#define _RIVE_RENDER_PRIVATE_H_

namespace rive
{
    enum PathCommandType
    {
        TYPE_MOVE  = 0,
        TYPE_LINE  = 1,
        TYPE_CUBIC = 2,
        TYPE_CLOSE = 3,
    };

    struct PathCommand
    {
        PathCommandType m_Command;
        float           m_X;
        float           m_Y;
        float           m_OX;
        float           m_OY;
        float           m_IX;
        float           m_IY;
    };

    struct PathDescriptor
    {
        RenderPath* m_Path;
        Mat2D       m_Transform;
    };

    struct PathLimits
    {
        float    m_MinX;
        float    m_MinY;
        float    m_MaxX;
        float    m_MaxY;
    };

    struct GradientStop
    {
        unsigned int m_Color;
        float        m_Stop;
    };

    struct SharedRenderPaintBuilder
    {
        jc::Array<GradientStop> m_Stops;
        unsigned int            m_Color;
        FillType                m_GradientType;
        float                   m_StartX;
        float                   m_StartY;
        float                   m_EndX;
        float                   m_EndY;
    };

    struct Context
    {
        RenderMode      m_RenderMode;
        RequestBufferCb m_RequestBufferCb;
        DestroyBufferCb m_DestroyBufferCb;
        void*           m_BufferCbUserData;
    };

    class SharedRenderPaint : public RenderPaint
    {
    public:
        SharedRenderPaint();
        void color(unsigned int value)                              override;
        void style(RenderPaintStyle value)                          override { m_Style = value; }
        void thickness(float value)                                 override {}
        void join(StrokeJoin value)                                 override {}
        void cap(StrokeCap value)                                   override {}
        void blendMode(BlendMode value)                             override {}
        void linearGradient(float sx, float sy, float ex, float ey) override;
        void radialGradient(float sx, float sy, float ex, float ey) override;
        void addStop(unsigned int color, float stop)                override;
        void completeGradient()                                     override;
        inline RenderPaintStyle      getStyle()  { return m_Style; }
        inline bool                  isVisible() { return m_IsVisible; }

        SharedRenderPaintBuilder* m_Builder;
        PaintData                 m_Data;
        RenderPaintStyle          m_Style;
        bool                      m_IsVisible;
    };

    class SharedRenderPath : public RenderPath
    {
    public:
        // TODO: use a global buffer or something else
        static const uint32_t COUNTOUR_BUFFER_ELEMENT_COUNT = 512;

        Context*                  m_Context;
        jc::Array<PathCommand>    m_PathCommands;
        jc::Array<PathDescriptor> m_Paths;
        float                     m_ContourVertexData[COUNTOUR_BUFFER_ELEMENT_COUNT * 2];
        uint32_t                  m_ContourVertexCount;
        FillRule                  m_FillRule;
        bool                      m_IsDirty;
        bool                      m_IsShapeDirty;

        SharedRenderPath(Context* ctx);
        void            reset()                                                           override;
        void            addRenderPath(RenderPath* path, const Mat2D& transform)           override;
        void            fillRule(FillRule value)                                          override;
        void            moveTo(float x, float y)                                          override;
        void            lineTo(float x, float y)                                          override;
        void            cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
        virtual void    close()                                                           override;
        inline FillRule getFillRule() { return m_FillRule; }
        bool            isShapeDirty();
    };

    class SharedRenderer : public Renderer
    {
    public:
        static const int STACK_ENTRY_MAX_CLIP_PATHS = 16;
        struct StackEntry
        {
            Mat2D          m_Transform;
            PathDescriptor m_ClipPaths[STACK_ENTRY_MAX_CLIP_PATHS];
            uint8_t        m_ClipPathsCount;
        };

        Context*                  m_Context;
        jc::Array<StackEntry>     m_ClipPathStack;
        jc::Array<PathDescriptor> m_ClipPaths;
        jc::Array<PathDescriptor> m_AppliedClips;
        jc::Array<PathDrawEvent>  m_DrawEvents;
        Mat2D                     m_Transform;
        SharedRenderPaint*        m_RenderPaint;
        float                     m_ContourQuality;
        uint8_t                   m_IsClippingDirty     : 1;
        uint8_t                   m_IsClipping          : 1;
        uint8_t                   m_IsClippingSupported : 1;

        void save()                            override;
        void restore()                         override;
        void transform(const Mat2D& transform) override;
        void clipPath(RenderPath* path)        override;
        void startFrame();

        void pushDrawEvent(PathDrawEvent evt);
        void setPaint(SharedRenderPaint* rp);
    };

    ////////////////////////////////////////////////////
    // Stencil To Cover
    ////////////////////////////////////////////////////
    class StencilToCoverRenderPath;
    class StencilToCoverRenderer : public SharedRenderer
    {
    public:
        StencilToCoverRenderPath* m_FullscreenPath;

        StencilToCoverRenderer(Context* ctx);
        ~StencilToCoverRenderer();
        void drawPath(RenderPath* path, RenderPaint* paint) override;
        void applyClipping();
        void applyClipPath(StencilToCoverRenderPath* path, const Mat2D& transform);
    };

    class StencilToCoverRenderPath : public SharedRenderPath
    {
    public:
        // TODO: use a global buffer or something else
        static const uint32_t COUNTOUR_BUFFER_ELEMENT_COUNT = 128 * 3;
        uint32_t          m_ContourIndexData[COUNTOUR_BUFFER_ELEMENT_COUNT];
        uint32_t          m_ContourIndexCount;
        HBuffer           m_ContourVertexBuffer;
        HBuffer           m_ContourIndexBuffer;
        HBuffer           m_CoverVertexBuffer;
        HBuffer           m_CoverIndexBuffer;
        float             m_ContourError;
        PathLimits        m_Limits;

        StencilToCoverRenderPath(Context* ctx);
        ~StencilToCoverRenderPath();

        void stencil(SharedRenderer* renderer, const Mat2D& transform, unsigned int idx, bool isEvenOdd, bool isClipping);
        void cover(SharedRenderer* renderer, const Mat2D transform, const Mat2D transformLocal, bool isClipping);
        void computeContour();
        void updateBuffers();
        friend class StencilToCoverRenderer;
    };

    ////////////////////////////////////////////////////
    // Triangle Tessellation
    ////////////////////////////////////////////////////
    class TessellationRenderer : public SharedRenderer
    {
    public:
        TessellationRenderer(Context* ctx);
        void drawPath(RenderPath* path, RenderPaint* paint) override;
        void applyClipping();
    };

    class TessellationRenderPath : public SharedRenderPath
    {
    public:
        float   m_ContourError;
        HBuffer m_VertexBuffer;
        HBuffer m_IndexBuffer;

        void computeContour();
        void addContours(void* tess, const Mat2D& m);
        void updateContour(float contourError);
        void updateTesselation(float contourError);

        TessellationRenderPath(Context* ctx);
        ~TessellationRenderPath();
        void drawMesh(SharedRenderer* renderer, const Mat2D& transform);
    };

    ////////////////////////////////////////////////////
    // Helper Functions
    ////////////////////////////////////////////////////
    void segmentCubic(const Vec2D& from,
                      const Vec2D& fromOut,
                      const Vec2D& toIn,
                      const Vec2D& to,
                      float t1,
                      float t2,
                      float minSegmentLength,
                      float distTooFar,
                      float* vertices,
                      uint32_t& verticesCount,
                      PathLimits* pathLimits);
}

#endif
