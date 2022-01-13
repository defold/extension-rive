#if !defined(DM_DISABLE_LIBTESS)

#include <float.h>
#include <string.h>

#include <jc/array.h>
#include <tesselator.h>

#include <rive/artboard.hpp>
#include <rive/renderer.hpp>
#include <rive/contour_render_path.hpp>

#include "riverender/rive_render_api.h"
#include "riverender/rive_render_private.h"

#define PRINT_COMMANDS 0

namespace rive
{
    ////////////////////////////////////////////////////////
    // Tessellation - RenderPath
    ////////////////////////////////////////////////////////

    TessellationRenderPath::TessellationRenderPath(Context* ctx)
    : SharedRenderPath(ctx)
    , m_VertexBuffer(0)
    , m_IndexBuffer(0)
    {}

    TessellationRenderPath::~TessellationRenderPath()
    {
        m_Context->m_DestroyBufferCb(m_VertexBuffer, m_Context->m_BufferCbUserData);
        m_Context->m_DestroyBufferCb(m_IndexBuffer, m_Context->m_BufferCbUserData);
    }

    void TessellationRenderPath::fillRule(FillRule value)
    {
        m_FillRule = value;
    }

    void TessellationRenderPath::addContours(void* tess, const Mat2D& m)
    {
        if (isContainer())
        {
            for (int i = 0; i < (int) m_SubPaths.size(); ++i)
            {
                TessellationRenderPath* sharedPath = (TessellationRenderPath*) m_SubPaths[i].path();
                sharedPath->addContours(tess, m_SubPaths[i].transform());
            }
            return;
        }

        const int numComponents = 2;
        const int stride        = sizeof(float) * numComponents;

        Mat2D identity;
        if (identity == m)
        {
            tessAddContour((TESStesselator*)tess, numComponents, &m_ContourVertices[4][0], stride, m_ContourVertices.size());
        }
        else
        {
            const float m0 = m[0];
            const float m1 = m[1];
            const float m2 = m[2];
            const float m3 = m[3];
            const float m4 = m[4];
            const float m5 = m[5];

            float transformBuffer[512 * numComponents]; // todo: dynamic array

            rive::Vec2D* contourVertices = &m_ContourVertices[4];
            int numContours              = m_ContourVertices.size() - 4;

            for (int i = 0; i < numContours; i++)
            {
                float x                  = contourVertices[i][0];
                float y                  = contourVertices[i][1];
                transformBuffer[i*2    ] = m0 * x + m2 * y + m4;
                transformBuffer[i*2 + 1] = m1 * x + m3 * y + m5;
            }

            tessAddContour((TESStesselator*) tess, numComponents, transformBuffer, stride, numContours);
        }
    }

    void TessellationRenderPath::updateContour()
    {
        if (isContainer())
        {
            for (int i = 0; i < (int) m_SubPaths.size(); ++i)
            {
                TessellationRenderPath* sharedPath = (TessellationRenderPath*) m_SubPaths[i].path();
                sharedPath->updateContour();
            }
        }

        if (isDirty())
        {
            computeContour();
        }
    }

    void TessellationRenderPath::updateTesselation()
    {
        if (!isDirty())
        {
            return;
        }

        updateContour();

        TESStesselator* tess = tessNewTess(0);

        Mat2D identity;
        addContours((void*) tess, identity);

        const int windingRule = m_FillRule == FillRule::nonZero ? TESS_WINDING_NONZERO : TESS_WINDING_ODD;
        const int elementType = TESS_POLYGONS;
        const int polySize    = 3;
        const int vertexSize  = 2;

        if (tessTesselate(tess, windingRule, elementType, polySize, vertexSize, 0))
        {
            int              tessVerticesCount = tessGetVertexCount(tess);
            int              tessElementsCount = tessGetElementCount(tess);
            const TESSreal*  tessVertices      = tessGetVertices(tess);
            const TESSindex* tessElements      = tessGetElements(tess);

            m_VertexBuffer = m_Context->m_RequestBufferCb(m_VertexBuffer, BUFFER_TYPE_VERTEX_BUFFER, (void*) tessVertices, tessVerticesCount * sizeof(float) * vertexSize, m_Context->m_BufferCbUserData);
            m_IndexBuffer  = m_Context->m_RequestBufferCb(m_IndexBuffer, BUFFER_TYPE_INDEX_BUFFER, (void*) tessElements, tessElementsCount * sizeof(int) * polySize, m_Context->m_BufferCbUserData);
        }

        tessDeleteTess(tess);
    }

    void TessellationRenderPath::drawMesh(SharedRenderer* renderer, const Mat2D& transform)
    {
        updateTesselation();
    }

    ////////////////////////////////////////////////////////
    // Tessellation - Renderer
    ////////////////////////////////////////////////////////

    TessellationRenderer::TessellationRenderer(Context* ctx)
    {
        m_Context = ctx;
    }

    void TessellationRenderer::applyClipping()
    {
        bool same = true;
        if (m_ClipPaths.Size() == m_AppliedClips.Size())
        {
            for (int i = 0; i < (int) m_ClipPaths.Size(); ++i)
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

        if (m_ClipPaths.Size() > 0)
        {
            PathDrawEvent evtClippingBegin = { .m_Type = EVENT_CLIPPING_BEGIN };
            pushDrawEvent(evtClippingBegin);

            for (int i = 0; i < (int) m_ClipPaths.Size(); ++i)
            {
                const PathDescriptor& pd = m_ClipPaths[i];

                PathDrawEvent evtDraw = {
                    .m_Type           = EVENT_DRAW,
                    .m_Path           = pd.m_Path,
                    .m_TransformWorld = pd.m_Transform,
                };

                pushDrawEvent(evtDraw);

                ((TessellationRenderPath*) pd.m_Path)->drawMesh(this, m_Transform);
            }

            PathDrawEvent evtClippingEnd = {
                .m_Type             = EVENT_CLIPPING_END,
                .m_AppliedClipCount = (uint32_t) m_ClipPaths.Size(),
            };
            pushDrawEvent(evtClippingEnd);

            m_AppliedClips.SetCapacity(m_ClipPaths.Capacity());
            m_AppliedClips.SetSize(0);

            for (int i = 0; i < (int) m_ClipPaths.Size(); ++i)
            {
                m_AppliedClips.Push(m_ClipPaths[i]);
            }
        }
        else
        {
            PathDrawEvent evtClippingDisable = { .m_Type = EVENT_CLIPPING_DISABLE };
            pushDrawEvent(evtClippingDisable);
        }
    }

    void TessellationRenderer::drawImage(RenderImage* image, BlendMode value, float opacity)
    {

    }

    void TessellationRenderer::drawPath(RenderPath* path, RenderPaint* paint)
    {
        TessellationRenderPath*  p = (TessellationRenderPath*) path;
        SharedRenderPath*     srph = (SharedRenderPath*) path;
        SharedRenderPaint*      rp = (SharedRenderPaint*) paint;

        if (!rp->isVisible())
        {
            return;
        }

        if (m_IsClippingSupported && m_IsClippingDirty)
        {
            applyClipping();
        }

        setPaint(rp);

        if (rp->getStyle() != RenderPaintStyle::stroke)
        {
            PathDrawEvent evt = {
                .m_Type           = EVENT_DRAW,
                .m_Path           = path,
                .m_TransformWorld = m_Transform
            };
            pushDrawEvent(evt);
            p->drawMesh(this, m_Transform);
        }
        else
        {
            rp->drawPaint(this, m_Transform, srph);
        }
    }
}

#endif // RIVE_HAS_LIBTESS
