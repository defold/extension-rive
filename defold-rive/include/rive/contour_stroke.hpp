#ifndef _RIVE_CONTOUR_STROKE_HPP_
#define _RIVE_CONTOUR_STROKE_HPP_

#include "rive/renderer.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include <vector>
#include <cstdint>

namespace rive
{
	class ContourRenderPath;

	///
	/// Builds a triangle strip vertex buffer from a ContourRenderPath.
	///
	class ContourStroke
	{
	protected:
		std::vector<Vec2D> m_TriangleStrip;
		std::vector<std::size_t> m_Offsets;
		uint32_t m_RenderOffset = 0;

	public:
		const std::vector<Vec2D>& triangleStrip() const
		{
			return m_TriangleStrip;
		}

		void reset();
		void resetRenderOffset();
		void nextRenderOffset(std::size_t& start, std::size_t& end);

		void extrude(const ContourRenderPath* renderPath,
		             bool isClosed,
		             StrokeJoin join,
		             StrokeCap cap,
		             float strokeWidth,
		             const Mat2D& transform);
	};
} // namespace rive
#endif