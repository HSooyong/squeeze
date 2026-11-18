#pragma once

#include "EditorLayout.h"

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace squeeze::ui
{

struct SqueezeBoxes
{
    juce::Rectangle<int> header;
    juce::Rectangle<int> logo;
    std::array<juce::Rectangle<int>, 4> modeTabs;
    juce::Rectangle<int> slotA;
    juce::Rectangle<int> slotB;
    juce::Rectangle<int> learn;
    juce::Rectangle<int> meter;
    juce::Rectangle<int> menu;
    juce::Rectangle<int> gain;
    juce::Rectangle<int> scene;
    juce::Rectangle<int> output;
    juce::Rectangle<int> meterHandle;
    juce::Rectangle<int> learnBody;
    juce::Rectangle<int> dock;
    juce::Rectangle<int> meterDrawer;
};

inline SqueezeBoxes squeezeBoxes(int width, int height, bool learnOpen = false, bool meterOpen = false)
{
    SqueezeBoxes boxes;
    const float scale = static_cast<float>(width) / static_cast<float>(kDefaultWidth);
    const auto px = [scale](float value) { return juce::roundToInt(value * scale); };

    const int headerH = juce::jmax(48, px(static_cast<float>(kHeaderHeight)));
    const int margin = juce::jmax(12, px(16.0f));
    const int gap = juce::jmax(6, px(8.0f));
    boxes.header = { 0, 0, width, headerH };

    const int pillH = juce::jmax(28, headerH - px(16.0f));
    const int y = (headerH - pillH) / 2;
    const int menuW = juce::jmax(28, px(36.0f));
    const int pillW = juce::jmax(76, px(96.0f));
    const int slotW = juce::jmax(32, px(40.0f));

    int right = width - margin;
    boxes.menu = { right - menuW, y, menuW, pillH };
    right = boxes.menu.getX() - gap;
    boxes.meter = { right - pillW, y, pillW, pillH };
    right = boxes.meter.getX() - gap;
    boxes.learn = { right - pillW, y, pillW, pillH };
    right = boxes.learn.getX() - gap;
    boxes.slotB = { right - slotW, y, slotW, pillH };
    right = boxes.slotB.getX() - px(4.0f);
    boxes.slotA = { right - slotW, y, slotW, pillH };

    int logoW = juce::jmax(108, px(160.0f));
    boxes.logo = { margin, y, logoW, pillH };

    int tabsLeft = boxes.logo.getRight() + gap;
    int tabsRight = boxes.slotA.getX() - gap;
    if (tabsRight - tabsLeft < 4 * 78)
    {
        logoW = juce::jmax(96, tabsRight - gap - 4 * 78 - margin);
        boxes.logo = { margin, y, logoW, pillH };
        tabsLeft = boxes.logo.getRight() + gap;
    }

    const int tabGap = juce::jmax(4, px(6.0f));
    const int tabsWidth = juce::jmax(0, tabsRight - tabsLeft);
    const int tabW = juce::jmax(64, (tabsWidth - 3 * tabGap) / 4);
    for (int index = 0; index < 4; ++index)
        boxes.modeTabs[static_cast<size_t>(index)] = { tabsLeft + index * (tabW + tabGap), y, tabW, pillH };

    const int bodyTop = headerH + juce::jmax(8, px(10.0f));
    const int handleH = juce::jmax(32, px(34.0f));
    const int handleW = juce::jmax(92, px(112.0f));
    const int bottomMargin = juce::jmax(8, px(10.0f));
    boxes.meterHandle = { (width - handleW) / 2, height - bottomMargin - handleH, handleW, handleH };

    const int gainW = juce::jlimit(148, 220, px(200.0f));
    const int gainH = juce::jmax(108, px(132.0f));
    boxes.gain = { margin, bodyTop, gainW, gainH };

    const int outW = juce::jmax(68, px(72.0f));
    const int outTop = bodyTop + px(4.0f);
    const int outBottom = boxes.meterHandle.getY() - juce::jmax(8, px(10.0f));
    boxes.output = { width - margin - outW, outTop, outW, juce::jmax(100, outBottom - outTop) };

    const int sceneLeft = boxes.gain.getRight() + gap;
    const int sceneRight = boxes.output.getX() - gap;
    const int sceneBottom = boxes.meterHandle.getY() - juce::jmax(6, px(8.0f));
    boxes.scene = { sceneLeft,
                    bodyTop,
                    juce::jmax(0, sceneRight - sceneLeft),
                    juce::jmax(0, sceneBottom - bodyTop) };

    if (meterOpen)
    {
        const int drawerH = juce::jlimit(120, 180, px(148.0f));
        boxes.meterDrawer = { 0, height - drawerH, width, drawerH };
        boxes.meterHandle = { (width - handleW) / 2, boxes.meterDrawer.getY() - handleH, handleW, handleH };
        const int sceneBottomOpen = boxes.meterHandle.getY() - gap;
        boxes.scene.setHeight(juce::jmax(80, sceneBottomOpen - boxes.scene.getY()));
        boxes.output.setHeight(juce::jmax(80, sceneBottomOpen - boxes.output.getY()));
        boxes.gain.setHeight(juce::jmin(boxes.gain.getHeight(), boxes.scene.getHeight()));
    }

    if (learnOpen)
    {
        const int sideW = juce::jlimit(210, 280, px(250.0f));
        const int dockW = juce::jlimit(220, 300, px(270.0f));
        const int bodyBottom = boxes.scene.getBottom();
        boxes.learnBody = { margin,
                            boxes.gain.getBottom() + gap,
                            sideW,
                            juce::jmax(80, bodyBottom - boxes.gain.getBottom() - gap) };
        boxes.dock = { boxes.output.getX() - gap - dockW, boxes.scene.getY(), dockW, boxes.scene.getHeight() };
        const int left = boxes.learnBody.getRight() + gap;
        const int rightEdge = boxes.dock.getX() - gap;
        boxes.scene = { left, boxes.scene.getY(), juce::jmax(80, rightEdge - left), boxes.scene.getHeight() };
    }

    return boxes;
}

inline bool rectanglesIntersect(juce::Rectangle<int> a, juce::Rectangle<int> b)
{
    return a.intersects(b);
}

} // namespace squeeze::ui
