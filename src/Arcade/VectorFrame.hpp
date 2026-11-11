#pragma once

#include "../Rendering/RenderBuffer.hpp"
#include <cmath>

class VectorFrame
{
private:
  RenderBuffer &buffer;
  b2Vec2 cursor = b2Vec2(-30, -30);

public:
  explicit VectorFrame(RenderBuffer &target) : buffer(target)
  {
    buffer.drawing_frame->clear();
  }

  void line(const b2Vec2 &start, const b2Vec2 &end)
  {
    if ((start - cursor).LengthSquared() > 0.0001f)
    {
      buffer.renderSegment(false, cursor, start);
    }
    buffer.renderSegment(true, start, end);
    cursor = end;
  }

  void point(const b2Vec2 &position, uint16_t extra_dwell_us = 0)
  {
    if ((position - cursor).LengthSquared() > 0.0001f)
    {
      buffer.renderSegment(false, cursor, position);
    }
    buffer.renderSegment(true, position, position, 1, extra_dwell_us);
    cursor = position;
  }

  void circle(const b2Vec2 &center, float radius)
  {
    b2Vec2 previous = center + b2Vec2(radius, 0);
    for (int vertex = 1; vertex <= 12; ++vertex)
    {
      const float angle = vertex * (2.0f * b2_pi / 12);
      const b2Vec2 next = center + radius * b2Vec2(cosf(angle), sinf(angle));
      line(previous, next);
      previous = next;
    }
  }

  void text(float x, float y, const char *message)
  {
    cursor = buffer.draw_text(cursor, x, y, message, false);
  }

  void centered(float y, const char *message)
  {
    const b2Vec2 size = buffer.draw_text(cursor, 0, 0, message, true);
    text(-size.x * 0.5f, y, message);
  }

  void finish()
  {
    //buffer.renderSegment(false, cursor, b2Vec2(-30, -30));
    buffer.needs_render = false;
  }
};