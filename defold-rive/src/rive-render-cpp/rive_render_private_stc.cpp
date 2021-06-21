#include <float.h>
#include <jc/array.h>

#include <renderer.hpp>
#include <artboard.hpp>

#include "rive/rive_render_api.h"
#include "rive/rive_render_private.h"

namespace rive
{
    StencilToCoverRenderer::StencilToCoverRenderer(Context* ctx)
    {
        m_Context = ctx;
        m_FullscreenPath = new StencilToCoverRenderPath(ctx);

        m_FullscreenPath->m_Limits = {
            .m_MinX = -1.0f,
            .m_MinY = -1.0f,
            .m_MaxX =  1.0f,
            .m_MaxY =  1.0f,
        };

        m_FullscreenPath->updateBuffers();
    }

    StencilToCoverRenderer::~StencilToCoverRenderer()
    {
        if (m_FullscreenPath)
        {
            delete m_FullscreenPath;
        }
    }

    void StencilToCoverRenderer::applyClipping()
    {
        m_IsClippingDirty = false;
        bool same = true;
        if (m_ClipPaths.Size() == m_AppliedClips.Size())
        {
            for (int i = 0; i < (int)m_ClipPaths.Size(); ++i)
            {
                const PathDescriptor& pdA = m_ClipPaths[i];
                const PathDescriptor& pdB = m_AppliedClips[i];

                if (pdA.m_Path != pdB.m_Path || (pdA.m_Transform == pdB.m_Transform))
                {
                    same = false;
                    break;
                }
            }
        }
        else
        {
            same = false;
        }

        if (same)
        {
            return;
        }

        m_IsClipping = false;

        PathDrawEvent evt = { .m_Type = EVENT_CLIPPING_BEGIN };
        pushDrawEvent(evt);

        if (m_ClipPaths.Size() > 0)
        {
            for (int i = 0; i < (int)m_ClipPaths.Size(); ++i)
            {
                const PathDescriptor& pd = m_ClipPaths[i];
                applyClipPath((StencilToCoverRenderPath*) pd.m_Path, pd.m_Transform);
            }

            m_AppliedClips.SetCapacity(m_ClipPaths.Capacity());
            m_AppliedClips.SetSize(0);

            for (int i = 0; i < m_ClipPaths.Size(); ++i)
            {
                m_AppliedClips.Push(m_ClipPaths[i]);
            }
        }

        evt.m_Type = EVENT_CLIPPING_END;
        pushDrawEvent(evt);
    }

    void StencilToCoverRenderer::applyClipPath(StencilToCoverRenderPath* path, const Mat2D& transform)
    {
        bool isEvenOdd = path->getFillRule() == FillRule::evenOdd;
        path->stencil(this, transform, 0, isEvenOdd, m_IsClipping);

        if (m_IsClipping)
        {
            Mat2D identityTransform;

            PathDrawEvent evt = {
                .m_Type           = EVENT_DRAW_COVER,
                .m_Path           = m_FullscreenPath,
                .m_TransformWorld = identityTransform,
                .m_TransformLocal = identityTransform,
                .m_IsClipping     = m_IsClipping,
            };
            pushDrawEvent(evt);
        }
        else
        {
            path->cover(this, transform, Mat2D(), m_IsClipping);
        }

        m_IsClipping = true;
    }

    void StencilToCoverRenderer::drawPath(RenderPath* path, RenderPaint* paint)
    {
        StencilToCoverRenderPath* p = (StencilToCoverRenderPath*) path;
        SharedRenderPaint*       rp = (SharedRenderPaint*) paint;

        if (rp->getStyle() != RenderPaintStyle::fill || !rp->isVisible())
        {
            return;
        }

        if (m_IsClippingSupported && m_IsClippingDirty)
        {
            applyClipping();
        }

        setPaint(rp);
        bool isEvenOdd = p->getFillRule() == FillRule::evenOdd;
        p->stencil(this, m_Transform, 0, isEvenOdd, m_IsClipping);
        p->cover(this, m_Transform, Mat2D(), m_IsClipping);
    }

    /* StencilToCoverRenderPath impl */
    StencilToCoverRenderPath::StencilToCoverRenderPath(Context* ctx)
    : SharedRenderPath(ctx)
    , m_ContourIndexCount(0)
    , m_ContourVertexBuffer(0)
    , m_ContourIndexBuffer(0)
    , m_CoverVertexBuffer(0)
    , m_CoverIndexBuffer(0)
    , m_ContourError(0.0f)
    {}

