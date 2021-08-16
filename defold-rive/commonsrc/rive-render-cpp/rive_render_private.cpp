#include <float.h>
#include <string.h>

#include <jc/array.h>

#include <artboard.hpp>
#include <contour_render_path.hpp>

#include "rive/rive_render_api.h"
#include "rive/rive_render_private.h"

namespace rive
{
    static void getColorArrayFromUint(unsigned int colorIn, float* rgbaOut)
    {
        rgbaOut[0] = (float)((0x00ff0000 & colorIn) >> 16) / 255.0f;
        rgbaOut[1] = (float)((0x0000ff00 & colorIn) >> 8)  / 255.0f;
        rgbaOut[2] = (float)((0x000000ff & colorIn) >> 0)  / 255.0f;
        rgbaOut[3] = (float)((0xff000000 & colorIn) >> 24) / 255.0f;
    }

    SharedRenderPaint::SharedRenderPaint()
    : m_Builder(0)
    , m_Data({})
    {}

    void SharedRenderPaint::color(unsigned int value)
    {
        m_Data = {
            .m_FillType = FILL_TYPE_SOLID,
            .m_StopCount = 1
        };

        getColorArrayFromUint(value, &m_Data.m_Colors[0]);

        m_IsVisible = m_Data.m_Colors[3] > 0.0f;
    }

    void SharedRenderPaint::linearGradient(float sx, float sy, float ex, float ey)
    {
        m_Builder                 = new SharedRenderPaintBuilder();
        m_Builder->m_GradientType = FILL_TYPE_LINEAR;
        m_Builder->m_StartX       = sx;
        m_Builder->m_StartY       = sy;
        m_Builder->m_EndX         = ex;
        m_Builder->m_EndY         = ey;
    }

    void SharedRenderPaint::radialGradient(float sx, float sy, float ex, float ey)
    {
        m_Builder                 = new SharedRenderPaintBuilder();
        m_Builder->m_GradientType = FILL_TYPE_RADIAL;
        m_Builder->m_StartX       = sx;
        m_Builder->m_StartY       = sy;
        m_Builder->m_EndX         = ex;
        m_Builder->m_EndY         = ey;
    }

    void SharedRenderPaint::addStop(unsigned int color, float stop)
    {
        if (m_Builder->m_Stops.Size() == m_Builder->m_Stops.Capacity())
        {
            m_Builder->m_Stops.SetCapacity(m_Builder->m_Stops.Size() + 1);
        }

        m_Builder->m_Stops.Push({
            .m_Color = color,
            .m_Stop  = stop,
        });
    }

    void SharedRenderPaint::completeGradient()
    {
        m_Data             = {};
        m_Data.m_FillType  = m_Builder->m_GradientType;
        m_Data.m_StopCount = m_Builder->m_Stops.Size();

        m_Data.m_GradientLimits[0] = m_Builder->m_StartX;
        m_Data.m_GradientLimits[1] = m_Builder->m_StartY;
        m_Data.m_GradientLimits[2] = m_Builder->m_EndX;
        m_Data.m_GradientLimits[3] = m_Builder->m_EndY;

        m_IsVisible = false;
        assert(m_Data.m_StopCount < PaintData::MAX_STOPS);
        for (int i = 0; i < (int) m_Builder->m_Stops.Size(); ++i)
        {
            const GradientStop& stop = m_Builder->m_Stops[i];
            getColorArrayFromUint(stop.m_Color, &m_Data.m_Colors[i * 4]);
            m_Data.m_Stops[i] = stop.m_Stop;

            if (m_Data.m_Colors[i*4 + 3] > 0.0f)
            {
                m_IsVisible = true;
            }
        }

        m_Builder->m_Stops.SetSize(0);
        m_Builder->m_Stops.SetCapacity(0);
        delete m_Builder;
    }

    /* Shared render path */
    SharedRenderPath::SharedRenderPath(Context* ctx)
    : m_Context(ctx)
    {}

