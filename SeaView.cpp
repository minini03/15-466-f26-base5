#include "SeaView.hpp"

#include "SeaMeshes.hpp"
#include "SeaProgram.hpp"
#include "Scene.hpp"
#include "GL.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

void draw_sea(Game const &game, float time, float notice, glm::uvec2 const &drawable_size, TextRenderer &text) {
	glClearColor(0.02f, 0.10f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	Player const *me = game.players.empty() ? nullptr : &game.players.front();
	float my_radius = me ? me->radius : Game::StartRadius;
	glm::vec2 cam = me ? me->position : glm::vec2(0.0f);

	float aspect = float(drawable_size.x) / float(std::max< uint32_t >(1, drawable_size.y));
	float view_h = 3.6f + my_radius * 4.8f;
	float half_h = view_h * 0.5f;
	float half_w = half_h * aspect;
	float arena_w = Game::ArenaMax.x - Game::ArenaMin.x;
	float arena_h = Game::ArenaMax.y - Game::ArenaMin.y;
	if (view_h >= arena_h) cam.y = 0.5f * (Game::ArenaMin.y + Game::ArenaMax.y);
	else cam.y = std::clamp(cam.y, Game::ArenaMin.y + half_h, Game::ArenaMax.y - half_h);
	if (half_w * 2.0f >= arena_w) cam.x = 0.5f * (Game::ArenaMin.x + Game::ArenaMax.x);
	else cam.x = std::clamp(cam.x, Game::ArenaMin.x + half_w, Game::ArenaMax.x - half_w);

	float fovy = glm::radians(40.0f);
	float tilt = glm::radians(50.0f);
	float dist = half_h * std::cos(tilt) / std::tan(fovy * 0.5f);
	glm::vec3 target(cam.x, cam.y, 0.0f);
	glm::vec3 eye = target + glm::vec3(0.0f, -dist * std::sin(tilt), dist * std::cos(tilt));
	glm::mat4 clip_from_world = glm::perspective(fovy, aspect, 0.05f, 80.0f) * glm::lookAt(eye, target, glm::vec3(0.0f, 0.0f, 1.0f));

	glm::quat light_rot = glm::angleAxis(glm::radians(-32.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::vec3 shine = -glm::mat3_cast(light_rot)[2];
	glm::quat ident(1.0f, 0.0f, 0.0f, 0.0f);

	//Authored environment from sea.blend:
	if (sea_assets.sea_scene) {
		sea_assets.sea_scene->draw(clip_from_world);
	}

	Scene scene;
	Scene::Drawable::Pipeline base;
	base.program = sea_program->program;
	base.vao = sea_assets.sea_vao;
	base.type = GL_TRIANGLES;
	base.CLIP_FROM_OBJECT_mat4 = sea_program->CLIP_FROM_OBJECT_mat4;
	base.LIGHT_FROM_OBJECT_mat4x3 = -1U;
	base.LIGHT_FROM_NORMAL_mat3 = sea_program->LIGHT_FROM_NORMAL_mat3;
	base.textures[0].texture = sea_assets.white_tex;
	base.textures[0].target = GL_TEXTURE_2D;

	GLint tint_loc = GLint(sea_program->TINT_vec3);
	GLint dir_loc = GLint(sea_program->LIGHT_DIRECTION_vec3);
	GLint energy_loc = GLint(sea_program->LIGHT_ENERGY_vec3);

	auto add_xf = [&](Scene::Transform *parent, glm::vec3 const &pos, glm::quat const &rot, glm::vec3 const &scale) {
		scene.transforms.emplace_back();
		Scene::Transform *t = &scene.transforms.back();
		t->parent = parent;
		t->position = pos;
		t->rotation = rot;
		t->scale = scale;
		return t;
	};
	auto draw_mesh = [&](Scene::Transform *xf, Mesh const &mesh, GLuint tex, glm::vec3 const &tint, GLuint vao = 0) {
		if (vao == 0) vao = sea_assets.sea_vao;
		scene.drawables.emplace_back(xf);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.pipeline = base;
		drawable.pipeline.vao = vao;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		drawable.pipeline.textures[0].texture = tex;
		drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
		drawable.pipeline.set_uniforms = [=]() {
			glUniform3f(tint_loc, tint.x, tint.y, tint.z);
			glUniform3f(dir_loc, shine.x, shine.y, shine.z);
			glUniform3f(energy_loc, 1.0f, 0.98f, 0.94f);
		};
	};

	struct NameAnchor {
		glm::vec3 at;
		float radius;
		std::string name;
	};
	std::vector< NameAnchor > names;

	auto add_fish = [&](glm::vec2 const &pos, float facing, float radius, glm::vec3 const &color, bool invuln, std::string const &name) {
		float hover = 0.20f + radius * 0.18f;
		glm::quat facing_rot = glm::angleAxis(facing, glm::vec3(0.0f, 0.0f, 1.0f));
		Scene::Transform *root = add_xf(nullptr, glm::vec3(pos.x, pos.y, hover), facing_rot, glm::vec3(radius));
		root->name = name;

		float shadow_z = (0.025f - hover) / std::max(radius, 0.05f);
		Scene::Transform *shadow = add_xf(root, glm::vec3(0.0f, 0.0f, shadow_z), ident, glm::vec3(1.05f, 0.70f, 1.0f));
		draw_mesh(shadow, sea_assets.disc, sea_assets.white_tex, glm::vec3(0.03f, 0.06f, 0.07f));

		draw_mesh(root, sea_assets.fish_body, sea_assets.white_tex, color, sea_assets.fish_vao);
		draw_mesh(root, sea_assets.fish_mouth, sea_assets.white_tex, glm::vec3(1.0f, 0.55f, 0.72f), sea_assets.fish_vao);
		draw_mesh(root, sea_assets.fish_eye1, sea_assets.white_tex, glm::vec3(0.04f, 0.04f, 0.05f), sea_assets.fish_vao);
		draw_mesh(root, sea_assets.fish_eye2, sea_assets.white_tex, glm::vec3(0.04f, 0.04f, 0.05f), sea_assets.fish_vao);

		if (invuln) {
			float pulse = 1.0f + 0.06f * std::sin(time * 5.5f);
			Scene::Transform *ring = add_xf(root, glm::vec3(0.0f, 0.0f, 0.55f), ident, glm::vec3(pulse));
			draw_mesh(ring, sea_assets.ring, sea_assets.white_tex, glm::vec3(0.95f, 0.97f, 1.0f));
		}
		if (!name.empty()) {
			names.push_back(NameAnchor{glm::vec3(pos.x, pos.y, hover + radius * 1.15f), radius, name});
		}
	};

	for (auto const &npc : game.npcs) {
		add_fish(npc.position, npc.facing, npc.radius, npc.color, false, "");
	}
	for (auto const &bait : game.baits) {
		float bob = std::sin(time * 3.2f + bait.position.x * 2.0f) * 0.04f;
		float s = Game::BaitRadius * 1.6f * (bait.life < 1.0f ? std::max(bait.life, 0.25f) : 1.0f);
		Scene::Transform *pellet = add_xf(nullptr, glm::vec3(bait.position.x, bait.position.y, 0.28f + bob), ident, glm::vec3(s));
		draw_mesh(pellet, sea_assets.sphere, sea_assets.white_tex, glm::vec3(1.0f, 0.62f, 0.16f));
	}
	for (auto const &player : game.players) {
		add_fish(player.position, player.facing, player.radius, player.color, player.invuln > 0.0f, player.name);
	}

	scene.draw(clip_from_world);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::vector< TextLabel > labels;
	auto to_px = [&](glm::vec3 const &world, glm::vec2 &px) {
		glm::vec4 c = clip_from_world * glm::vec4(world, 1.0f);
		if (c.w <= 0.05f) return false;
		glm::vec2 ndc = glm::vec2(c.x, c.y) / c.w;
		px = glm::vec2((ndc.x * 0.5f + 0.5f) * float(drawable_size.x), (ndc.y * 0.5f + 0.5f) * float(drawable_size.y));
		return true;
	};
	float screen_h = float(drawable_size.y);
	for (NameAnchor const &anchor : names) {
		glm::vec2 px;
		if (!to_px(anchor.at, px)) continue;
		float nh = std::clamp(screen_h * 0.018f * std::sqrt(anchor.radius / Game::StartRadius), screen_h * 0.016f, screen_h * 0.032f);
		TextLabel label;
		label.text = anchor.name;
		label.x = px.x;
		label.y = px.y - nh * 0.35f;
		label.height = nh;
		label.color = glm::vec3(1.0f);
		label.center = true;
		labels.push_back(label);
	}

	if (me) {
		bool biggest = true;
		for (auto const &player : game.players) {
			if (&player != me && player.radius > me->radius + 0.001f) biggest = false;
		}
		for (auto const &npc : game.npcs) {
			if (npc.radius > me->radius + 0.001f) biggest = false;
		}
		float H = screen_h * 0.040f;
		float x = screen_h * 0.03f;
		float y = screen_h - screen_h * 0.03f - H;
		char buf[96];
		std::snprintf(buf, sizeof(buf), "score %u    alive %.0fs", me->score, me->alive);
		labels.push_back(TextLabel{buf, x, y, H, glm::vec3(1.0f), false});
		char dash_buf[80];
		if (me->dash_cd > 0.05f) {
			std::snprintf(dash_buf, sizeof(dash_buf), "WASD / arrows swim    dash %.1fs", me->dash_cd);
		} else {
			std::snprintf(dash_buf, sizeof(dash_buf), "WASD / arrows swim    dash ready");
		}
		float sub = H * 0.72f;
		labels.push_back(TextLabel{dash_buf, x, y - H * 1.35f, sub, glm::vec3(0.82f, 0.90f, 0.94f), false});
		labels.push_back(TextLabel{"eat bait and smaller fish", x, y - H * 2.55f, sub, glm::vec3(0.82f, 0.90f, 0.94f), false});
		if (biggest) {
			labels.push_back(TextLabel{"you are the biggest fish", x, y - H * 3.9f, H * 0.85f, glm::vec3(1.0f, 0.86f, 0.47f), false});
		}
		if (notice > 0.0f) {
			labels.push_back(TextLabel{"you got eaten — respawned small", float(drawable_size.x) * 0.5f, screen_h * 0.08f, H * 0.9f, glm::vec3(1.0f, 0.67f, 0.63f), true});
		}
	}

	text.draw(drawable_size, labels);
	glDisable(GL_BLEND);
	GL_ERRORS();
}