    StencilToCoverRenderPath::~StencilToCoverRenderPath()
    {
        void* requestUserData = m_Context->m_BufferCbUserData;
        m_Context->m_DestroyBufferCb(m_ContourVertexBuffer, requestUserData);
        m_Context->m_DestroyBufferCb(m_ContourIndexBuffer, requestUserData);
        m_Context->m_DestroyBufferCb(m_CoverVertexBuffer, requestUserData);
        m_Context->m_DestroyBufferCb(m_CoverIndexBuffer, requestUserData);
    }

    void StencilToCoverRenderPath::computeContour()
    {
        const float minSegmentLength = m_ContourError * m_ContourError;
        const float distTooFar       = m_ContourError;

        m_IsDirty              = false;
        m_ContourIndexCount    = 0;
        m_ContourVertexCount   = 1;
        m_ContourVertexData[0] = 0.0f;
        m_ContourVertexData[1] = 0.0f;

        m_Limits = {
            .m_MinX =  FLT_MAX,
            .m_MinY =  FLT_MAX,
            .m_MaxX = -FLT_MAX,
            .m_MaxY = -FLT_MAX,
        };

        float penX          = 0.0f;
        float penY          = 0.0f;
        float penDownX      = 0.0f;
        float penDownY      = 0.0f;
        float isPenDown     = false;
        int penDownIndex    = 1;
        int nextVertexIndex = 1;

        #define ADD_VERTEX(x,y) \
            { \
                m_ContourVertexData[m_ContourVertexCount * 2]     = x; \
                m_ContourVertexData[m_ContourVertexCount * 2 + 1] = y; \
                m_Limits.m_MinX = fmin(m_Limits.m_MinX, x); \
                m_Limits.m_MinY = fmin(m_Limits.m_MinY, y); \
                m_Limits.m_MaxX = fmax(m_Limits.m_MaxX, x); \
                m_Limits.m_MaxY = fmax(m_Limits.m_MaxY, y); \
                m_ContourVertexCount++; \
            }

        #define ADD_TRIANGLE(p0,p1,p2) \
            { \
                m_ContourIndexData[m_ContourIndexCount++] = p0; \
                m_ContourIndexData[m_ContourIndexCount++] = p1; \
                m_ContourIndexData[m_ContourIndexCount++] = p2; \
            }

        #define PEN_DOWN() \
            if (!isPenDown) \
            { \
                isPenDown = true; \
                penDownX = penX; \
                penDownY = penY; \
                ADD_VERTEX(penX, penY); \
                penDownIndex = nextVertexIndex; \
                nextVertexIndex++; \
            }

        for (int i=0; i < m_PathCommands.Size(); i++)
        {
            const PathCommand& pc = m_PathCommands[i];
            switch(pc.m_Command)
            {
                case TYPE_MOVE:
                    penX = pc.m_X;
                    penY = pc.m_Y;
                    break;
                case TYPE_LINE:
                    PEN_DOWN();
                    ADD_VERTEX(pc.m_X, pc.m_Y);
                    ADD_TRIANGLE(0, nextVertexIndex - 1, nextVertexIndex++);
                    break;
                case TYPE_CUBIC:
                {
                    PEN_DOWN();
                    const int size = m_ContourVertexCount;
                    segmentCubic(
                        Vec2D(penX, penY),
                        Vec2D(pc.m_OX, pc.m_OY),
                        Vec2D(pc.m_IX, pc.m_IY),
                        Vec2D(pc.m_X, pc.m_Y),
                        0.0f,
                        1.0f,
                        minSegmentLength,
                        distTooFar,
                        m_ContourVertexData,
                        m_ContourVertexCount,
                        &m_Limits);

                    const int addedVertices = m_ContourVertexCount - size;
                    for (int i = 0; i < addedVertices; ++i)
                    {
                        ADD_TRIANGLE(0, nextVertexIndex - 1, nextVertexIndex++);
                    }

                    penX = pc.m_X;
                    penY = pc.m_Y;
                } break;
                case TYPE_CLOSE:
                    if (isPenDown)
                    {
                        penX      = penDownX;
                        penY      = penDownY;
                        isPenDown = false;

                        if (m_ContourIndexCount > 0)
                        {
                            // Add the first vertex back to the indices to draw the
                            // close.
                            const int lastIndex = m_ContourIndexData[m_ContourIndexCount - 1];
                            ADD_TRIANGLE(0, lastIndex, penDownIndex);
                        }
                    }
                    break;
            }
        }

        // Always close the fill...
        if (isPenDown)
        {
            const int lastIndex = m_ContourIndexData[m_ContourIndexCount - 1];
            ADD_TRIANGLE(0, lastIndex, penDownIndex);
        }

        m_ContourVertexData[0] = m_Limits.m_MinX;
        m_ContourVertexData[1] = m_Limits.m_MinY;

        #undef ADD_VERTEX
        #undef PEN_DOWN
    }

