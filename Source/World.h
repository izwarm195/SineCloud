/*
  ==============================================================================
    World.h
    Layer 3: Game / Interaction
    ³¡¾°ÈÝÆ÷£ºÓµÓÐ¹²Ïí mesh¡¢Player¡¢Èô¸É KnobEntity¡£
    Ã¿Ö¡£º
      1. °ÑÏà»úË®Æ½Ãæ basis ×¢Èë Player£»
      2. update ËùÓÐ entity£»
      3. ¼ÆËã Player Óë¸÷ Knob µÄ¾àÀë -> Focused ×´Ì¬£»
      4. °Ñ pivot Èí¸úËæµ½ Player£»
      5. draw ËùÓÐ entity¡£
    Êó±ê½»»¥£º
      - mousePress(ray) -> ¶Ô KnobEntity ×öÊ°È¡£¬ÈôÃüÖÐÇÒ Focused£¬¿ªÊ¼ÍÏ×§
      - mouseDrag(deltaPx) -> ÍÆ InertialValue
      - mouseRelease() -> ½áÊøÍÏ×§
  ==============================================================================*/
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

#include "Entity.h"
#include "Player.h"
#include "KnobEntity.h"
#include "InputState.h"
#include "Easing.h"
#include "PluginProcessor.h"

namespace sc
{
    class World
    {
    public:
        explicit World(SineCloudAudioProcessor& p);

        // GL ×ÊÔ´ÉúÃüÖÜÆÚ£¨Óë SceneView µÄ newOpenGLContextCreated/Closing ¶ÔÆë£©
        void uploadMeshes(juce::OpenGLContext& ctx);
        void releaseMeshes(juce::OpenGLContext& ctx);

        // Ö÷Ñ­»·
        void update(float dt, const InputState& in, Camera& cam);
        void draw(Renderer& r, const Camera& cam);

        // Êó±ê
        bool onMousePress(const Ray& worldRay);
        void onMouseDragDelta(juce::Point<float> deltaPx);
        void onMouseRelease();

        // 交互
        KnobEntity* getFocusedKnob() const noexcept { return focusedKnob; }
        void onMouseWheel(float deltaY);

        // 私有成员区（加在 draggingKnob 旁边）
        KnobEntity* focusedKnob{ nullptr };   // ← 当前玩家临近的旋钮


        // µ÷ÊÔ / ÐÅÏ¢
        Player& getPlayer() noexcept { return *player; }
        const std::vector<std::unique_ptr<KnobEntity>>& getKnobs() const noexcept { return knobs; }

    private:
        void buildKnobs();

        SineCloudAudioProcessor& processor;

        // ¹²Ïí mesh
        std::unique_ptr<Mesh> groundMesh;   // GL_LINES
        std::unique_ptr<Mesh> boxMesh;      // 1x1x1
        std::unique_ptr<Mesh> cylMesh;      // °ë¾¶ 1, ¸ß 1
        std::unique_ptr<Mesh> ptrMesh;      // 1x1x1£¨ÓÃ×÷Ö¸ÕëÌõ£¬¿¿ scale À­³É³¤Ìõ£©

        // ÊµÌå
        std::unique_ptr<Player> player;
        std::vector<std::unique_ptr<KnobEntity>> knobs;

        // ÍÏ×§ÖÐµÄÐýÅ¥
        KnobEntity* draggingKnob{ nullptr };

        // µ÷²Î
        float interactReach{ 1.6f }; // Íæ¼Òµ½ÐýÅ¥ÖÐÐÄµÄ"focus"¾àÀë
        float pivotFollowRate{ 0.9999f };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(World)
    };
}