    /* Shared Renderer */
    SharedRenderer::SharedRenderer()
    : m_IndexBuffer(0)
    {
        m_Indices.emplace_back(0);
        m_Indices.emplace_back(1);
        m_Indices.emplace_back(2);
        m_Indices.emplace_back(2);
        m_Indices.emplace_back(3);
        m_Indices.emplace_back(0);
    }

    SharedRenderer::~SharedRenderer()
    {
        m_Context->m_DestroyBufferCb(m_IndexBuffer, m_Context->m_BufferCbUserData);
    }

    void SharedRenderer::updateIndexBuffer(size_t contourLength)
    {
        if (contourLength < 2)
        {
            return;
        }
        uint32_t edgeCount       = (m_Indices.size() - 6) / 3;
        uint32_t targetEdgeCount = contourLength - 2;
        if (edgeCount < targetEdgeCount)
        {
            while (edgeCount < targetEdgeCount)
            {
                m_Indices.push_back(3);
                m_Indices.push_back(edgeCount + 4);
                m_Indices.push_back(edgeCount + 5);
                edgeCount++;
            }

            m_IndexBuffer = m_Context->m_RequestBufferCb(m_IndexBuffer,
                BUFFER_TYPE_INDEX_BUFFER, &m_Indices[0], m_Indices.size() * sizeof(int),
                m_Context->m_BufferCbUserData);
        }
    }

    void SharedRenderer::clipPath(RenderPath* path)
    {
        if (m_ClipPaths.Full())
        {
            m_ClipPaths.SetCapacity(m_ClipPaths.Capacity() + 1);
        }

        m_ClipPaths.Push({.m_Path = path, .m_Transform = m_Transform});
        m_IsClippingDirty = true;
    }

    void SharedRenderer::transform(const Mat2D& transform)
    {
        Mat2D::multiply(m_Transform, m_Transform, transform);
    }

    void SharedRenderer::save()
    {
        StackEntry entry = {};
        entry.m_Transform = Mat2D(m_Transform);

        assert(m_ClipPaths.Size() < STACK_ENTRY_MAX_CLIP_PATHS);

        entry.m_ClipPathsCount = m_ClipPaths.Size();
        memcpy(entry.m_ClipPaths, m_ClipPaths.Begin(), m_ClipPaths.Size() * sizeof(PathDescriptor));

        if (m_ClipPathStack.Full())
        {
            m_ClipPathStack.SetCapacity(m_ClipPathStack.Capacity() + 1);
        }

        m_ClipPathStack.Push(entry);
    }

    void SharedRenderer::restore()
    {
        const StackEntry last = m_ClipPathStack.Pop();
        m_Transform = last.m_Transform;
        m_ClipPaths.SetSize(0);
        m_ClipPaths.SetCapacity(last.m_ClipPathsCount);
        m_IsClippingDirty = true;

        for (int i = 0; i < last.m_ClipPathsCount; ++i)
        {
            m_ClipPaths.Push(last.m_ClipPaths[i]);
        }
    }

    void SharedRenderer::pushDrawEvent(PathDrawEvent event)
    {
        if (m_DrawEvents.Full())
        {
            m_DrawEvents.SetCapacity(m_DrawEvents.Capacity() + 1);
        }
        m_DrawEvents.Push(event);
    }

    void SharedRenderer::setPaint(SharedRenderPaint* rp)
    {
        if (m_RenderPaint != rp)
        {
            m_RenderPaint = rp;

            PathDrawEvent evt = {
                .m_Type  = EVENT_SET_PAINT,
                .m_Paint = rp,
            };

            pushDrawEvent(evt);
        }
    }

