#pragma once

#include <JuceHeader.h>
#include "Vec3.h"
#include "SceneCamera.h"

//==============================================================================
class SceneObject
{
public:
    SceneObject() = default;
    virtual ~SceneObject() = default;

    void setWorldPos(Vec3 p) { worldPos = p; }
    Vec3 getWorldPos() const { return worldPos; }

    virtual void updateScreenPosition(const SceneCamera& cam) = 0;

    virtual void drawSelf(juce::Graphics& g) { juce::ignoreUnused(g); }

    float getDepthKey() const { return worldPos.y; }

    virtual bool  isInteractable()    const { return true; }
    virtual float getInteractRadius() const { return 60.0f; }

protected:
    Vec3 worldPos{ 0.0f, 0.0f, 0.0f };
};
