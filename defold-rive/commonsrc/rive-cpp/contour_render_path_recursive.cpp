#if defined(LOW_LEVEL_RENDERING) && defined(CONTOUR_RECURSIVE)

#include <assert.h>
#include "contour_render_path.hpp"
#include "math/cubic_utilities.hpp"

using namespace rive;

// TODO when we add strokes, add ranges in the contour that need to be stroked
// as contiguous lines.

// struct StrokeRange
// {
// 	unsigned int start;
// 	unsigned int end;
// };

class RecursiveCubicSegmenter
{
private:
	Vec2D m_Pen, m_PenDown;
	bool m_IsPenDown = false;
	std::vector<Vec2D>* m_Contour;
	// std::vector<StrokeRange> m_StrokeRanges;

	AABB m_Bounds;
	float m_Threshold, m_ThresholdSquared;

public:
	RecursiveCubicSegmenter(std::vector<Vec2D>* contour, float threshold) :
	    m_Contour(contour),
	    m_Bounds(AABB::forExpansion()),
	    m_Threshold(threshold),
	    m_ThresholdSquared(threshold * threshold)
	{
	}

	const Vec2D& pen() { return m_Pen; }
	bool isPenDown() { return m_IsPenDown; }

	void addVertex(const Vec2D& vertex)
	{
		m_Contour->emplace_back(vertex);
		AABB::expandTo(m_Bounds, vertex);
	}

	const AABB& bounds() const { return m_Bounds; }

	inline void penUp()
	{
		if (!m_IsPenDown)
		{
			return;
		}
		m_IsPenDown = false;
	}

	inline void penDown()
	{
		if (m_IsPenDown)
		{
			return;
		}
		m_IsPenDown = true;
		Vec2D::copy(m_PenDown, m_Pen);
		addVertex(m_PenDown);
	}

	inline void close()
	{
		if (!m_IsPenDown)
		{
			return;
		}
		Vec2D::copy(m_Pen, m_PenDown);
		m_IsPenDown = false;

		// TODO: Can we optimize and not dupe this point if it's the last point
		// already in the list? For example: a procedural triangle closes itself
		// with a lineTo the first point.
		addVertex(m_PenDown);
	}

	inline void pen(const Vec2D& position) { Vec2D::copy(m_Pen, position); }

	void segmentCubic(const Vec2D& from,
	                  const Vec2D& fromOut,
	                  const Vec2D& toIn,
	                  const Vec2D& to,
	                  float t1,
	                  float t2)
	{
		if (CubicUtilities::shouldSplitCubic(
		        from, fromOut, toIn, to, m_Threshold))
		{
			float halfT = (t1 + t2) / 2.0f;

			Vec2D hull[6];
			CubicUtilities::computeHull(from, fromOut, toIn, to, 0.5f, hull);

			segmentCubic(from, hull[0], hull[3], hull[5], t1, halfT);

			segmentCubic(hull[5], hull[4], hull[2], to, halfT, t2);
		}
		else
		{
			if (Vec2D::distanceSquared(from, to) > m_ThresholdSquared)
			{
				addVertex(Vec2D(CubicUtilities::cubicAt(
				                    t2, from[0], fromOut[0], toIn[0], to[0]),
				                CubicUtilities::cubicAt(
				                    t2, from[1], fromOut[1], toIn[1], to[1])));
			}
		}
	}
};

void ContourRenderPath::computeContour()
{
	m_IsDirty = false;
	assert(m_ContourVertices.empty());
	RecursiveCubicSegmenter segmenter(&m_ContourVertices, m_ContourThreshold);

	// First four vertices are the bounds.
	m_ContourVertices.emplace_back(Vec2D());
	m_ContourVertices.emplace_back(Vec2D());
	m_ContourVertices.emplace_back(Vec2D());
	m_ContourVertices.emplace_back(Vec2D());

	for (rive::PathCommand& command : m_Commands)
	{
		switch (command.type())
		{
			case PathCommandType::move:
				segmenter.penUp();
				segmenter.pen(command.point());
				break;
			case PathCommandType::line:
				segmenter.penDown();
				segmenter.pen(command.point());
				segmenter.addVertex(command.point());
				break;
			case PathCommandType::cubic:
				segmenter.penDown();
				segmenter.segmentCubic(segmenter.pen(),
				                       command.outPoint(),
				                       command.inPoint(),
				                       command.point(),
				                       0.0f,
				                       1.0f);
				// segmenter.addVertex(command.point());
				segmenter.pen(command.point());
				break;
			case PathCommandType::close:
				segmenter.close();
				break;
		}
	}
	// TODO: when we stroke we may want to differentiate whether or not the path
	// actually closed.
	segmenter.close();

	// TODO: consider if there's a case with no points.
	AABB::copy(m_ContourBounds, segmenter.bounds());
	Vec2D& first = m_ContourVertices[0];
	first[0] = m_ContourBounds.minX;
	first[1] = m_ContourBounds.minY;

	Vec2D& second = m_ContourVertices[1];
	second[0] = m_ContourBounds.maxX;
	second[1] = m_ContourBounds.minY;

	Vec2D& third = m_ContourVertices[2];
	third[0] = m_ContourBounds.maxX;
	third[1] = m_ContourBounds.maxY;

	Vec2D& fourth = m_ContourVertices[3];
	fourth[0] = m_ContourBounds.minX;
	fourth[1] = m_ContourBounds.maxY;
}
#endif