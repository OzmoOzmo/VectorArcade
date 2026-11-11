#include "StartState.hpp"
#include "../Game.hpp"
#include "../../Controls/Controls.hpp"
#include "../../HardwareConfig.h"

GAME_STATE StartState::handle(Game *game, float elapsed_time)
{
#if ASTEROIDS_RENDERER_BENCHMARK
  return GAME_STATE_PLAYING;
#else
  // wait for fire to be pushed
  if (game->get_controls()->is_firing())
  {
    return GAME_STATE_PLAYING;
  }
  return GAME_STATE_START;
#endif
}

void StartState::enter(Game *game)
{
}
void StartState::exit(Game *game)
{
}
