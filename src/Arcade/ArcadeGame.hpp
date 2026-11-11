#pragma once

class RenderBuffer;

class ArcadeGame
{
public:
  virtual ~ArcadeGame() = default;
  virtual void step_world(float elapsed_time) = 0;
  virtual void render_frame(RenderBuffer &buffer) = 0;
  virtual bool finished() const { return false; }
};