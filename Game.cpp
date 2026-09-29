#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 5;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	send_button(left);
	send_button(right);
	send_button(up);
	send_button(down);
	send_button(jump);
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 5) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 5!");

	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	recv_button(recv_buffer[4+0], &left);
	recv_button(recv_buffer[4+1], &right);
	recv_button(recv_buffer[4+2], &up);
	recv_button(recv_buffer[4+3], &down);
	recv_button(recv_buffer[4+4], &jump);

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}

void Player::send_join_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint8_t len = uint8_t(std::min< size_t >(24, name.size()));
	uint32_t size = 4 + len;
	connection.send(Message::C2S_Join);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto byte = [](float channel) {
		return uint8_t(std::clamp(int(std::lround(channel * 255.0f)), 0, 255));
	};
	connection.send(byte(color.r));
	connection.send(byte(color.g));
	connection.send(byte(color.b));
	connection.send(len);
	connection.send_buffer.insert(connection.send_buffer.end(), name.begin(), name.begin() + len);
}

bool Player::recv_join_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Join)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size < 4) throw std::runtime_error("Join message shorter than name header.");
	if (recv_buffer.size() < 4 + size) return false;

	auto channel = [](uint8_t byte) {
		return float(byte) / 255.0f;
	};
	color.r = channel(recv_buffer[4]);
	color.g = channel(recv_buffer[5]);
	color.b = channel(recv_buffer[6]);
	uint8_t len = recv_buffer[7];
	if (uint32_t(4 + len) != size) throw std::runtime_error("Join message size does not match name.");
	name.assign(reinterpret_cast< char const * >(&recv_buffer[8]), reinterpret_cast< char const * >(&recv_buffer[8]) + len);
	if (name.empty()) throw std::runtime_error("Join message has an empty name.");

	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);
	return true;
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
}

float Game::frand() {
	return mt() / float(mt.max());
}

glm::vec3 Game::random_bright_color() {
	float hue = frand() * 6.0f;
	if (hue >= 6.0f) hue = 0.0f;
	float x = 1.0f - std::fabs(std::fmod(hue, 2.0f) - 1.0f);
	glm::vec3 rgb(1.0f);
	switch (int(hue)) {
		case 0: rgb = glm::vec3(1.0f, x, 0.15f); break;
		case 1: rgb = glm::vec3(x, 1.0f, 0.15f); break;
		case 2: rgb = glm::vec3(0.15f, 1.0f, x); break;
		case 3: rgb = glm::vec3(0.15f, x, 1.0f); break;
		case 4: rgb = glm::vec3(x, 0.15f, 1.0f); break;
		default: rgb = glm::vec3(1.0f, 0.15f, x); break;
	}
	return glm::mix(rgb, glm::vec3(1.0f), 0.12f);
}

float Game::speed_for(float radius) const {
	return CruiseSpeed * std::sqrt(StartRadius / std::max(radius, 0.05f));
}

float Game::radius_for(uint32_t score) const {
	return StartRadius * std::sqrt(float(score) / float(initial_score));
}

glm::vec2 Game::open_point(float radius) {
	glm::vec2 fallback(0.0f);
	for (int attempt = 0; attempt < 48; ++attempt) {
		glm::vec2 p;
		p.x = glm::mix(ArenaMin.x + radius + 0.2f, ArenaMax.x - radius - 0.2f, frand());
		p.y = glm::mix(ArenaMin.y + radius + 0.2f, ArenaMax.y - radius - 0.2f, frand());
		fallback = p;
		bool blocked = false;
		auto crowded = [&](glm::vec2 const &q, float r) {
			float gap = radius + r + 0.45f;
			return glm::length2(p - q) < gap * gap;
		};
		for (auto const &player : players) {
			if (crowded(player.position, player.radius)) blocked = true;
		}
		for (auto const &npc : npcs) {
			if (!npc.remove && crowded(npc.position, npc.radius)) blocked = true;
		}
		if (!blocked) return p;
	}
	return fallback;
}

