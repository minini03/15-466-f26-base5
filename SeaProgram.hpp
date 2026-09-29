#pragma once

#include "GL.hpp"
#include "Load.hpp"

//Lit fish shader: hemisphere light, a grayscale texture, and a per-draw tint.
//Drawn through Scene::Drawable, not DrawLines.
struct SeaProgram {
	SeaProgram();
	~SeaProgram();

	GLuint program = 0;

	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	GLuint CLIP_FROM_OBJECT_mat4 = -1U;
	GLuint LIGHT_FROM_NORMAL_mat3 = -1U;
	GLuint LIGHT_DIRECTION_vec3 = -1U;
	GLuint LIGHT_ENERGY_vec3 = -1U;
	GLuint TINT_vec3 = -1U;
};

extern Load< SeaProgram > sea_program;
