#include "SeaProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< SeaProgram > sea_program(LoadTagEarly, []() -> SeaProgram const * {
	return new SeaProgram();
});

SeaProgram::SeaProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"uniform mat3 LIGHT_FROM_NORMAL;\n"
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = CLIP_FROM_OBJECT * Position;\n"
		"	normal = LIGHT_FROM_NORMAL * Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		"#version 330\n"
		"uniform sampler2D TEX;\n"
		"uniform vec3 LIGHT_DIRECTION;\n"
		"uniform vec3 LIGHT_ENERGY;\n"
		"uniform vec3 TINT;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 n = normalize(normal);\n"
		"	//LIGHT_DIRECTION is the direction the scene light travels (its -Z).\n"
		"	vec3 toward_source = normalize(-LIGHT_DIRECTION);\n"
		"	float hemi = dot(n, toward_source) * 0.5 + 0.5;\n"
		"	vec3 light = mix(vec3(0.40), vec3(1.08), clamp(hemi, 0.0, 1.0)) * LIGHT_ENERGY;\n"
		"	vec3 albedo = texture(TEX, texCoord).rgb * color.rgb * TINT;\n"
		"	fragColor = vec4(light * albedo, 1.0);\n"
		"}\n"
	);

	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
	LIGHT_FROM_NORMAL_mat3 = glGetUniformLocation(program, "LIGHT_FROM_NORMAL");
	LIGHT_DIRECTION_vec3 = glGetUniformLocation(program, "LIGHT_DIRECTION");
	LIGHT_ENERGY_vec3 = glGetUniformLocation(program, "LIGHT_ENERGY");
	TINT_vec3 = glGetUniformLocation(program, "TINT");

	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "TEX"), 0);
	glUniform3f(TINT_vec3, 1.0f, 1.0f, 1.0f);
	glUniform3f(LIGHT_DIRECTION_vec3, 0.0f, 0.45f, -1.0f);
	glUniform3f(LIGHT_ENERGY_vec3, 1.0f, 0.98f, 0.94f);
	glUseProgram(0);

	GL_ERRORS();
}

SeaProgram::~SeaProgram() {
	glDeleteProgram(program);
	program = 0;
}
