#ifndef _RIVE_RENDER_API_H_
#define _RIVE_RENDER_API_H_

namespace rive
{
    typedef uintptr_t    HBuffer;
    typedef RenderPaint* HRenderPaint;
    typedef RenderPath*  HRenderPath;
    typedef Renderer*    HRenderer;

    enum BufferType
    {
        BUFFER_TYPE_VERTEX_BUFFER = 0,
        BUFFER_TYPE_INDEX_BUFFER  = 1,
    };

    typedef HBuffer (*RequestBufferCb)(HBuffer buffer, BufferType type, void* data, unsigned int dataSize);
    typedef void    (*DestroyBufferCb)(HBuffer buffer);

    enum FillType
    {
        FILL_TYPE_NONE   = 0,
        FILL_TYPE_SOLID  = 1,
        FILL_TYPE_LINEAR = 2,
        FILL_TYPE_RADIAL = 3,
    };

    enum RenderMode
    {
        MODE_TESSELLATION     = 0,
        MODE_STENCIL_TO_COVER = 1,
    };

    enum PathDrawEventType
    {
        EVENT_NONE             = 0,
        EVENT_DRAW             = 1,
        EVENT_DRAW_STENCIL     = 2,
        EVENT_DRAW_COVER       = 3,
        EVENT_SET_PAINT        = 4,
        EVENT_CLIPPING_BEGIN   = 5,
        EVENT_CLIPPING_END     = 6,
        EVENT_CLIPPING_DISABLE = 7,
    };

    struct PathDrawEvent
    {
        PathDrawEventType m_Type;
        HRenderPath       m_Path;
        HRenderPaint      m_Paint;
        Mat2D             m_TransformWorld;
        Mat2D             m_TransformLocal;
        uint32_t          m_Idx              : 22;
        uint32_t          m_AppliedClipCount : 8;
        uint32_t          m_IsEvenOdd        : 1;
        uint32_t          m_IsClipping       : 1;
    };

    struct CreateRendererParams
    {
        RenderMode      m_RenderMode;
        RequestBufferCb m_RequestBufferCb;
        DestroyBufferCb m_DestroyBufferCb;
    };

    struct DrawBuffers
    {
        union
        {
            struct
            {
                HBuffer m_ContourVertexBuffer;
                HBuffer m_ContourIndexBuffer;
                HBuffer m_CoverVertexBuffer;
                HBuffer m_CoverIndexBuffer;
            } m_StencilToCover;

            struct
            {
                HBuffer m_VertexBuffer;
                HBuffer m_IndexBuffer;
            } m_Tessellation;
        };
    };

    struct PaintData
    {
        static const int MAX_STOPS = 16;
        FillType     m_FillType;
        unsigned int m_StopCount;
        float        m_Stops[MAX_STOPS];
        float        m_Colors[MAX_STOPS * 4];
        float        m_GradientLimits[4];
    };

    void                setBufferCallbacks(RequestBufferCb rcb, DestroyBufferCb dcb);
    void                setRenderMode(RenderMode mode);
    void                setContourQuality(HRenderer renderer, float quality);
    void                setClippingSupport(HRenderer renderer, bool state);
    bool                getClippingSupport(HRenderer renderer);
    float               getContourError(HRenderer renderer);
    const PaintData     getPaintData(HRenderPaint paint);
    RenderMode          getRenderMode();
    HRenderer           createRenderer();
    void                destroyRenderer(HRenderer renderer);
    void                newFrame(HRenderer renderer);
    uint32_t            getDrawEventCount(HRenderer renderer);
    const PathDrawEvent getDrawEvent(HRenderer renderer, uint32_t i);
    const DrawBuffers   getDrawBuffers(HRenderPath path);
}

#endif /* _RIVE_RENDER_API_H_ */
