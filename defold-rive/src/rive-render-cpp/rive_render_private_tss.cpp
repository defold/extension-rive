#include <float.h>
#include <string.h>

#include <jc/array.h>
#include <tesselator.h>

#include <artboard.hpp>
#include <renderer.hpp>

#include "rive/rive_render_api.h"
#include "rive/rive_render_private.h"

#define PRINT_COMMANDS 0

namespace rive
{
	/* TessellationRenderPath Impl */
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

    void TessellationRenderPath::addContours(void* tess, const Mat2D& m)
    {
        m_IsShapeDirty = false;

        if (m_Paths.Size() > 0)
        {
            for (int i = 0; i < (int) m_Paths.Size(); ++i)
            {
                TessellationRenderPath* tessellationPath = (TessellationRenderPath*) m_Paths[i].m_Path;
                if (tessellationPath)
                {
                    tessellationPath->addContours(tess, m_Paths[i].m_Transform);
                }
            }
            return;
        }

        const int numComponents = 2;
        const int stride        = sizeof(float) * numComponents;

        Mat2D identity;
        if (identity == m)
        {
            tessAddContour((TESStesselator*)tess, numComponents, m_ContourVertexData, stride, m_ContourVertexCount);
        }
        else
        {
            const float m0 = m[0];
            const float m1 = m[1];
            const float m2 = m[2];
            const float m3 = m[3];
            const float m4 = m[4];
            const float m5 = m[5];

            float transformBuffer[COUNTOUR_BUFFER_ELEMENT_COUNT * numComponents];

            for (int i = 0; i < m_ContourVertexCount * numComponents; i += numComponents)
            {
                float x                = m_ContourVertexData[i];
                float y                = m_ContourVertexData[i + 1];
                transformBuffer[i    ] = m0 * x + m2 * y + m4;
                transformBuffer[i + 1] = m1 * x + m3 * y + m5;
            }

            tessAddContour((TESStesselator*)tess, numComponents, transformBuffer, stride, m_ContourVertexCount);
        }
    }

    void TessellationRenderPath::computeContour()
    {
        const float minSegmentLength = m_ContourError * m_ContourError;
        const float distTooFar       = m_ContourError;

        m_IsDirty       = false;
        float penX      = 0.0f;
        float penY      = 0.0f;
        float penDownX  = 0.0f;
        float penDownY  = 0.0f;
        float isPenDown = false;

        m_ContourVertexCount = 0;

        #define ADD_VERTEX(x,y) \
            { \
                m_ContourVertexData[m_ContourVertexCount * 2]     = x; \
                m_ContourVertexData[m_ContourVertexCount * 2 + 1] = y; \
                m_ContourVertexCount++; \
            }

        #define PEN_DOWN() \
            if (!isPenDown) \
            { \
                isPenDown = true; \
                penDownX = penX; \
                penDownY = penY; \
                ADD_VERTEX(penX, penY); \
            }

        for (int i=0; i < (int)m_PathCommands.Size(); i++)
        {
            const PathCommand& pc = m_PathCommands[i];
            switch(pc.m_Command)
            {
                case TYPE_MOVE:
                    penX = pc.m_X;
                    penY = pc.m_Y;
                    break;
                case TYPE_LINE:
                    PEN_DOWN()
                    ADD_VERTEX(pc.m_X, pc.m_Y);
                    break;
                case TYPE_CUBIC:
                    PEN_DOWN()
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
                        0);
                    penX = pc.m_X;
                    penY = pc.m_Y;
                    break;
                case TYPE_CLOSE:
                    if (isPenDown)
                    {
                        penX      = penDownX;
                        penY      = penDownY;
                        isPenDown = false;
                    }
                    break;
            }
        }

        #undef ADD_VERTEX
        #undef PEN_DOWN
    }

    void TessellationRenderPath::updateContour(float contourError)
    {
        if (m_Paths.Size() > 0)
        {
            for (int i=0; i < (int)m_Paths.Size(); i++)
            {
                TessellationRenderPath* sharedPath = (TessellationRenderPath*) m_Paths[i].m_Path;
                sharedPath->updateContour(contourError);
            }
        }

        m_IsDirty      = m_IsDirty      || contourError != m_ContourError;
        m_IsShapeDirty = m_IsShapeDirty || contourError != m_ContourError;
        m_ContourError = contourError;

        if (m_IsDirty)
        {
            computeContour();
        }
    }

    void TessellationRenderPath::updateTesselation(float contourError)
    {
        updateContour(contourError);

        if (!isShapeDirty())
        {
            return;
        }

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

            void* requestUserData = m_Context->m_BufferCbUserData;
            m_VertexBuffer = m_Context->m_RequestBufferCb(m_VertexBuffer, BUFFER_TYPE_VERTEX_BUFFER, (void*) tessVertices, tessVerticesCount * sizeof(float) * vertexSize, requestUserData);
            m_IndexBuffer  = m_Context->m_RequestBufferCb(m_IndexBuffer, BUFFER_TYPE_INDEX_BUFFER, (void*) tessElements, tessElementsCount * sizeof(int) * polySize, requestUserData);
        }
        else
        {
            printf("Unable to tessellate path %p\n", this);
        }

        tessDeleteTess(tess);
    }

    void TessellationRenderPath::drawMesh(SharedRenderer* renderer, const Mat2D& transform)
    {
        updateTesselation(getContourError(renderer));
    }

    /* Renderer impl */
    TessellationRenderer::TessellationRenderer(Context* ctx)
    {
        m_Context = ctx;
    }

    void TessellationRenderer::applyClipping()
    {
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

        if (m_ClipPaths.Size() > 0)
        {
            PathDrawEvent evtClippingBegin = { .m_Type = EVENT_CLIPPING_BEGIN };
            pushDrawEvent(evtClippingBegin);

            for (int i = 0; i < (int)m_ClipPaths.Size(); ++i)
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

            for (int i = 0; i < m_ClipPaths.Size(); ++i)
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

    void TessellationRenderer::drawPath(RenderPath* path, RenderPaint* paint)
    {
        TessellationRenderPath*  p = (TessellationRenderPath*) path;
        SharedRenderPaint*      rp = (SharedRenderPaint*) paint;

        if (rp->getStyle() != RenderPaintStyle::fill || !rp->isVisible())
        {
            return;
        }

        if (m_IsClippingSupported && m_IsClippingDirty)
        {
            applyClipping();
        }

        setPaint(rp);

        PathDrawEvent evt = {
            .m_Type           = EVENT_DRAW,
            .m_Path           = path,
            .m_TransformWorld = m_Transform
        };
        pushDrawEvent(evt);

        p->drawMesh(this, m_Transform);
    }
}
