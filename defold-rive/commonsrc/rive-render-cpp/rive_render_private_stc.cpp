#include <float.h>
#include <jc/array.h>

#include <renderer.hpp>
#include <artboard.hpp>
#include <contour_render_path.hpp>

#include "rive/rive_render_api.h"
#include "rive/rive_render_private.h"

namespace rive
{
    StencilToCoverRenderer::StencilToCoverRenderer(Context* ctx)
    {
        const float coverVertices[] = {
            -1.0f, -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,  1.0f,
        };

        m_Context = ctx;
        m_FullscreenPath                 = new StencilToCoverRenderPath(ctx);
        m_FullscreenPath->m_VertexBuffer = m_Context->m_RequestBufferCb(m_FullscreenPath->m_VertexBuffer,
            BUFFER_TYPE_VERTEX_BUFFER, (void*) coverVertices, sizeof(coverVertices), m_Context->m_BufferCbUserData);
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
        bool isEvenOdd = path->fillRule() == FillRule::evenOdd;
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
        bool isEvenOdd = p->fillRule() == FillRule::evenOdd;
        p->stencil(this, m_Transform, 0, isEvenOdd, m_IsClipping);
        p->cover(this, m_Transform, Mat2D(), m_IsClipping);
    }

    /* StencilToCoverRenderPath impl */
    StencilToCoverRenderPath::StencilToCoverRenderPath(Context* ctx)
    : SharedRenderPath(ctx)
    , m_VertexBuffer(0)
    {}

    StencilToCoverRenderPath::~StencilToCoverRenderPath()
    {
        m_Context->m_DestroyBufferCb(m_VertexBuffer, m_Context->m_BufferCbUserData);
    }

    void StencilToCoverRenderPath::fillRule(FillRule value)
    {
        m_FillRule = value;
    }

    void StencilToCoverRenderPath::updateBuffers(SharedRenderer* renderer)
    {
        std::size_t vertexCount = m_ContourVertices.size();
        renderer->updateIndexBuffer(vertexCount - 3);
        m_VertexBuffer = renderer->m_Context->m_RequestBufferCb(m_VertexBuffer,
            BUFFER_TYPE_VERTEX_BUFFER, &m_ContourVertices[0][0], vertexCount * sizeof(float) * 2.0f, renderer->m_Context->m_BufferCbUserData);
    }

    void StencilToCoverRenderPath::stencil(SharedRenderer* renderer, const Mat2D& transform, unsigned int idx, bool isEvenOdd, bool isClipping)
    {
        if (isContainer())
        {
            for (int i = 0; i < m_SubPaths.size(); ++i)
            {
                Mat2D stcPathTransform;
                Mat2D::multiply(stcPathTransform, transform, m_SubPaths[i].transform());
                StencilToCoverRenderPath* stcPath = (StencilToCoverRenderPath*) m_SubPaths[i].path();
                stcPath->stencil(renderer, stcPathTransform, idx++, isEvenOdd, isClipping);
            }
            return;
        }

        if (isDirty())
        {
            computeContour();
            updateBuffers(renderer);
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
        if (isContainer())
        {
            for (int i = 0; i < m_SubPaths.size(); ++i)
            {
                Mat2D stcWorldTransform;
                Mat2D::multiply(stcWorldTransform, transform, m_SubPaths[i].transform());

                StencilToCoverRenderPath* stcPath = (StencilToCoverRenderPath*) m_SubPaths[i].path();
                stcPath->cover(renderer, stcWorldTransform, m_SubPaths[i].transform(), isClipping);
            }
            return;
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
