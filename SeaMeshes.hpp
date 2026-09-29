#pragma once

#include "GL.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"

//Authored assets: sea.blend -> sea.pnct/sea.scene (gameplay units), fish.blend -> fish.pnct.
struct SeaAssets {
	MeshBuffer *sea_buffer = nullptr;
	MeshBuffer *fish_buffer = nullptr;
	Scene *sea_scene = nullptr;

	GLuint sea_vao = 0;
	GLuint fish_vao = 0;
	GLuint white_tex = 0; //1x1 white; no procedural pattern textures

	Mesh sphere;
	Mesh disc;
	Mesh ring;

	Mesh fish_body;
	Mesh fish_mouth;
	Mesh fish_eye1;
	Mesh fish_eye2;
};

extern SeaAssets sea_assets;