    void StencilToCoverRenderPath::updateBuffers()
    {
        const float coverVertexData[] = {
            m_Limits.m_MinX, m_Limits.m_MinY, m_Limits.m_MaxX, m_Limits.m_MinY,
            m_Limits.m_MaxX, m_Limits.m_MaxY, m_Limits.m_MinX, m_Limits.m_MaxY,
        };

        const int coverIndexData[] = {
            0, 1, 2,
            2, 3, 0,
        };

        void* requestUserData = m_Context->m_BufferCbUserData;
        m_ContourVertexBuffer = m_Context->m_RequestBufferCb(m_ContourVertexBuffer, BUFFER_TYPE_VERTEX_BUFFER, m_ContourVertexData, m_ContourVertexCount * sizeof(float) * 2, requestUserData);
        m_ContourIndexBuffer  = m_Context->m_RequestBufferCb(m_ContourIndexBuffer, BUFFER_TYPE_INDEX_BUFFER, m_ContourIndexData, m_ContourIndexCount * sizeof(int), requestUserData);
        m_CoverVertexBuffer   = m_Context->m_RequestBufferCb(m_CoverVertexBuffer, BUFFER_TYPE_VERTEX_BUFFER, (void*) coverVertexData, sizeof(coverVertexData), requestUserData);
        m_CoverIndexBuffer    = m_Context->m_RequestBufferCb(m_CoverIndexBuffer, BUFFER_TYPE_INDEX_BUFFER, (void*) coverIndexData, sizeof(coverIndexData), requestUserData);
    }

    void StencilToCoverRenderPath::stencil(SharedRenderer* renderer, const Mat2D& transform, unsigned int idx, bool isEvenOdd, bool isClipping)
    {
        if (m_Paths.Size() > 0)
        {
            for (int i = 0; i < m_Paths.Size(); ++i)
            {
                StencilToCoverRenderPath* stcPath = (StencilToCoverRenderPath*) m_Paths[i].m_Path;
                if (stcPath)
                {
                    Mat2D stcPathTransform;
                    Mat2D::multiply(stcPathTransform, transform, m_Paths[i].m_Transform);
                    stcPath->stencil(renderer, stcPathTransform, idx++, isEvenOdd, isClipping);
                }
            }
            return;
        }

        float currentContourError = getContourError((HRenderer) renderer);
        m_IsDirty                 = m_IsDirty || currentContourError != m_ContourError;
        m_ContourError            = currentContourError;

        if (m_IsDirty)
        {
            computeContour();
            updateBuffers();
        }

        PathDrawEvent evt = {
            .m_Type           = EVENT_DRAW_STENCIL,
            .m_Path           = this,
            .m_TransformWorld = transform,
            .m_Idx            = idx,
            .m_IsEvenOdd      = isEvenOdd,
            .m_IsClipping     = isClipping,
        };

        renderer->pushDrawEvent(evt);
    }

    void StencilToCoverRenderPath::cover(SharedRenderer* renderer, const Mat2D transform, const Mat2D transformLocal, bool isClipping)
    {
        if (m_Paths.Size() > 0)
        {
            for (int i = 0; i < m_Paths.Size(); ++i)
            {
                StencilToCoverRenderPath* stcPath = (StencilToCoverRenderPath*) m_Paths[i].m_Path;
                if (stcPath)
                {
                    Mat2D stcWorldTransform;
                    Mat2D::multiply(stcWorldTransform, transform, m_Paths[i].m_Transform);
                    stcPath->cover(renderer, stcWorldTransform, m_Paths[i].m_Transform, isClipping);
                }
            }
            return;
        }

        float currentContourError = getContourError((HRenderer) renderer);
        m_IsDirty                 = m_IsDirty || currentContourError != m_ContourError;
        m_ContourError            = currentContourError;

        if (m_IsDirty)
        {
            computeContour();
            updateBuffers();
        }

        PathDrawEvent evt = {
            .m_Type           = EVENT_DRAW_COVER,
            .m_Path           = this,
            .m_TransformWorld = transform,
            .m_TransformLocal = transformLocal,
            .m_IsClipping     = isClipping,
        };

        renderer->pushDrawEvent(evt);
    }
}