void Game::spawn_npc() {
	Npc npc;
	float roll = frand();
	//Same size bands as before, expressed as score because radius = StartRadius * sqrt(score / initial_score).
	if (roll < 0.55f) npc.score = 4u + uint32_t(frand() * 12.0f);          //4..15
	else if (roll < 0.85f) npc.score = 20u + uint32_t(frand() * 51.0f);    //20..70
	else npc.score = 80u + uint32_t(frand() * float(NpcMaxScore - 80 + 1)); //80..NpcMaxScore
	npc.score = std::min(npc.score, NpcMaxScore);
	npc.radius = radius_for(npc.score);

	npc.color = random_bright_color();
	npc.facing = frand() * 6.2831853f;
	npc.wish = glm::vec2(std::cos(npc.facing), std::sin(npc.facing));
	npc.velocity = npc.wish * speed_for(npc.radius) * 0.65f;
	npc.think = frand() * 1.5f;
	npc.position = open_point(npc.radius);
	npcs.emplace_back(npc);
}

void Game::spawn_bait() {
	if (baits.size() >= BaitCap) return;
	Bait bait;
	float margin = 0.5f;
	bait.position.x = glm::mix(ArenaMin.x + margin, ArenaMax.x - margin, frand());
	bait.position.y = glm::mix(ArenaMin.y + margin, ArenaMax.y - margin, frand());
	bait.life = BaitLife;
	baits.emplace_back(bait);
}

void Game::populate_npcs() {
	npcs.clear();
	for (uint32_t i = 0; i < NpcCount; ++i) spawn_npc();
	baits.clear();
	for (uint32_t i = 0; i < 10; ++i) spawn_bait();
	bait_timer = BaitInterval;
}

