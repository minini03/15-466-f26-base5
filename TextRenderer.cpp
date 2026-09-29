#include "TextRenderer.hpp"

#include "data_path.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Glyph {
	float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
	float w = 0.0f, h = 0.0f;
	float left = 0.0f, top = 0.0f;
};

struct TextRenderer::Impl {
	FT_Library ft = nullptr;
	FT_Face face = nullptr;
	hb_font_t *hb_font = nullptr;
	hb_buffer_t *hb_buf = nullptr;

	GLuint tex = 0;
	GLuint program = 0;
	GLuint vao = 0;
	GLuint vbo = 0;
	GLint clip_loc = -1;
	GLint pos_loc = -1;
	GLint uv_loc = -1;
	GLint col_loc = -1;

	int atlas = 1024;
	int pen_x = 1;
	int pen_y = 1;
	int row_h = 0;
	int font_px = 64;

	std::unordered_map< uint32_t, Glyph > glyphs;

	Impl() {
		if (FT_Init_FreeType(&ft) != 0) {
			throw std::runtime_error("FT_Init_FreeType failed");
		}
		std::string font_path = data_path("Original_Surfer/OriginalSurfer-Regular.ttf");
		if (FT_New_Face(ft, font_path.c_str(), 0, &face) != 0) {
			throw std::runtime_error("Could not open font '" + font_path + "'.");
		}
		if (FT_Set_Pixel_Sizes(face, 0, FT_UInt(font_px)) != 0) {
			throw std::runtime_error("FT_Set_Pixel_Sizes failed");
		}
		hb_font = hb_ft_font_create(face, nullptr);
		if (!hb_font) throw std::runtime_error("hb_ft_font_create failed");
		hb_buf = hb_buffer_create();

		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		std::vector< glm::u8vec4 > blank(size_t(atlas) * size_t(atlas), glm::u8vec4(255, 255, 255, 0));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlas, atlas, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		program = gl_compile_program(
			"#version 330\n"
			"uniform mat4 CLIP_FROM_PIXEL;\n"
			"in vec2 Position;\n"
			"in vec2 TexCoord;\n"
			"in vec4 Color;\n"
			"out vec2 texCoord;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = CLIP_FROM_PIXEL * vec4(Position, 0.0, 1.0);\n"
			"	texCoord = TexCoord;\n"
			"	color = Color;\n"
			"}\n"
		,
			"#version 330\n"
			"uniform sampler2D TEX;\n"
			"in vec2 texCoord;\n"
			"in vec4 color;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	float a = texture(TEX, texCoord).a;\n"
			"	fragColor = vec4(color.rgb, color.a * a);\n"
			"}\n"
		);
		clip_loc = glGetUniformLocation(program, "CLIP_FROM_PIXEL");
		pos_loc = glGetAttribLocation(program, "Position");
		uv_loc = glGetAttribLocation(program, "TexCoord");
		col_loc = glGetAttribLocation(program, "Color");
		glUseProgram(program);
		glUniform1i(glGetUniformLocation(program, "TEX"), 0);
		glUseProgram(0);

		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)0);
		glEnableVertexAttribArray(pos_loc);
		glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)(sizeof(float) * 2));
		glEnableVertexAttribArray(uv_loc);
		glVertexAttribPointer(col_loc, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 8, (void *)(sizeof(float) * 4));
		glEnableVertexAttribArray(col_loc);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		GL_ERRORS();
	}

	~Impl() {
		if (hb_buf) hb_buffer_destroy(hb_buf);
		if (hb_font) hb_font_destroy(hb_font);
		if (face) FT_Done_Face(face);
		if (ft) FT_Done_FreeType(ft);
		if (tex) glDeleteTextures(1, &tex);
		if (vbo) glDeleteBuffers(1, &vbo);
		if (vao) glDeleteVertexArrays(1, &vao);
		if (program) glDeleteProgram(program);
	}

	Glyph const &cache(uint32_t glyph_id) {
		auto found = glyphs.find(glyph_id);
		if (found != glyphs.end()) return found->second;

		Glyph g;
		if (FT_Load_Glyph(face, glyph_id, FT_LOAD_RENDER) != 0) {
			glyphs.emplace(glyph_id, g);
			return glyphs.at(glyph_id);
		}
		FT_Bitmap const &bmp = face->glyph->bitmap;
		g.left = float(face->glyph->bitmap_left);
		g.top = float(face->glyph->bitmap_top);
		g.w = float(bmp.width);
		g.h = float(bmp.rows);
		if (bmp.width > 0 && bmp.rows > 0 && bmp.buffer != nullptr && bmp.pixel_mode == FT_PIXEL_MODE_GRAY) {
			int w = int(bmp.width);
			int h = int(bmp.rows);
			if (pen_x + w + 1 >= atlas) {
				pen_x = 1;
				pen_y += row_h + 1;
				row_h = 0;
			}
			if (pen_y + h + 1 < atlas) {
				std::vector< glm::u8vec4 > pix(size_t(w) * size_t(h));
				for (int row = 0; row < h; ++row) {
					//FreeType row 0 is the top. Upload the bottom row first so GL's v axis stays upright.
					int src_row = h - 1 - row;
					unsigned char const *src = bmp.buffer + src_row * bmp.pitch;
					for (int col = 0; col < w; ++col) {
						pix[size_t(row) * size_t(w) + size_t(col)] = glm::u8vec4(255, 255, 255, src[col]);
					}
				}
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glBindTexture(GL_TEXTURE_2D, tex);
				glTexSubImage2D(GL_TEXTURE_2D, 0, pen_x, pen_y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());
				glBindTexture(GL_TEXTURE_2D, 0);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				g.u0 = float(pen_x) / float(atlas);
				g.v0 = float(pen_y) / float(atlas);
				g.u1 = float(pen_x + w) / float(atlas);
				g.v1 = float(pen_y + h) / float(atlas);
				pen_x += w + 1;
				if (h > row_h) row_h = h;
			}
		}
		auto inserted = glyphs.emplace(glyph_id, g);
		return inserted.first->second;
	}

	void draw(glm::uvec2 const &drawable_size, std::vector< TextLabel > const &labels) {
		if (labels.empty() || drawable_size.x == 0 || drawable_size.y == 0) return;

		struct V {
			float x, y, u, v, r, g, b, a;
		};
		std::vector< V > verts;
		verts.reserve(labels.size() * 24 * 12);

		auto emit = [&](float x0, float y0, float x1, float y1, Glyph const &g, glm::vec4 const &col) {
			auto push = [&](float x, float y, float u, float v) {
				verts.push_back(V{x, y, u, v, col.r, col.g, col.b, col.a});
			};
			push(x0, y0, g.u0, g.v0);
			push(x1, y0, g.u1, g.v0);
			push(x1, y1, g.u1, g.v1);
			push(x0, y0, g.u0, g.v0);
			push(x1, y1, g.u1, g.v1);
			push(x0, y1, g.u0, g.v1);
		};

		for (TextLabel const &label : labels) {
			if (label.text.empty() || label.height <= 0.0f) continue;
			hb_buffer_clear_contents(hb_buf);
			hb_buffer_add_utf8(hb_buf, label.text.c_str(), int(label.text.size()), 0, -1);
			hb_buffer_guess_segment_properties(hb_buf);
			hb_shape(hb_font, hb_buf, nullptr, 0);

			unsigned int count = 0;
			hb_glyph_info_t *info = hb_buffer_get_glyph_infos(hb_buf, &count);
			hb_glyph_position_t *pos = hb_buffer_get_glyph_positions(hb_buf, &count);

			float cursor = 0.0f;
			struct Stamp { Glyph g; float x, y; };
			std::vector< Stamp > stamps;
			stamps.reserve(count);
			for (unsigned int i = 0; i < count; ++i) {
				Glyph const &g = cache(info[i].codepoint);
				float xo = float(pos[i].x_offset) / 64.0f;
				float yo = float(pos[i].y_offset) / 64.0f;
				if (g.w > 0.0f && g.h > 0.0f) {
					float x = cursor + xo + g.left;
					float y = yo + (g.top - g.h);
					stamps.push_back(Stamp{g, x, y});
				}
				cursor += float(pos[i].x_advance) / 64.0f;
			}
			float scale = label.height / float(font_px);
			float origin_x = label.center ? label.x - cursor * scale * 0.5f : label.x;
			float origin_y = label.y;
			float shadow = std::max(1.0f, label.height * 0.045f);
			for (Stamp const &s : stamps) {
				float x0 = origin_x + s.x * scale;
				float y0 = origin_y + s.y * scale;
				float x1 = x0 + s.g.w * scale;
				float y1 = y0 + s.g.h * scale;
				emit(x0 + shadow, y0 - shadow, x1 + shadow, y1 - shadow, s.g, glm::vec4(0.02f, 0.05f, 0.08f, 0.85f));
				emit(x0, y0, x1, y1, s.g, glm::vec4(label.color, 1.0f));
			}
		}
		if (verts.empty()) return;

		glm::mat4 clip = glm::ortho(0.0f, float(drawable_size.x), 0.0f, float(drawable_size.y));
		glUseProgram(program);
		glUniformMatrix4fv(clip_loc, 1, GL_FALSE, glm::value_ptr(clip));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, GLsizei(verts.size()));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
		GL_ERRORS();
	}
};

TextRenderer::TextRenderer() : impl(new Impl()) {
}

TextRenderer::~TextRenderer() {
	delete impl;
	impl = nullptr;
}

void TextRenderer::draw(glm::uvec2 const &drawable_size, std::vector< TextLabel > const &labels) {
	impl->draw(drawable_size, labels);
}
