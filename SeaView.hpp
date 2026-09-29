#pragma once

#include "Game.hpp"
#include "TextRenderer.hpp"

#include <glm/glm.hpp>

//Build a scene of lit meshes and draw names / HUD with TextRenderer.
void draw_sea(Game const &game, float time, float notice, glm::uvec2 const &drawable_size, TextRenderer &text);
