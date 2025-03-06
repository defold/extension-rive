#ifndef _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/text/text_target_modifier.hpp"
namespace rive
{
class TextFollowPathModifierBase : public TextTargetModifier
{
protected:
    typedef TextTargetModifier Super;

public:
    static const uint16_t typeKey = 547;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextFollowPathModifierBase::typeKey:
            case TextTargetModifierBase::typeKey:
            case TextModifierBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    static const uint16_t radialPropertyKey = 779;

protected:
    bool m_Radial = false;

public:
    inline bool radial() const { return m_Radial; }
    void radial(bool value)
    {
        if (m_Radial == value)
        {
            return;
        }
        m_Radial = value;
        radialChanged();
    }

    Core* clone() const override;
    void copy(const TextFollowPathModifierBase& object)
    {
        m_Radial = object.m_Radial;
        TextTargetModifier::copy(object);
    }

    bool deserialize(uint16_t propertyKey, BinaryReader& reader) override
    {
        switch (propertyKey)
        {
            case radialPropertyKey:
                m_Radial = CoreBoolType::deserialize(reader);
                return true;
        }
        return TextTargetModifier::deserialize(propertyKey, reader);
    }

protected:
    virtual void radialChanged() {}
};
} // namespace rive

#endif