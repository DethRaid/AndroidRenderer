#pragma once

class CommandBuffer;
class SceneView;

class PhaseInterface {
public:
    virtual void render(CommandBuffer& commands, SceneView& view) = 0;
};



