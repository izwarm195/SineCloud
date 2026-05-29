/*
  ==============================================================================
    BossEntity.h
    Layer 3: Game / Interaction
    占位骨架。按方案，BossEntity 持有一个目标 KnobEntity*（或 paramId）+ HP +
    攻击模式；玩家攻击命中时通过 onHit() 触发对参数的扰动。
    本阶段不实例化进 World，仅保留接口。
  ==============================================================================*/
#pragma once

#include "Entity.h"
#include "KnobEntity.h"

namespace sc
{
    class BossEntity : public Entity
    {
    public:
        explicit BossEntity(KnobEntity* target = nullptr) : targetKnob(target) {}

        void setTargetKnob(KnobEntity* k) noexcept { targetKnob = k; }

        /** 玩家攻击命中：把伤害转化为对目标参数的"踢一下"。 */
        virtual void onHit(float /*damage*/) {}

        // TODO: HP / phase / attack patterns
        float hp{ 100.0f };

    protected:
        KnobEntity* targetKnob{ nullptr };
    };
}
