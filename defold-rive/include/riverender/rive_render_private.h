#ifndef _RIVE_RENDER_PRIVATE_H_
#define _RIVE_RENDER_PRIVATE_H_

namespace rive
{
    // TODO: We get compiler warnings when copying this in the arrays, fix it?
    //       -> moving an object of non-trivially copyable type 'struct rive::PathDescriptor' 
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

    class SharedRenderer;
    class SharedRenderPath;
    class SharedRenderPaint : public RenderPaint
    {
    public:
        SharedRenderPaint(Context* ctx);
        ~SharedRenderPaint();
        void color(unsigned int value)                              override;
        void style(RenderPaintStyle value)                          override;
        void thickness(float value)                                 override;
        void join(StrokeJoin value)                                 override;
        void cap(StrokeCap value)                                   override;
        void blendMode(BlendMode value)                             override;
        void linearGradient(float sx, float sy, float ex, float ey) override;
        void radialGradient(float sx, float sy, float ex, float ey) override;
        void addStop(unsigned int color, float stop)                override;
        void completeGradient()                                     override;
        void invalidateStroke()                                     override;
        bool isVisible();
        RenderPaintStyle getStyle();
        void virtual drawPaint(SharedRenderer* renderer, const Mat2D& transform, SharedRenderPath* path);

        Context*                  m_Context;
        SharedRenderPaintBuilder* m_Builder;
        ContourStroke*            m_Stroke;
        HBuffer                   m_StrokeBuffer;
        PaintData                 m_Data;
        RenderPaintStyle          m_Style;
        float                     m_StrokeThickness;
        StrokeJoin                m_StrokeJoin;
        StrokeCap                 m_StrokeCap;
        bool                      m_StrokeDirty;
        bool                      m_IsVisible;
    };

    class SharedRenderPath : public ContourRenderPath
    {
    public:
        Context* m_Context;
        SharedRenderPath(Context* ctx);
        void renderStroke(SharedRenderer* renderer, SharedRenderPaint* paint,
            const Mat2D& transform, const Mat2D& localTransform = Mat2D::identity());
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
        std::vector<unsigned int> m_Indices; // todo: use jc::array instead
        jc::Array<StackEntry>     m_ClipPathStack;
        jc::Array<PathDescriptor> m_ClipPaths;
        jc::Array<PathDescriptor> m_AppliedClips;
        jc::Array<PathDrawEvent>  m_DrawEvents;
        Mat2D                     m_Transform;
        SharedRenderPaint*        m_RenderPaint;
        HBuffer                   m_IndexBuffer;
        float                     m_ContourQuality;
        uint8_t                   m_IsClippingDirty     : 1;
        uint8_t                   m_IsClipping          : 1;
        uint8_t                   m_IsClippingSupported : 1;

        SharedRenderer();
        ~SharedRenderer();
        void save()                            override;
        void restore()                         override;
        void transform(const Mat2D& transform) override;
        void clipPath(RenderPath* path)        override;
        void startFrame();

        void pushDrawEvent(PathDrawEvent evt);
        void setPaint(SharedRenderPaint* rp);
        void updateIndexBuffer(size_t contourLength);
    };

    ////////////////////////////////////////////////////
    // Stencil To Cover
    ////////////////////////////////////////////////////
    class StencilToCoverRenderPath;
    class StencilToCoverRenderPaint : public SharedRenderPaint
    {
    public:
        StencilToCoverRenderPaint(Context* ctx);
        void drawPaint(SharedRenderer* renderer, const Mat2D& transform, SharedRenderPath* path);
    };

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
        FillRule m_FillRule;
        HBuffer  m_VertexBuffer;

        StencilToCoverRenderPath(Context* ctx);
        ~StencilToCoverRenderPath();

        void stencil(SharedRenderer* renderer, const Mat2D& transform, unsigned int idx, bool isEvenOdd, bool isClipping);
        void cover(SharedRenderer* renderer, const Mat2D transform, const Mat2D transformLocal, bool isClipping);
        void updateBuffers(SharedRenderer* renderer);
        void fillRule(FillRule value) override;
        FillRule fillRule() const { return m_FillRule; }

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
        FillRule m_FillRule;
        float    m_ContourError;
        HBuffer  m_VertexBuffer;
        HBuffer  m_IndexBuffer;

        void addContours(void* tess, const Mat2D& m);
        void updateContour();
        void updateTesselation();

        TessellationRenderPath(Context* ctx);
        ~TessellationRenderPath();
        void drawMesh(SharedRenderer* renderer, const Mat2D& transform);
        void fillRule(FillRule value) override;
        FillRule fillRule() const { return m_FillRule; }
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
