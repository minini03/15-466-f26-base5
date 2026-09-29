#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <vector>
#include <random>

struct Connection;

inline constexpr uint32_t initial_score = 20; //starting score; radius scales with sqrt(score)

//Game state, separate from rendering.
//Clients send controls. The server simulates the sea and sends the whole state back.

enum class Message : uint8_t {
	C2S_Controls = 1,
	C2S_Join = 2, //name and rgb, sent once when a player connects
	S2C_State = 's',
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		Button left, right, up, down, jump;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	//sent once on connect; server copies them onto this player:
	void send_join_message(Connection *connection) const;
	bool recv_join_message(Connection *connection);

	//player state (sent from server):
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	float radius = 0.16f; //always radius_for(score); kept so collision and drawing can read it
	float facing = 0.0f; //radians, 0 faces +x
	float invuln = 0.0f; //seconds left before this fish can be eaten
	float dash_left = 0.0f; //seconds of dash remaining; 0 means not dashing
	float dash_cd = 0.0f; //seconds until dash can be used again
	float alive = 0.0f; //seconds survived since last spawn/respawn
	uint32_t score = initial_score; //bait is +5; eating a fish adds that fish's score
	std::string name = "";

	//server-only (not serialized):
	bool downed = false;
};

//Computer-controlled fish. Clients only receive the serialized fields.
struct Npc {
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	float radius = 0.16f; //radius_for(score)
	float facing = 0.0f;
	uint32_t score = initial_score;

	//server-only:
	glm::vec2 wish = glm::vec2(1.0f, 0.0f);
	float think = 0.0f;
	bool remove = false;
};

//Food pellet. Spawned by the server and eaten by whichever fish reaches it.
struct Bait {
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	float life = 14.0f;
	bool eaten = false; //server-only
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	std::vector< Npc > npcs;
	std::vector< Bait > baits;

	Player *spawn_player(); //add player at the end of the players list
	void remove_player(Player *); //remove player from game
	void populate_npcs(); //fill the sea with computer fish (server only)

	std::mt19937 mt; //used for spawning
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//sea size (matches sea.blend wall inner opening ±12.5):
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-12.5f, -12.5f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 12.5f,  12.5f);

	inline static constexpr float StartRadius = 0.16f;
	inline static constexpr float MaxRadius = 1.15f;
	inline static constexpr float CruiseSpeed = 1.3f;
	inline static constexpr float AccelHalflife = 0.18f;
	inline static constexpr float InvulnTime = 3.0f;
	inline static constexpr uint32_t NpcCount = 26;
	inline static constexpr uint32_t NpcMaxScore = 100; //NPCs stop growing past this score

	inline static constexpr float BaitInterval = 1.0f;
	inline static constexpr float BaitLife = 14.0f;
	inline static constexpr float BaitRadius = 0.04f;
	inline static constexpr uint32_t BaitScore = 5;
	inline static constexpr uint32_t BaitCap = 25;

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;

	private:
	float bait_timer = 0.4f; //server-only; first pellet arrives quickly

	float frand();
	glm::vec3 random_bright_color();
	glm::vec2 open_point(float radius);
	float speed_for(float radius) const;
	float radius_for(uint32_t score) const;
	void spawn_npc();
	void spawn_bait();
	void respawn_player(Player &player);
};