    /* Misc functions */
    float getContourError(HRenderer renderer)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        const float maxContourError = 5.0f;
        const float minContourError = 0.5f;
        return minContourError * r->m_ContourQuality + maxContourError * (1.0f - r->m_ContourQuality);
    }

    bool getClippingSupport(HRenderer renderer)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        return r->m_IsClippingSupported;
    }

    void setBufferCallbacks(HContext ctx, RequestBufferCb rcb, DestroyBufferCb dcb, void* userData)
    {
        Context* c            = (Context*) ctx;
        c->m_RequestBufferCb  = rcb;
        c->m_DestroyBufferCb  = dcb;
        c->m_BufferCbUserData = userData;
    }

    void setRenderMode(HContext ctx, RenderMode mode)
    {
        Context* c      = (Context*) ctx;
        c->m_RenderMode = mode;
    }

    const PaintData getPaintData(HRenderPaint paint)
    {
        SharedRenderPaint* pd = (SharedRenderPaint*) paint;
        return pd->m_Data;
    }

    void setContourQuality(HRenderer renderer, float quality)
    {
        SharedRenderer* r   = (SharedRenderer*) renderer;
        r->m_ContourQuality = quality;
    }

    void setClippingSupport(HRenderer renderer, bool state)
    {
        SharedRenderer* r        = (SharedRenderer*) renderer;
        r->m_IsClippingSupported = state;
    }

    void setTransform(HRenderer renderer, const Mat2D& transform)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        r->m_Transform = transform;
    }

    RenderMode getRenderMode(HContext ctx)
    {
        Context* c = (Context*) ctx;
        return c->m_RenderMode;
    }

    RenderPaint* createRenderPaint(HContext ctx)
    {
        return new SharedRenderPaint;
    }

    RenderPath* createRenderPath(HContext ctx)
    {
        Context* c = (Context*) ctx;
        switch(c->m_RenderMode)
        {
            case MODE_TESSELLATION:     return new TessellationRenderPath(c);
            case MODE_STENCIL_TO_COVER: return new StencilToCoverRenderPath(c);
            default:break;
        }
        return 0;
    }

    HRenderer createRenderer(HContext ctx)
    {
        Context* c = (Context*) ctx;
        switch(c->m_RenderMode)
        {
            case MODE_TESSELLATION:     return (HRenderer) new TessellationRenderer(c);
            case MODE_STENCIL_TO_COVER: return (HRenderer) new StencilToCoverRenderer(c);
            default:break;
        }
        return 0;
    }

    void destroyRenderer(HRenderer renderer)
    {
        assert(renderer);
        delete renderer;
    }

    void newFrame(HRenderer renderer)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        r->m_AppliedClips.SetSize(0);
        r->m_DrawEvents.SetSize(0);
        r->m_IsClippingDirty = false;
        r->m_RenderPaint = 0;
        r->m_IsClipping = false;
    }

    uint32_t getDrawEventCount(HRenderer renderer)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        return r->m_DrawEvents.Size();
    }

    const PathDrawEvent getDrawEvent(HRenderer renderer, uint32_t i)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        return r->m_DrawEvents[i];
    }

    const DrawBuffers getDrawBuffers(HContext ctx, HRenderer renderer, HRenderPath path)
    {
        DrawBuffers buffers  = {};
        SharedRenderer* r    = (SharedRenderer*) renderer;
        Context* c           = (Context*) ctx;

        if (c->m_RenderMode == MODE_STENCIL_TO_COVER)
        {
            StencilToCoverRenderPath* p = (StencilToCoverRenderPath*) path;
            if (r)
            {
                buffers.m_IndexBuffer = r->m_IndexBuffer;
            }
            if (p)
            {
                buffers.m_VertexBuffer = p->m_VertexBuffer;
            }
        }
        else if (c->m_RenderMode == MODE_TESSELLATION)
        {
            TessellationRenderPath* p = (TessellationRenderPath*) path;
            if (p)
            {
                buffers.m_IndexBuffer     = p->m_IndexBuffer;
                buffers.m_VertexBuffer    = p->m_VertexBuffer;
            }
        }

        return buffers;
    }

    void destroyContext(HContext ctx)
    {
        assert(ctx);
        delete (Context*) ctx;
    }

    HContext createContext()
    {
        Context* ctx = new Context;
        return (HContext) ctx;
    }


    void resetClipping(HRenderer renderer)
    {
        SharedRenderer* r = (SharedRenderer*) renderer;
        r->m_IsClippingDirty = true;
        r->m_IsClipping = false;
        r->m_ClipPaths.SetSize(0);
        r->m_AppliedClips.SetSize(0);
    }
}
