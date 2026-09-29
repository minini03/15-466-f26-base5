#include "PlayMode.hpp"

#include "SeaView.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

PlayMode::PlayMode(Client &client_) : client(client_) {
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	(void)window_size;

	auto set_down = [](Button &b) {
		b.downs += 1;
		b.pressed = true;
	};
	auto set_up = [](Button &b) {
		b.pressed = false;
	};

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.key == SDLK_A || evt.key.key == SDLK_LEFT) {
			set_down(controls.left);
			return true;
		} else if (evt.key.key == SDLK_D || evt.key.key == SDLK_RIGHT) {
			set_down(controls.right);
			return true;
		} else if (evt.key.key == SDLK_W || evt.key.key == SDLK_UP) {
			set_down(controls.up);
			return true;
		} else if (evt.key.key == SDLK_S || evt.key.key == SDLK_DOWN) {
			set_down(controls.down);
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			set_down(controls.jump);
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A || evt.key.key == SDLK_LEFT) {
			set_up(controls.left);
			return true;
		} else if (evt.key.key == SDLK_D || evt.key.key == SDLK_RIGHT) {
			set_up(controls.right);
			return true;
		} else if (evt.key.key == SDLK_W || evt.key.key == SDLK_UP) {
			set_up(controls.up);
			return true;
		} else if (evt.key.key == SDLK_S || evt.key.key == SDLK_DOWN) {
			set_up(controls.down);
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			set_up(controls.jump);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	time += elapsed;

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.jump.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	if (!game.players.empty()) {
		float radius = game.players.front().radius;
		if (prev_radius > 0.0f && radius < prev_radius * 0.75f) notice = 2.4f;
		prev_radius = radius;
	}
	notice = std::max(0.0f, notice - elapsed);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	draw_sea(game, time, notice, drawable_size, text);
}
