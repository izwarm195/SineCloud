/*
  ==============================================================================
    BossEntity.h
    Layer 3: Game / Interaction
    Õ¼Î»¹Ç¼Ü¡£°´·½°¸£¬BossEntity ³ÖÓÐÒ»¸öÄ¿±ê KnobEntity*£¨»ò paramId£©+ HP +
    ¹¥»÷Ä£Ê½£»Íæ¼Ò¹¥»÷ÃüÖÐÊ±Í¨¹ý onHit() ´¥·¢¶Ô²ÎÊýµÄÈÅ¶¯¡£
    ±¾½×¶Î²»ÊµÀý»¯½ø World£¬½ö±£Áô½Ó¿Ú¡£
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

        /** Íæ¼Ò¹¥»÷ÃüÖÐ£º°ÑÉËº¦×ª»¯Îª¶ÔÄ¿±ê²ÎÊýµÄ"ÌßÒ»ÏÂ"¡£ */
        virtual void onHit(float /*damage*/) {}

        // TODO: HP / phase / attack patterns
        float hp{ 100.0f };

    protected:
        KnobEntity* targetKnob{ nullptr };
    };
}
