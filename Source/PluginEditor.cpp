#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "InertialSlider.h"

namespace
{
    // 12 ¸öÒôÃû£¬Óë Csound gSNoteNames ¶ÔÆë
    static const juce::StringArray kNoteNames{
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
}

//==============================================================================

SineCloudAudioProcessorEditor::SineCloudAudioProcessorEditor(SineCloudAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1180, 500);

    dreamBox = { 30,  50, 440, 400 };
    adsrBox = { 490, 50, 200, 400 };
    spaceBox = { 710, 50, 440, 400 };

    // ---- Èý¸ö group label ----
    styleGroupLabel(dreamGroupLabel, "DREAM");
    styleGroupLabel(adsrGroupLabel, "ADSR");
    styleGroupLabel(spaceGroupLabel, "SPACE");
    addAndMakeVisible(dreamGroupLabel);
    addAndMakeVisible(adsrGroupLabel);
    addAndMakeVisible(spaceGroupLabel);

    // ---- DREAM ¿ò 6 ¸öÐýÅ¥ ----
    setupKnob(dreamSlider, dreamLabel, "Dream", "", true);
    setupKnob(pitchSlider, pitchLabel, "Pitch", "", true);
    setupKnob(floatSlider, floatLabel, "Float", " ms", true);
    setupKnob(shimmerSlider, shimmerLabel, "Shimmer", " st", true);
    setupKnob(densitySlider, densityLabel, "Density", "", true);
    setupKnob(gainSlider, gainLabel, "Gain", "", true);

    // ---- ADSR ¿ò 4 ¸öÏ¸ÐýÅ¥£¨²»ÏÔÊ¾ Slider ×Ô´ø label£¬ÓÃÓÒ²à label Ìæ´ú£© ----
    auto setupAdsr = [this](InertialSlider& s, juce::Label& lbl, const juce::String& name)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setMouseDragSensitivity(400);   // Ä¬ÈÏ 250£¬Ô½´óÔ½²»ÁéÃô
            s.setVelocityBasedMode(false);    // ¹ØµôËÙ¶È¸ÐÓ¦£¬´¿ÏßÐÔ¸üÎÈ

            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
            s.setTextValueSuffix(" ms");
            s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);
            s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(60, 60, 60));
            s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            addAndMakeVisible(s);

            lbl.setText(name, juce::dontSendNotification);
            lbl.setJustificationType(juce::Justification::centredLeft);
            lbl.setColour(juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible(lbl);
        };
    setupAdsr(attackSlider, attackLabel, "Attack");
    setupAdsr(decaySlider, decayLabel, "Decay");
    setupAdsr(sustainSlider, sustainLabel, "Sustain");
    setupAdsr(releaseSlider, releaseLabel, "Release");

    // ---- SPACE ¿ò 5 ¸öÐýÅ¥ ----
   
    setupKnob(dlyTimeSlider, dlyTimeLabel, "Dly Time", " ms", true);
    setupKnob(dlyFbSlider, dlyFbLabel, "Dly Fb", "", true);
    setupKnob(dlyMixSlider, dlyMixLabel, "Dly Mix", "", true);
    setupKnob(revMixSlider, revMixLabel, "Rev Mix", "", true);
    setupKnob(revSizeSlider, revSizeLabel, "Rev Size", "", true);

    // ---- Root ÏÔÊ¾ ----
    rootDisplay.setText("ROOT: C", juce::dontSendNotification);
    rootDisplay.setJustificationType(juce::Justification::centred);
    rootDisplay.setColour(juce::Label::textColourId, juce::Colours::white);
    rootDisplay.setFont(juce::Font(16.0f));
    addAndMakeVisible(rootDisplay);

    // ---- Attachments ----
    auto& s = audioProcessor.apvts;
    dreamA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DREAM, dreamSlider);
    pitchA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_PITCH, pitchSlider);
    floatA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_FLOAT, floatSlider);
    shimmerA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_SHIMMER, shimmerSlider);
    densityA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DENSITY, densitySlider);
    gainA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_GAIN, gainSlider);

    attackA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_ATTACK, attackSlider);
    decayA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DECAY, decaySlider);
    sustainA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_SUSTAIN, sustainSlider);
    releaseA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_RELEASE, releaseSlider);

    dlyTimeA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DLY_TIME, dlyTimeSlider);
    dlyFbA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DLY_FB, dlyFbSlider);
    dlyMixA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_DLY_MIX, dlyMixSlider);
    revMixA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_REV_MIX, revMixSlider);
    revSizeA = std::make_unique<SA>(s, SineCloudAudioProcessor::PARAM_REV_SIZE, revSizeSlider);

    startTimerHz(15);  // Ë¢ÐÂ Root ÏÔÊ¾
    if (kUseSceneDemo)
    {
        sceneDemo = std::make_unique<IsoSceneDemo>(audioProcessor);
        addAndMakeVisible(*sceneDemo);
        setSize(1180, 700);
    }

}