void Game::respawn_player(Player &player) {
	player.score = initial_score;
	player.radius = radius_for(player.score);
	player.velocity = glm::vec2(0.0f);
	player.invuln = InvulnTime;
	player.dash_left = 0.0f;
	player.alive = 0.0f;
	player.downed = false;
	player.position = open_point(player.radius);
	player.facing = frand() * 6.2831853f;
}

Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	player.score = initial_score;
	player.radius = radius_for(player.score);
	player.invuln = InvulnTime;
	player.alive = 0.0f;
	player.position = open_point(player.radius);
	player.color = random_bright_color();
	player.facing = frand() * 6.2831853f;
	player.name = "Fish " + std::to_string(next_player_number++);

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {
	const float pi = 3.14159265f;

	bait_timer -= elapsed;
	if (bait_timer <= 0.0f) {
		bait_timer = BaitInterval * (0.75f + 0.5f * frand());
		spawn_bait();
	}
	for (auto &bait : baits) bait.life -= elapsed;

	auto wrap_angle = [&](float d) {
		d = std::remainder(d, 2.0f * pi);
		return d;
	};

	//Swim: ease velocity toward a wish direction, then keep the nose pointed that way.
	auto drive = [&](glm::vec2 &position, glm::vec2 &velocity, float &facing, glm::vec2 dir, float radius, float boost) {
		float max_speed = speed_for(radius) * boost;
		if (dir == glm::vec2(0.0f)) {
			float amt = 1.0f - std::pow(0.5f, elapsed / (AccelHalflife * 2.0f));
			velocity = glm::mix(velocity, glm::vec2(0.0f, 0.0f), amt);
		} else {
			dir = glm::normalize(dir);
			float amt = 1.0f - std::pow(0.5f, elapsed / AccelHalflife);
			float along = glm::dot(velocity, dir);
			if (along < max_speed) along = glm::mix(along, max_speed, amt);
			glm::vec2 side(-dir.y, dir.x);
			float perp = glm::mix(glm::dot(velocity, side), 0.0f, amt);
			velocity = dir * along + side * perp;
		}
		position += velocity * elapsed;

		if (glm::length2(velocity) > 0.0004f) {
			float target = std::atan2(velocity.y, velocity.x);
			float turn = 5.2f * std::sqrt(StartRadius / std::max(radius, 0.05f));
			float step = wrap_angle(target - facing);
			step = std::clamp(step, -turn * elapsed, turn * elapsed);
			facing += step;
		}

		if (position.x < ArenaMin.x + radius) {
			position.x = ArenaMin.x + radius;
			velocity.x = std::abs(velocity.x);
		}
		if (position.x > ArenaMax.x - radius) {
			position.x = ArenaMax.x - radius;
			velocity.x = -std::abs(velocity.x);
		}
		if (position.y < ArenaMin.y + radius) {
			position.y = ArenaMin.y + radius;
			velocity.y = std::abs(velocity.y);
		}
		if (position.y > ArenaMax.y - radius) {
			position.y = ArenaMax.y - radius;
			velocity.y = -std::abs(velocity.y);
		}
	};

	for (auto &p : players) {
		p.invuln = std::max(0.0f, p.invuln - elapsed);
		p.dash_cd = std::max(0.0f, p.dash_cd - elapsed);
		p.dash_left = std::max(0.0f, p.dash_left - elapsed);
		p.alive += elapsed;
		p.downed = false;

		if (p.controls.jump.downs > 0 && p.dash_cd <= 0.0f) {
			p.dash_left = 1.0f;
			p.dash_cd = 10.0f;
		}

		glm::vec2 dir(0.0f);
		if (p.controls.left.pressed) dir.x -= 1.0f;
		if (p.controls.right.pressed) dir.x += 1.0f;
		if (p.controls.down.pressed) dir.y -= 1.0f;
		if (p.controls.up.pressed) dir.y += 1.0f;
		if (dir == glm::vec2(0.0f) && p.dash_left > 0.0f) {
			dir = glm::vec2(std::cos(p.facing), std::sin(p.facing));
		}

		float boost = (p.dash_left > 0.0f) ? 1.8f : 1.0f;
		drive(p.position, p.velocity, p.facing, dir, p.radius, boost);

		p.controls.left.downs = 0;
		p.controls.right.downs = 0;
		p.controls.up.downs = 0;
		p.controls.down.downs = 0;
		p.controls.jump.downs = 0;
	}

	//NPCs: flee, chase, or wander. Then swim via drive().
	for (auto &n : npcs) {
		n.remove = false;
		n.think -= elapsed;

		glm::vec2 threat_dir(0.0f);
		float threat_d = 1.0e9f;
		glm::vec2 prey_dir(0.0f);
		float prey_d = 1.0e9f;

		//Nearest fish big enough to eat us, and nearest fish we can eat.
		auto consider = [&](glm::vec2 const &pos, float radius) {
			glm::vec2 d = pos - n.position;
			float dist2 = glm::length2(d);
			if (dist2 < 1.0e-6f) return;
			float dist = std::sqrt(dist2);
			if (radius > n.radius && dist < threat_d) {
				threat_d = dist;
				threat_dir = d / dist;
			} else if (n.radius > radius && dist < prey_d) {
				prey_d = dist;
				prey_dir = d / dist;
			}
		};
		for (auto const &p : players) consider(p.position, p.radius);
		for (auto const &o : npcs) {
			if (&o == &n) continue;
			consider(o.position, o.radius);
		}

		if (threat_d < 2.6f + n.radius) {
			n.wish = -threat_dir; //flee
		} else if (prey_d < 3.2f + n.radius * 2.0f) {
			n.wish = prey_dir; //chase
		} else if (n.radius < 0.35f) { //small fish also go for bait
			glm::vec2 bait_dir(0.0f);
			float bait_d = 1.0e9f;
			for (auto const &bait : baits) {
				if (bait.life <= 0.0f) continue;
				glm::vec2 d = bait.position - n.position;
				float dist2 = glm::length2(d);
				if (dist2 < 1.0e-6f || dist2 >= bait_d * bait_d) continue;
				bait_d = std::sqrt(dist2);
				bait_dir = d / bait_d;
			}
			if (bait_d < 3.6f) n.wish = bait_dir;
			else if (n.think <= 0.0f) { //no bait nearby: pick a new wander heading
				float ang = frand() * 2.0f * pi;
				n.wish = glm::vec2(std::cos(ang), std::sin(ang));
				n.think = 0.7f + frand() * 1.6f;
			}
		} else if (n.think <= 0.0f) { //larger fish just wander
			float ang = frand() * 2.0f * pi;
			n.wish = glm::vec2(std::cos(ang), std::sin(ang));
			n.think = 0.7f + frand() * 1.6f;
		}

		//Steer back into the sea when close to an edge.
		glm::vec2 wall(0.0f);
		float margin = 1.3f + n.radius;
		if (n.position.x < ArenaMin.x + margin) wall.x += 1.0f;
		if (n.position.x > ArenaMax.x - margin) wall.x -= 1.0f;
		if (n.position.y < ArenaMin.y + margin) wall.y += 1.0f;
		if (n.position.y > ArenaMax.y - margin) wall.y -= 1.0f;
		glm::vec2 wish = n.wish;
		if (wall != glm::vec2(0.0f)) wish = glm::normalize(n.wish + wall * 2.2f);

		drive(n.position, n.velocity, n.facing, wish, n.radius, 1.0f);
	}

	//Biggest fish eats first, so a middle fish can't swallow someone in the same instant it is swallowed.
	struct Body {
		glm::vec2 *pos;
		glm::vec2 *vel;
		float *radius;
		float *invuln;
		uint32_t *score;
		bool *eaten;
	};
	std::vector< Body > bodies;
	bodies.reserve(players.size() + npcs.size());
	for (auto &p : players) {
		bodies.push_back(Body{&p.position, &p.velocity, &p.radius, &p.invuln, &p.score, &p.downed});
	}
	for (auto &n : npcs) {
		bodies.push_back(Body{&n.position, &n.velocity, &n.radius, nullptr, &n.score, &n.remove});
	}
	std::sort(bodies.begin(), bodies.end(), [](Body const &a, Body const &b){
		return *a.radius > *b.radius;
	});

	for (size_t i = 0; i < bodies.size(); ++i) {
		if (*bodies[i].eaten) continue;
		for (size_t j = 0; j < bodies.size(); ++j) {
			if (i == j || *bodies[j].eaten) continue;
			float ri = *bodies[i].radius;
			float rj = *bodies[j].radius;
			if (ri <= rj) continue; //any strictly larger fish can eat
			if (bodies[j].invuln && *bodies[j].invuln > 0.0f) continue;
			glm::vec2 d = *bodies[j].pos - *bodies[i].pos;
			float reach = (ri + rj) * 0.72f;
			if (glm::length2(d) > reach * reach) continue;
			*bodies[i].score += *bodies[j].score;
			//NPCs have no invuln pointer; keep them from snowballing forever.
			if (!bodies[i].invuln) {
				*bodies[i].score = std::min(*bodies[i].score, NpcMaxScore);
			}
			*bodies[i].radius = radius_for(*bodies[i].score);
			*bodies[i].vel *= 0.82f;
			*bodies[j].eaten = true;
		}
	}

	//Soft-separate fish that can't eat each other (same/near size), so NPCs don't stack.
	for (size_t i = 0; i < bodies.size(); ++i) {
		if (*bodies[i].eaten) continue;
		for (size_t j = i + 1; j < bodies.size(); ++j) {
			if (*bodies[j].eaten) continue;
			float ri = *bodies[i].radius;
			float rj = *bodies[j].radius;
			if (ri > rj || rj > ri) continue; //strictly larger may overlap to eat
			glm::vec2 d = *bodies[i].pos - *bodies[j].pos;
			float dist2 = glm::length2(d);
			float min_sep = ri + rj;
			if (dist2 >= min_sep * min_sep) continue;
			float dist = std::sqrt(std::max(dist2, 1e-8f));
			glm::vec2 n = d / dist;
			float push = 0.5f * (min_sep - dist);
			*bodies[i].pos += n * push;
			*bodies[j].pos -= n * push;
		}
	}
	auto clamp_body = [&](Body &b) {
		float r = *b.radius;
		b.pos->x = std::clamp(b.pos->x, ArenaMin.x + r, ArenaMax.x - r);
		b.pos->y = std::clamp(b.pos->y, ArenaMin.y + r, ArenaMax.y - r);
	};
	for (auto &b : bodies) {
		if (!*b.eaten) clamp_body(b);
	}

	auto try_bait = [&](glm::vec2 const &pos, uint32_t &score, float &radius, bool skip, bool is_npc) {
		if (skip) return;
		for (auto &bait : baits) {
			if (bait.eaten || bait.life <= 0.0f) continue;
			float reach = radius * 0.9f + BaitRadius;
			if (glm::length2(pos - bait.position) > reach * reach) continue;
			score += BaitScore;
			if (is_npc) score = std::min(score, NpcMaxScore);
			radius = radius_for(score);
			bait.eaten = true;
		}
	};
	for (auto &p : players) try_bait(p.position, p.score, p.radius, p.downed, false);
	for (auto &n : npcs) try_bait(n.position, n.score, n.radius, n.remove, true);

	for (auto &p : players) {
		if (p.downed) respawn_player(p);
	}
	npcs.erase(std::remove_if(npcs.begin(), npcs.end(), [](Npc const &n){ return n.remove; }), npcs.end());
	while (npcs.size() < NpcCount) spawn_npc();
	baits.erase(std::remove_if(baits.begin(), baits.end(), [](Bait const &b){ return b.eaten || b.life <= 0.0f; }), baits.end());
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	//send player info helper:
	auto send_player = [&](Player const &player) {
		connection.send(player.position);
		connection.send(player.velocity);
		connection.send(player.color);
		connection.send(player.radius);
		connection.send(player.facing);
		connection.send(player.invuln);
		connection.send(player.dash_left);
		connection.send(player.dash_cd);
		connection.send(player.alive);
		connection.send(player.score);

		//NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		//effectively: truncates player name to 255 chars
		uint8_t len = uint8_t(std::min< size_t >(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	auto send_npc = [&](Npc const &npc) {
		connection.send(npc.position);
		connection.send(npc.velocity);
		connection.send(npc.color);
		connection.send(npc.radius);
		connection.send(npc.facing);
		connection.send(npc.score);
	};
	connection.send(uint8_t(npcs.size()));
	for (auto const &npc : npcs) send_npc(npc);

	connection.send(uint8_t(baits.size()));
	for (auto const &bait : baits) {
		connection.send(bait.position);
		connection.send(bait.life);
	}

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.position);
		read(&player.velocity);
		read(&player.color);
		read(&player.radius);
		read(&player.facing);
		read(&player.invuln);
		read(&player.dash_left);
		read(&player.dash_cd);
		read(&player.alive);
		read(&player.score);
		uint8_t name_len;
		read(&name_len);
		//n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		player.name = "";
		for (uint8_t n = 0; n < name_len; ++n) {
			char c;
			read(&c);
			player.name += c;
		}
	}

	npcs.clear();
	uint8_t npc_count;
	read(&npc_count);
	for (uint8_t i = 0; i < npc_count; ++i) {
		npcs.emplace_back();
		Npc &npc = npcs.back();
		read(&npc.position);
		read(&npc.velocity);
		read(&npc.color);
		read(&npc.radius);
		read(&npc.facing);
		read(&npc.score);
	}

	baits.clear();
	uint8_t bait_count;
	read(&bait_count);
	for (uint8_t i = 0; i < bait_count; ++i) {
		baits.emplace_back();
		Bait &bait = baits.back();
		read(&bait.position);
		read(&bait.life);
	}

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}
