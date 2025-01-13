#ifndef _RIVE_SCROLL_CONSTRAINT_HPP_
#define _RIVE_SCROLL_CONSTRAINT_HPP_
#include "rive/generated/constraints/scrolling/scroll_constraint_base.hpp"
#include "rive/advance_flags.hpp"
#include "rive/advancing_component.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/transform_components.hpp"
#include <stdio.h>
namespace rive
{

class ScrollConstraint : public ScrollConstraintBase,
                         public AdvancingComponent,
                         public LayoutConstraint
{
private:
    TransformComponents m_componentsA;
    TransformComponents m_componentsB;
    float m_offsetX = 0;
    float m_offsetY = 0;
    ScrollPhysics* m_physics;
    Mat2D m_scrollTransform;

public:
    void constrain(TransformComponent* component) override;
    std::vector<DraggableProxy*> draggables() override;
    void buildDependencies() override;
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    void dragView(Vec2D delta);
    void runPhysics();
    void constrainChild(LayoutComponent* component) override;
    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;

    ScrollPhysics* physics() const { return m_physics; }
    void physics(ScrollPhysics* physics) { m_physics = physics; }
    void initPhysics();

    ScrollPhysicsType physicsType() const
    {
        return ScrollPhysicsType(physicsTypeValue());
    }

    LayoutComponent* content() { return parent()->as<LayoutComponent>(); }
    LayoutComponent* viewport()
    {
        return parent()->parent()->as<LayoutComponent>();
    }
    float contentWidth() { return content()->layoutWidth(); }
    float contentHeight() { return content()->layoutHeight(); }
    float viewportWidth()
    {
        return direction() == DraggableConstraintDirection::vertical
                   ? viewport()->layoutWidth()
                   : std::max(0.0f,
                              viewport()->layoutWidth() - content()->layoutX());
    }
    float viewportHeight()
    {
        return direction() == DraggableConstraintDirection::horizontal
                   ? viewport()->layoutHeight()
                   : std::max(0.0f,
                              viewport()->layoutHeight() -
                                  content()->layoutY());
    }
    float visibleWidthRatio()
    {
        if (contentWidth() == 0)
        {
            return 1;
        }
        return std::min(1.0f, viewportWidth() / contentWidth());
    }
    float visibleHeightRatio()
    {
        if (contentHeight() == 0)
        {
            return 1;
        }
        return std::min(1.0f, viewportHeight() / contentHeight());
    }
    float maxOffsetX()
    {
        return std::min(0.0f,
                        viewportWidth() - contentWidth() -
                            viewport()->paddingRight());
    }
    float maxOffsetY()
    {
        return std::min(0.0f,
                        viewportHeight() - contentHeight() -
                            viewport()->paddingBottom());
    }
    float clampedOffsetX()
    {
        if (maxOffsetX() > 0)
        {
            return 0;
        }
        if (m_physics != nullptr && m_physics->enabled())
        {
            return m_physics
                ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                        Vec2D(m_offsetX, m_offsetY))
                .x;
        }
        return math::clamp(m_offsetX, maxOffsetX(), 0);
    }
    float clampedOffsetY()
    {
        if (maxOffsetY() > 0)
        {
            return 0;
        }
        if (m_physics != nullptr && m_physics->enabled())
        {
            return m_physics
                ->clamp(Vec2D(maxOffsetX(), maxOffsetY()),
                        Vec2D(m_offsetX, m_offsetY))
                .y;
        }
        return math::clamp(m_offsetY, maxOffsetY(), 0);
    }

    float offsetX() { return m_offsetX; }
    float offsetY() { return m_offsetY; }
    void offsetX(float value)
    {
        if (m_offsetX == value)
        {
            return;
        }
        m_offsetX = value;
        content()->markWorldTransformDirty();
    }
    void offsetY(float value)
    {
        if (m_offsetY == value)
        {
            return;
        }
        m_offsetY = value;
        content()->markWorldTransformDirty();
    }
};
} // namespace rive

#endif