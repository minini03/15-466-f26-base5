#include "SeaMeshes.hpp"

#include "SeaProgram.hpp"
#include "Load.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"

#include <glm/glm.hpp>

SeaAssets sea_assets;

namespace {

GLuint make_white_tex() {
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glm::u8vec4 white(255, 255, 255, 255);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

void load_sea_assets() {
	sea_assets.white_tex = make_white_tex();

	sea_assets.sea_buffer = new MeshBuffer(data_path("sea.pnct"));
	sea_assets.sea_vao = sea_assets.sea_buffer->make_vao_for_program(sea_program->program);
	sea_assets.sphere = sea_assets.sea_buffer->lookup("Sphere");
	sea_assets.disc = sea_assets.sea_buffer->lookup("Disc");
	sea_assets.ring = sea_assets.sea_buffer->lookup("Ring");

	sea_assets.fish_buffer = new MeshBuffer(data_path("fish.pnct"));
	sea_assets.fish_vao = sea_assets.fish_buffer->make_vao_for_program(sea_program->program);
	sea_assets.fish_body = sea_assets.fish_buffer->lookup("body");
	sea_assets.fish_mouth = sea_assets.fish_buffer->lookup("mouth");
	sea_assets.fish_eye1 = sea_assets.fish_buffer->lookup("eye1");
	sea_assets.fish_eye2 = sea_assets.fish_buffer->lookup("eye2");

	//Static environment from sea.blend. Skip FX helper objects (used only as mesh sources).
	//Authored in gameplay units: sand top z=0, wall inner opening = Arena ±12.5.
	sea_assets.sea_scene = new Scene(data_path("sea.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		if (transform->name == "Sphere" || transform->name == "Disc" || transform->name == "Ring") {
			return;
		}
		Mesh const &mesh = sea_assets.sea_buffer->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline.program = sea_program->program;
		drawable.pipeline.vao = sea_assets.sea_vao;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		drawable.pipeline.CLIP_FROM_OBJECT_mat4 = sea_program->CLIP_FROM_OBJECT_mat4;
		drawable.pipeline.LIGHT_FROM_OBJECT_mat4x3 = -1U;
		drawable.pipeline.LIGHT_FROM_NORMAL_mat3 = sea_program->LIGHT_FROM_NORMAL_mat3;
		drawable.pipeline.textures[0].texture = sea_assets.white_tex;
		drawable.pipeline.textures[0].target = GL_TEXTURE_2D;

		glm::vec3 tint(1.0f);
		if (transform->name == "Sand") tint = glm::vec3(0.72f, 0.68f, 0.48f);
		else if (transform->name == "Wall") tint = glm::vec3(0.55f, 0.88f, 0.92f);

		GLint tint_loc = GLint(sea_program->TINT_vec3);
		GLint dir_loc = GLint(sea_program->LIGHT_DIRECTION_vec3);
		GLint energy_loc = GLint(sea_program->LIGHT_ENERGY_vec3);
		drawable.pipeline.set_uniforms = [tint, tint_loc, dir_loc, energy_loc]() {
			glm::vec3 shine = glm::normalize(glm::vec3(0.15f, 0.35f, 1.0f));
			glUniform3f(tint_loc, tint.x, tint.y, tint.z);
			glUniform3f(dir_loc, -shine.x, -shine.y, -shine.z);
			glUniform3f(energy_loc, 1.0f, 0.98f, 0.94f);
		};
	});

	GL_ERRORS();
}

Load< void > load_sea_meshes(LoadTagLate, [](){
	load_sea_assets();
});

}