SineCloudAudioProcessorEditor::~SineCloudAudioProcessorEditor() = default;

//==============================================================================
void SineCloudAudioProcessorEditor::setupKnob(InertialSlider& s, juce::Label& lbl,
    const juce::String& name,
    const juce::String& suffix,
    bool valueBox)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    if (valueBox)
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    else
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setTextValueSuffix(suffix);

    s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(60, 60, 60));
    s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible(s);

    lbl.setText(name, juce::dontSendNotification);
    lbl.setJustificationType(juce::Justification::centred);
    lbl.setColour(juce::Label::textColourId, juce::Colours::white);
    lbl.attachToComponent(&s, false);
    addAndMakeVisible(lbl);
}

void SineCloudAudioProcessorEditor::styleGroupLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centredTop);
    l.setColour(juce::Label::textColourId, juce::Colours::white);
    l.setFont(juce::Font(14.0f, juce::Font::bold));
}

//==============================================================================
void SineCloudAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    // Èý¸ö group box µÄ±ß¿ò
    g.setColour(juce::Colour::fromRGB(80, 80, 80));
    g.drawRect(dreamBox, 1);
    g.drawRect(adsrBox, 1);
    g.drawRect(spaceBox, 1);
}

void SineCloudAudioProcessorEditor::resized()
{
    if (sceneDemo != nullptr)
    {
        sceneDemo->setBounds(getLocalBounds());
        return;
    }

    // ---- Group ±êÌâÎ»ÖÃ£¨Ã¿¸ö box ¶¥²¿¾ÓÖÐ£© ----
    dreamGroupLabel.setBounds(dreamBox.getX(), dreamBox.getY() + 4, dreamBox.getWidth(), 18);
    adsrGroupLabel.setBounds(adsrBox.getX(), adsrBox.getY() + 4, adsrBox.getWidth(), 18);
    spaceGroupLabel.setBounds(spaceBox.getX(), spaceBox.getY() + 4, spaceBox.getWidth(), 18);

    // ============================================================
    // DREAM ¿ò£ºÖÐÐÄ Dream(160) + Îå±ßÐÎ 5 ¸öÐýÅ¥(90)
    // ============================================================
    dreamSlider.setBounds(170, 180, 160, 160);
    pitchSlider.setBounds(205, 85, 90, 90);   // ¶¥
    floatSlider.setBounds(81, 175, 90, 90);   // ×ó
    shimmerSlider.setBounds(329, 175, 90, 90);   // ÓÒ
    densitySlider.setBounds(129, 320, 90, 90);   // ×óÏÂ
    gainSlider.setBounds(281, 320, 90, 90);   // ÓÒÏÂ

    // ============================================================
    // ADSR ¿ò£º4 ¸öÏ¸ÐýÅ¥ÊúÅÅ + ÓÒ²à label
    // ============================================================
    attackSlider.setBounds(530, 75, 50, 90);
    decaySlider.setBounds(530, 153, 50, 90);
    sustainSlider.setBounds(530, 232, 50, 90);
    releaseSlider.setBounds(530, 310, 50, 90);

    attackLabel.setBounds(590, 110, 75, 18);
    decayLabel.setBounds(590, 188, 75, 18);
    sustainLabel.setBounds(590, 267, 75, 18);
    releaseLabel.setBounds(590, 345, 75, 18);

    // ============================================================
    // SPACE ¿ò
    // ============================================================
    dlyTimeSlider.setBounds(885, 85, 90, 90);
    dlyFbSlider.setBounds(764, 320, 90, 90);
    dlyMixSlider.setBounds(1006, 320, 90, 90);
    revMixSlider.setBounds(890, 190, 80, 70);
    revSizeSlider.setBounds(890, 275, 80, 70);

    // ============================================================
    // µ×²¿ Root ÏÔÊ¾
    // ============================================================
    rootDisplay.setBounds(20, 460, 1140, 30);
}

//==============================================================================
void SineCloudAudioProcessorEditor::timerCallback()
{
    const int r = audioProcessor.getCurrentRoot();
    if (r >= 0 && r < 12)
        rootDisplay.setText("ROOT: " + kNoteNames[r], juce::dontSendNotification);
}