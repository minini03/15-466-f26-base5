#pragma once

#include "GL.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

//Screen-space text shaped with HarfBuzz and rasterized with FreeType.
struct TextLabel {
	std::string text;
	float x = 0.0f; //pixels from the left
	float y = 0.0f; //baseline, pixels from the bottom
	float height = 32.0f;
	glm::vec3 color = glm::vec3(1.0f);
	bool center = false; //center horizontally on x
};

struct TextRenderer {
	TextRenderer();
	~TextRenderer();

	TextRenderer(TextRenderer const &) = delete;
	TextRenderer &operator=(TextRenderer const &) = delete;

	void draw(glm::uvec2 const &drawable_size, std::vector< TextLabel > const &labels);

	private:
	struct Impl;
	Impl *impl = nullptr;
};
