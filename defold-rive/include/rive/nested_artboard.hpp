#ifndef _RIVE_NESTED_ARTBOARD_HPP_
#define _RIVE_NESTED_ARTBOARD_HPP_

#include "rive/generated/nested_artboard_base.hpp"
#include "rive/artboard_host.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"
#include "rive/hit_info.hpp"
#include "rive/span.hpp"
#include "rive/advancing_component.hpp"
#include <stdio.h>

namespace rive
{

class ArtboardInstance;
class NestedAnimation;
class NestedInput;
class NestedStateMachine;
class StateMachineInstance;
class NestedArtboard : public NestedArtboardBase,
                       public AdvancingComponent,
                       public ArtboardHost
{
protected:
    Artboard* m_Artboard = nullptr; // might point to m_Instance, and might not
    std::unique_ptr<ArtboardInstance> m_Instance; // may be null
    std::vector<NestedAnimation*> m_NestedAnimations;

protected:
    std::vector<uint32_t> m_DataBindPathIdsBuffer;

public:
    NestedArtboard();
    ~NestedArtboard() override;
    StatusCode onAddedClean(CoreContext* context) override;
    void draw(Renderer* renderer) override;
    Core* hitTest(HitInfo*, const Mat2D&) override;
    void addNestedAnimation(NestedAnimation* nestedAnimation);

    void nest(Artboard* artboard);
    size_t artboardCount() override { return 1; }
    ArtboardInstance* artboardInstance(int index = 0) override
    {
        return m_Instance.get();
    }
    Artboard* sourceArtboard() { return m_Artboard; }

    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    void update(ComponentDirt value) override;

    bool hasNestedStateMachines() const;
    Span<NestedAnimation*> nestedAnimations();
    NestedArtboard* nestedArtboard(std::string name) const;
    NestedStateMachine* stateMachine(std::string name) const;
    NestedInput* input(std::string name) const;
    NestedInput* input(std::string name, std::string stateMachineName) const;

    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;

    /// Convert a world space (relative to the artboard that this
    /// NestedArtboard is a child of) to the local space of the Artboard
    /// nested within. Returns true when the conversion succeeds, and false
    /// when one is not possible.
    bool worldToLocal(Vec2D world, Vec2D* local);
    void decodeDataBindPathIds(Span<const uint8_t> value) override;
    void copyDataBindPathIds(const NestedArtboardBase& object) override;
    std::vector<uint32_t> dataBindPathIds() override
    {
        return m_DataBindPathIdsBuffer;
    };
    void populateDataBinds(std::vector<DataBind*>* dataBinds) override;
    void bindViewModelInstance(rcp<ViewModelInstance> viewModelInstance,
                               DataContext* parent) override;
    void internalDataContext(DataContext* dataContext) override;
    void clearDataContext() override;

    bool advanceComponent(float elapsedSeconds,
                          AdvanceFlags flags = AdvanceFlags::Animate |
                                               AdvanceFlags::NewFrame) override;
    Artboard* parentArtboard() override { return artboard(); }
    void markHostTransformDirty() override { markTransformDirty(); }
};
} // namespace rive

#endif
