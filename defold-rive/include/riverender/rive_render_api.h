#ifndef _RIVE_RENDER_API_H_
#define _RIVE_RENDER_API_H_

#include <stdint.h>
#include <rive/math/mat2d.hpp>

namespace rive
{
    class RenderPaint;
    class RenderPath;
    class Renderer;

    typedef uintptr_t    HContext;
    typedef uintptr_t    HBuffer;
    typedef RenderPaint* HRenderPaint;
    typedef RenderPath*  HRenderPath;
    typedef Renderer*    HRenderer;

    enum BufferType
    {
        BUFFER_TYPE_VERTEX_BUFFER = 0,
        BUFFER_TYPE_INDEX_BUFFER  = 1,
    };

    typedef HBuffer (*RequestBufferCb)(HBuffer buffer, BufferType type, void* data, unsigned int dataSize, void* userData);
    typedef void    (*DestroyBufferCb)(HBuffer buffer, void* userData);

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

    struct DrawBuffers
    {
        HBuffer m_VertexBuffer;
        HBuffer m_IndexBuffer;
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

    HContext            createContext();
    void                destroyContext(HContext ctx);
    RenderMode          getRenderMode(HContext ctx);
    void                setBufferCallbacks(HContext ctx, RequestBufferCb rcb, DestroyBufferCb dcb, void* userData = 0);
    void                setRenderMode(HContext ctx, RenderMode mode);
    RenderPath*         createRenderPath(HContext ctx);
    RenderPaint*        createRenderPaint(HContext ctx);

    HRenderer           createRenderer(HContext ctx);
    void                destroyRenderer(HRenderer renderer);
    void                newFrame(HRenderer renderer);
    void                resetClipping(HRenderer renderer);
    void                setContourQuality(HRenderer renderer, float quality);
    void                setClippingSupport(HRenderer renderer, bool state);
    void                setTransform(HRenderer renderer, const Mat2D& transform);
    bool                getClippingSupport(HRenderer renderer);
    float               getContourError(HRenderer renderer);
    uint32_t            getDrawEventCount(HRenderer renderer);
    const DrawBuffers   getDrawBuffers(HContext ctx, HRenderer renderer, HRenderPath path);
    const PathDrawEvent getDrawEvent(HRenderer renderer, uint32_t i);
    const PaintData     getPaintData(HRenderPaint paint);
}

#endif /* _RIVE_RENDER_API_H_ */
