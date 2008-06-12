/* copyright (c) 2007 magnus auvinen, see licence.txt for more info */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <engine/e_config.h>
#include <engine/e_server_interface.h>
#include <game/g_version.h>
#include <game/g_collision.h>
#include <game/g_layers.h>
#include <game/g_math.h>
#include "gs_common.h"
#include "gs_game_ctf.h"
#include "gs_game_tdm.h"
#include "gs_game_dm.h"

TUNING_PARAMS tuning;

void create_damageind(vec2 p, float angle_mod, int amount);
void create_explosion(vec2 p, int owner, int weapon, bool bnodamage);
void create_smoke(vec2 p);
void create_playerspawn(vec2 p);
void create_death(vec2 p, int who);
void create_sound(vec2 pos, int sound, int mask=-1);

PLAYER *get_player(int index);
class PLAYER *intersect_player(vec2 pos0, vec2 pos1, float radius, vec2 &new_pos, class ENTITY *notthis = 0);
class PLAYER *closest_player(vec2 pos, float radius, ENTITY *notthis);

GAMEWORLD *world;

enum
{
	CHAT_ALL=-2,
	CHAT_SPEC=-1,
	CHAT_RED=0,
	CHAT_BLUE=1
};

static void send_chat(int cid, int team, const char *text)
{
	if(cid >= 0 && cid < MAX_CLIENTS)
		dbg_msg("chat", "%d:%d:%s: %s", cid, team, server_clientname(cid), text);
	else
		dbg_msg("chat", "*** %s", text);

	if(team == CHAT_ALL)
	{
		NETMSG_SV_CHAT msg;
		msg.team = 0;
		msg.cid = cid;
		msg.message = text;
		msg.pack(MSGFLAG_VITAL);
		server_send_msg(-1);
	}
	else
	{
		NETMSG_SV_CHAT msg;
		msg.team = 1;
		msg.cid = cid;
		msg.message = text;
		msg.pack(MSGFLAG_VITAL);

		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(players[i].client_id != -1 && players[i].team == team)
				server_send_msg(i);
		}
	}
}


void send_info(int who, int to_who)
{
	NETMSG_SV_SETINFO msg;
	msg.cid = who;
	msg.name = server_clientname(who);
	msg.skin = players[who].skin_name;
	msg.use_custom_color = players[who].use_custom_color;
	msg.color_body = players[who].color_body;
	msg.color_feet =players[who].color_feet;
	msg.pack(MSGFLAG_VITAL);
	
	server_send_msg(to_who);
}

void send_emoticon(int cid, int emoticon)
{
	NETMSG_SV_EMOTICON msg;
	msg.cid = cid;
	msg.emoticon = emoticon;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(-1);
}

void send_weapon_pickup(int cid, int weapon)
{
	NETMSG_SV_WEAPONPICKUP msg;
	msg.weapon = weapon;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
}


void send_broadcast(const char *text, int cid)
{
	NETMSG_SV_BROADCAST msg;
	msg.message = text;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(cid);
}

void send_tuning_params(int cid)
{
	/*
	msg_pack_start(NETMSGTYPE_SV_TUNE_PARAMS, MSGFLAG_VITAL);
	int *params = (int *)&tuning;
	for(unsigned i = 0; i < sizeof(tuning_params)/sizeof(int); i++)
		msg_pack_int(params[i]);
	msg_pack_end();
	server_send_msg(cid);
	*/
}

//////////////////////////////////////////////////
// Event handler
//////////////////////////////////////////////////
EVENT_HANDLER::EVENT_HANDLER()
{
	clear();
}

void *EVENT_HANDLER::create(int type, int size, int mask)
{
	if(num_events == MAX_EVENTS)
		return 0;
	if(current_offset+size >= MAX_DATASIZE)
		return 0;

	void *p = &data[current_offset];
	offsets[num_events] = current_offset;
	types[num_events] = type;
	sizes[num_events] = size;
	client_masks[num_events] = mask;
	current_offset += size;
	num_events++;
	return p;
}

void EVENT_HANDLER::clear()
{
	num_events = 0;
	current_offset = 0;
}

void EVENT_HANDLER::snap(int snapping_client)
{
	for(int i = 0; i < num_events; i++)
	{
		if(cmask_is_set(client_masks[i], snapping_client))
		{
			NETEVENT_COMMON *ev = (NETEVENT_COMMON *)&data[offsets[i]];
			if(distance(players[snapping_client].pos, vec2(ev->x, ev->y)) < 1500.0f)
			{
				void *d = snap_new_item(types[i], i, sizes[i]);
				mem_copy(d, &data[offsets[i]], sizes[i]);
			}
		}
	}
}

EVENT_HANDLER events;

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
ENTITY::ENTITY(int objtype)
{
	this->objtype = objtype;
	pos = vec2(0,0);
	flags = FLAG_PHYSICS;
	proximity_radius = 0;

	id = snap_new_id();

	next_entity = 0;
	prev_entity = 0;
	prev_type_entity = 0;
	next_type_entity = 0;
}

ENTITY::~ENTITY()
{
	world->remove_entity(this);
	snap_free_id(id);
}

//////////////////////////////////////////////////
// game world
//////////////////////////////////////////////////
GAMEWORLD::GAMEWORLD()
{
	paused = false;
	reset_requested = false;
	first_entity = 0x0;
	for(int i = 0; i < NUM_ENT_TYPES; i++)
		first_entity_types[i] = 0;
}

GAMEWORLD::~GAMEWORLD()
{
	// delete all entities
	while(first_entity)
		delete first_entity;
}

int GAMEWORLD::find_entities(vec2 pos, float radius, ENTITY **ents, int max)
{
	int num = 0;
	for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
	{
		if(!(ent->flags&ENTITY::FLAG_PHYSICS))
			continue;

		if(distance(ent->pos, pos) < radius+ent->proximity_radius)
		{
			ents[num] = ent;
			num++;
			if(num == max)
				break;
		}
	}

	return num;
}

int GAMEWORLD::find_entities(vec2 pos, float radius, ENTITY **ents, int max, const int* types, int maxtypes)
{
	int num = 0;
	for(int t = 0; t < maxtypes; t++)
	{
		for(ENTITY *ent = first_entity_types[types[t]]; ent; ent = ent->next_type_entity)
		{
			if(!(ent->flags&ENTITY::FLAG_PHYSICS))
				continue;

			if(distance(ent->pos, pos) < radius+ent->proximity_radius)
			{
				ents[num] = ent;
				num++;
				if(num == max)
					break;
			}
		}
	}

	return num;
}

void GAMEWORLD::insert_entity(ENTITY *ent)
{
	ENTITY *cur = first_entity;
	while(cur)
	{
		dbg_assert(cur != ent, "err");
		cur = cur->next_entity;
	}

	// insert it
	if(first_entity)
		first_entity->prev_entity = ent;
	ent->next_entity = first_entity;
	ent->prev_entity = 0x0;
	first_entity = ent;

	// into typelist aswell
	if(first_entity_types[ent->objtype])
		first_entity_types[ent->objtype]->prev_type_entity = ent;
	ent->next_type_entity = first_entity_types[ent->objtype];
	ent->prev_type_entity = 0x0;
	first_entity_types[ent->objtype] = ent;
}

void GAMEWORLD::destroy_entity(ENTITY *ent)
{
	ent->set_flag(ENTITY::FLAG_DESTROY);
}

void GAMEWORLD::remove_entity(ENTITY *ent)
{
	// not in the list
	if(!ent->next_entity && !ent->prev_entity && first_entity != ent)
		return;

	// remove
	if(ent->prev_entity)
		ent->prev_entity->next_entity = ent->next_entity;
	else
		first_entity = ent->next_entity;
	if(ent->next_entity)
		ent->next_entity->prev_entity = ent->prev_entity;

	if(ent->prev_type_entity)
		ent->prev_type_entity->next_type_entity = ent->next_type_entity;
	else
		first_entity_types[ent->objtype] = ent->next_type_entity;
	if(ent->next_type_entity)
		ent->next_type_entity->prev_type_entity = ent->prev_type_entity;

	ent->next_entity = 0;
	ent->prev_entity = 0;
	ent->next_type_entity = 0;
	ent->prev_type_entity = 0;
}

//
void GAMEWORLD::snap(int snapping_client)
{
	for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
		ent->snap(snapping_client);
}

void GAMEWORLD::reset()
{
	// reset all entities
	for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
		ent->reset();
	remove_entities();

	for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
		ent->post_reset();
	remove_entities();

	reset_requested = false;
}

void GAMEWORLD::remove_entities()
{
	// destroy objects marked for destruction
	ENTITY *ent = first_entity;
	while(ent)
	{
		ENTITY *next = ent->next_entity;
		if(ent->flags&ENTITY::FLAG_DESTROY)
		{
			remove_entity(ent);
			ent->destroy();
		}
		ent = next;
	}
}

void GAMEWORLD::tick()
{
	if(reset_requested)
		reset();

	if(!paused)
	{
		/*
		static PERFORMACE_INFO scopes[OBJTYPE_FLAG+1] =
		{
			{"null", 0},
			{"game", 0},
			{"player_info", 0},
			{"player_character", 0},
			{"projectile", 0},
			{"powerup", 0},
			{"flag", 0}
		};

		static PERFORMACE_INFO scopes_def[OBJTYPE_FLAG+1] =
		{
			{"null", 0},
			{"game", 0},
			{"player_info", 0},
			{"player_character", 0},
			{"projectile", 0},
			{"powerup", 0},
			{"flag", 0}
		};
				
		static PERFORMACE_INFO tick_scope = {"tick", 0};
		perf_start(&tick_scope);*/
		
		// update all objects
		for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
		{
			/*if(ent->objtype >= 0 && ent->objtype < OBJTYPE_FLAG)
				perf_start(&scopes[ent->objtype]);*/
			ent->tick();
			/*if(ent->objtype >= 0 && ent->objtype < OBJTYPE_FLAG)
				perf_end();*/
		}
		
		/*
		perf_end();

		static PERFORMACE_INFO deftick_scope = {"tick_defered", 0};
		perf_start(&deftick_scope);*/
		for(ENTITY *ent = first_entity; ent; ent = ent->next_entity)
		{
			/*if(ent->objtype >= 0 && ent->objtype < OBJTYPE_FLAG)
				perf_start(&scopes_def[ent->objtype]);*/
			ent->tick_defered();
			/*if(ent->objtype >= 0 && ent->objtype < OBJTYPE_FLAG)
				perf_end();*/
		}
		/*perf_end();*/
	}

	remove_entities();
}

struct INPUT_COUNT
{
	int presses;
	int releases;
};

static INPUT_COUNT count_input(int prev, int cur)
{
	INPUT_COUNT c = {0,0};
	prev &= INPUT_STATE_MASK;
	cur &= INPUT_STATE_MASK;
	int i = prev;
	while(i != cur)
	{
		i = (i+1)&INPUT_STATE_MASK;
		if(i&1)
			c.presses++;
		else
			c.releases++;
	}

	return c;
}


//////////////////////////////////////////////////
// projectile
//////////////////////////////////////////////////
PROJECTILE::PROJECTILE(int type, int owner, vec2 pos, vec2 dir, int span, ENTITY* powner,
	int damage, int flags, float force, int sound_impact, int weapon)
: ENTITY(NETOBJTYPE_PROJECTILE)
{
	this->type = type;
	this->pos = pos;
	this->direction = dir;
	this->lifespan = span;
	this->owner = owner;
	this->powner = powner;
	this->flags = flags;
	this->force = force;
	this->damage = damage;
	this->sound_impact = sound_impact;
	this->weapon = weapon;
	this->bounce = 0;
	this->start_tick = server_tick();
	world->insert_entity(this);
}

void PROJECTILE::reset()
{
	world->destroy_entity(this);
}

vec2 PROJECTILE::get_pos(float time)
{
	float curvature = 0;
	float speed = 0;
	if(type == WEAPON_GRENADE)
	{
		curvature = tuning.grenade_curvature;
		speed = tuning.grenade_speed;
	}
	else if(type == WEAPON_SHOTGUN)
	{
		curvature = tuning.shotgun_curvature;
		speed = tuning.shotgun_speed;
	}
	else if(type == WEAPON_GUN)
	{
		curvature = tuning.gun_curvature;
		speed = tuning.gun_speed;
	}
	
	return calc_pos(pos, direction, curvature, speed, time);
}


void PROJECTILE::tick()
{
	
	float pt = (server_tick()-start_tick-1)/(float)server_tickspeed();
	float ct = (server_tick()-start_tick)/(float)server_tickspeed();
	vec2 prevpos = get_pos(pt);
	vec2 curpos = get_pos(ct);

	lifespan--;
	
	int collide = col_intersect_line(prevpos, curpos, &curpos);
	//int collide = col_check_point((int)curpos.x, (int)curpos.y);
	
	ENTITY *targetplayer = (ENTITY*)intersect_player(prevpos, curpos, 6.0f, curpos, powner);
	if(targetplayer || collide || lifespan < 0)
	{
		if (lifespan >= 0 || weapon == WEAPON_GRENADE)
			create_sound(curpos, sound_impact);

		if (flags & PROJECTILE_FLAGS_EXPLODE)
			create_explosion(curpos, owner, weapon, false);
		else if (targetplayer)
		{
			targetplayer->take_damage(direction * max(0.001f, force), damage, owner, weapon);
		}

		world->destroy_entity(this);
	}
}

void PROJECTILE::fill_info(NETOBJ_PROJECTILE *proj)
{
	proj->x = (int)pos.x;
	proj->y = (int)pos.y;
	proj->vx = (int)(direction.x*100.0f);
	proj->vy = (int)(direction.y*100.0f);
	proj->start_tick = start_tick;
	proj->type = type;
}

void PROJECTILE::snap(int snapping_client)
{
	float ct = (server_tick()-start_tick)/(float)server_tickspeed();
	
	if(distance(players[snapping_client].pos, get_pos(ct)) > 1000.0f)
		return;

	NETOBJ_PROJECTILE *proj = (NETOBJ_PROJECTILE *)snap_new_item(NETOBJTYPE_PROJECTILE, id, sizeof(NETOBJ_PROJECTILE));
	fill_info(proj);
}


//////////////////////////////////////////////////
// laser
//////////////////////////////////////////////////
LASER::LASER(vec2 pos, vec2 direction, float start_energy, PLAYER *owner)
: ENTITY(NETOBJTYPE_LASER)
{
	this->pos = pos;
	this->owner = owner;
	energy = start_energy;
	dir = direction;
	bounces = 0;
	do_bounce();
	
	world->insert_entity(this);
}


bool LASER::hit_player(vec2 from, vec2 to)
{
	vec2 at;
	PLAYER *hit = intersect_player(pos, to, 0.0f, at, owner);
	if(!hit)
		return false;

	this->from = from;
	pos = at;
	energy = -1;		
	hit->take_damage(vec2(0,0), tuning.laser_damage, owner->client_id, WEAPON_RIFLE);
	return true;
}

void LASER::do_bounce()
{
	eval_tick = server_tick();
	
	if(energy < 0)
	{
		//dbg_msg("laser", "%d removed", server_tick());
		world->destroy_entity(this);
		return;
	}
	
	vec2 to = pos + dir*energy;
	
	if(col_intersect_line(pos, to, &to))
	{
		if(!hit_player(pos, to))
		{
			// intersected
			from = pos;
			pos = to - dir*2;
			vec2 temp_pos = pos;
			vec2 temp_dir = dir*4.0f;
			
			move_point(&temp_pos, &temp_dir, 1.0f, 0);
			pos = temp_pos;
			dir = normalize(temp_dir);
			
			energy -= distance(from, pos) + tuning.laser_bounce_cost;
			bounces++;
			
			if(bounces > tuning.laser_bounce_num)
				energy = -1;
				
			create_sound(pos, SOUND_RIFLE_BOUNCE);
		}
	}
	else
	{
		if(!hit_player(pos, to))
		{
			from = pos;
			pos = to;
			energy = -1;
		}
	}
		
	//dbg_msg("laser", "%d done %f %f %f %f", server_tick(), from.x, from.y, pos.x, pos.y);
}
	
void LASER::reset()
{
	world->destroy_entity(this);
}

void LASER::tick()
{
	if(server_tick() > eval_tick+(server_tickspeed()*tuning.laser_bounce_delay)/1000.0f)
	{
		do_bounce();
	}

}

void LASER::snap(int snapping_client)
{
	if(distance(players[snapping_client].pos, pos) > 1000.0f)
		return;

	NETOBJ_LASER *obj = (NETOBJ_LASER *)snap_new_item(NETOBJTYPE_LASER, id, sizeof(NETOBJ_LASER));
	obj->x = (int)pos.x;
	obj->y = (int)pos.y;
	obj->from_x = (int)from.x;
	obj->from_y = (int)from.y;
	obj->start_tick = eval_tick;
}


//////////////////////////////////////////////////
// player
//////////////////////////////////////////////////
// TODO: move to separate file
PLAYER::PLAYER()
: ENTITY(NETOBJTYPE_PLAYER_CHARACTER)
{
	init();
}

void PLAYER::init()
{
	proximity_radius = phys_size;
	client_id = -1;
	team = -1; // -1 == spectator

	latency_accum = 0;
	latency_accum_min = 0;
	latency_accum_max = 0;
	latency_avg = 0;
	latency_min = 0;
	latency_max = 0;

	reset();
}

void PLAYER::reset()
{
	pos = vec2(0.0f, 0.0f);
	core.reset();
	
	emote_type = 0;
	emote_stop = -1;
	
	//direction = vec2(0.0f, 1.0f);
	score = 0;
	dead = true;
	clear_flag(ENTITY::FLAG_PHYSICS);
	spawning = false;
	die_tick = 0;
	die_pos = vec2(0,0);
	damage_taken = 0;
	last_chat = 0;
	player_state = PLAYERSTATE_UNKNOWN;

	mem_zero(&input, sizeof(input));
	mem_zero(&previnput, sizeof(previnput));
	num_inputs = 0;

	last_action = -1;

	emote_stop = 0;
	damage_taken_tick = 0;
	attack_tick = 0;

	mem_zero(&ninja, sizeof(ninja));
	
	active_weapon = WEAPON_GUN;
	last_weapon = WEAPON_HAMMER;
	queued_weapon = -1;
}

void PLAYER::destroy() {  }

void PLAYER::set_weapon(int w)
{
	if(w == active_weapon)
		return;
		
	last_weapon = active_weapon;
	queued_weapon = -1;
	active_weapon = w;
	if(active_weapon < 0 || active_weapon >= NUM_WEAPONS)
		active_weapon = 0;
		
	create_sound(pos, SOUND_WEAPON_SWITCH);
}

void PLAYER::respawn()
{
	spawning = true;
}

const char *get_team_name(int team)
{
	if(gamecontroller->gametype == GAMETYPE_DM)
	{
		if(team == 0)
			return "game";
	}
	else
	{
		if(team == 0)
			return "red team";
		else if(team == 1)
			return "blue team";
	}
	
	return "spectators";
}

void PLAYER::set_team(int new_team)
{
	// clamp the team
	new_team = gamecontroller->clampteam(new_team);
	if(team == new_team)
		return;
		
	char buf[512];
	str_format(buf, sizeof(buf), "%s joined the %s", server_clientname(client_id), get_team_name(new_team));
	send_chat(-1, CHAT_ALL, buf); 
	
	die(client_id, -1);
	team = new_team;
	score = 0;
	dbg_msg("game", "team_join player='%d:%s' team=%d", client_id, server_clientname(client_id), team);
	
	gamecontroller->on_player_info_change(&players[client_id]);

	// send all info to this client
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i].client_id != -1)
			send_info(i, -1);
	}
}

vec2 spawn_points[3][64];
int num_spawn_points[3] = {0};

struct SPAWNEVAL
{
	SPAWNEVAL()
	{
		got = false;
		friendly_team = -1;
		die_pos = vec2(0,0);
		pos = vec2(100,100);
	}
		
	vec2 pos;
	bool got;
	int friendly_team;
	float score;
	vec2 die_pos;
};

static float evaluate_spawn(SPAWNEVAL *eval, vec2 pos)
{
	float score = 0.0f;
	
	for(int c = 0; c < MAX_CLIENTS; c++)
	{
		if(players[c].client_id == -1)
			continue;
			
		// don't count dead people
		if(!(players[c].flags&ENTITY::FLAG_PHYSICS))
			continue;
		
		// team mates are not as dangerous as enemies
		float scoremod = 1.0f;
		if(eval->friendly_team != -1 && players[c].team == eval->friendly_team)
			scoremod = 0.5f;
			
		float d = distance(pos, players[c].pos);
		if(d == 0)
			score += 1000000000.0f;
		else
			score += 1.0f/d;
	}
	
	// weight in the die posititon
	float d = distance(pos, eval->die_pos);
	if(d == 0)
		score += 1000000000.0f;
	else
		score += 1.0f/d;
	
	return score;
}

static void evaluate_spawn_type(SPAWNEVAL *eval, int t)
{
	// get spawn point
	/*
	int start, num;
	map_get_type(t, &start, &num);
	if(!num)
		return;
	*/
	for(int i  = 0; i < num_spawn_points[t]; i++)
	{
		//num_spawn_points[t]
		//mapres_spawnpoint *sp = (mapres_spawnpoint*)map_get_item(start + i, NULL, NULL);
		vec2 p = spawn_points[t][i];// vec2((float)sp->x, (float)sp->y);
		float s = evaluate_spawn(eval, p);
		if(!eval->got || eval->score > s)
		{
			eval->got = true;
			eval->score = s;
			eval->pos = p;
		}
	}
}

void PLAYER::try_respawn()
{
	vec2 spawnpos = vec2(100.0f, -60.0f);
	
	// get spawn point
	SPAWNEVAL eval;
	eval.die_pos = die_pos;
	
	eval.pos = vec2(100, 100);
	
	if(gamecontroller->gametype == GAMETYPE_CTF)
	{
		eval.friendly_team = team;
		
		// try first try own team spawn, then normal spawn and then enemy
		evaluate_spawn_type(&eval, 1+(team&1));
		if(!eval.got)
		{
			evaluate_spawn_type(&eval, 0);
			if(!eval.got)
				evaluate_spawn_type(&eval, 1+((team+1)&1));
		}
	}
	else
	{
		if(gamecontroller->gametype == GAMETYPE_TDM)
			eval.friendly_team = team;
			
		evaluate_spawn_type(&eval, 0);
		evaluate_spawn_type(&eval, 1);
		evaluate_spawn_type(&eval, 2);
	}
	
	spawnpos = eval.pos;

	// check if the position is occupado
	ENTITY *ents[2] = {0};
	int types[] = {NETOBJTYPE_PLAYER_CHARACTER};
	int num_ents = world->find_entities(spawnpos, 64, ents, 2, types, 1);
	for(int i = 0; i < num_ents; i++)
	{
		if(ents[i] != this)
			return;
	}
	
	spawning = false;
	pos = spawnpos;

	core.pos = pos;
	core.vel = vec2(0,0);
	core.hooked_player = -1;

	health = 10;
	armor = 0;
	jumped = 0;
	
	mem_zero(&ninja, sizeof(ninja));
	
	dead = false;
	set_flag(ENTITY::FLAG_PHYSICS);
	player_state = PLAYERSTATE_PLAYING;

	core.hook_state = HOOK_IDLE;

	mem_zero(&input, sizeof(input));

	// init weapons
	mem_zero(&weapons, sizeof(weapons));
	weapons[WEAPON_HAMMER].got = true;
	weapons[WEAPON_HAMMER].ammo = -1;
	weapons[WEAPON_GUN].got = true;
	weapons[WEAPON_GUN].ammo = 10;

	/*weapons[WEAPON_RIFLE].got = true;
	weapons[WEAPON_RIFLE].ammo = -1;*/
	
	active_weapon = WEAPON_GUN;
	last_weapon = WEAPON_HAMMER;
	queued_weapon = 0;

	reload_timer = 0;

	// Create sound and spawn effects
	create_sound(pos, SOUND_PLAYER_SPAWN);
	create_playerspawn(pos);

	gamecontroller->on_player_spawn(this);
}

bool PLAYER::is_grounded()
{
	if(col_check_point((int)(pos.x+phys_size/2), (int)(pos.y+phys_size/2+5)))
		return true;
	if(col_check_point((int)(pos.x-phys_size/2), (int)(pos.y+phys_size/2+5)))
		return true;
	return false;
}


int PLAYER::handle_ninja()
{
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));

	if ((server_tick() - ninja.activationtick) > (data->weapons.ninja.duration * server_tickspeed() / 1000))
	{
		// time's up, return
		weapons[WEAPON_NINJA].got = false;
		active_weapon = last_weapon;
		if(active_weapon == WEAPON_NINJA)
			active_weapon = WEAPON_GUN;
		set_weapon(active_weapon);
		return 0;
	}
	
	// force ninja weapon
	set_weapon(WEAPON_NINJA);

	// Check if it should activate
	if (count_input(latest_previnput.fire, latest_input.fire).presses && (server_tick() > ninja.currentcooldown))
	{
		// ok then, activate ninja
		attack_tick = server_tick();
		ninja.activationdir = direction;
		ninja.currentmovetime = data->weapons.ninja.movetime * server_tickspeed() / 1000;
		ninja.currentcooldown = data->weapons.ninja.base->firedelay * server_tickspeed() / 1000 + server_tick();
		
		// reset hit objects
		numobjectshit = 0;

		create_sound(pos, SOUND_NINJA_FIRE);

		// release all hooks when ninja is activated
		//release_hooked();
		//release_hooks();
	}

	ninja.currentmovetime--;

	if (ninja.currentmovetime == 0)
	{
		// reset player velocity
		core.vel *= 0.2f;
		//return MODIFIER_RETURNFLAGS_OVERRIDEWEAPON;
	}

	if (ninja.currentmovetime > 0)
	{
		// Set player velocity
		core.vel = ninja.activationdir * data->weapons.ninja.velocity;
		vec2 oldpos = pos;
		move_box(&core.pos, &core.vel, vec2(phys_size, phys_size), 0.0f);
		// reset velocity so the client doesn't predict stuff
		core.vel = vec2(0.0f,0.0f);
		if ((ninja.currentmovetime % 2) == 0)
		{
			//create_smoke(pos);
		}

		// check if we hit anything along the way
		{
			int type = NETOBJTYPE_PLAYER_CHARACTER;
			ENTITY *ents[64];
			vec2 dir = pos - oldpos;
			float radius = phys_size * 2.0f; //length(dir * 0.5f);
			vec2 center = oldpos + dir * 0.5f;
			int num = world->find_entities(center, radius, ents, 64, &type, 1);

			for (int i = 0; i < num; i++)
			{
				// Check if entity is a player
				if (ents[i] == this)
					continue;
				// make sure we haven't hit this object before
				bool balreadyhit = false;
				for (int j = 0; j < numobjectshit; j++)
				{
					if (hitobjects[j] == ents[i])
						balreadyhit = true;
				}
				if (balreadyhit)
					continue;

				// check so we are sufficiently close
				if (distance(ents[i]->pos, pos) > (phys_size * 2.0f))
					continue;

				// hit a player, give him damage and stuffs...
				create_sound(ents[i]->pos, SOUND_NINJA_HIT);
				// set his velocity to fast upward (for now)
				if(numobjectshit < 10)
					hitobjects[numobjectshit++] = ents[i];
				ents[i]->take_damage(vec2(0,10.0f), data->weapons.ninja.base->damage, client_id,WEAPON_NINJA);
			}
		}
		return 0;
	}

	return 0;
}


void PLAYER::do_weaponswitch()
{
	if(reload_timer != 0) // make sure we have reloaded
		return;
		
	if(queued_weapon == -1) // check for a queued weapon
		return;

	if(weapons[WEAPON_NINJA].got) // if we have ninja, no weapon selection is possible
		return;

	// switch weapon
	set_weapon(queued_weapon);
}

void PLAYER::handle_weaponswitch()
{
	int wanted_weapon = active_weapon;
	if(queued_weapon != -1)
		wanted_weapon = queued_weapon;
	
	// select weapon
	int next = count_input(latest_previnput.next_weapon, latest_input.next_weapon).presses;
	int prev = count_input(latest_previnput.prev_weapon, latest_input.prev_weapon).presses;

	if(next < 128) // make sure we only try sane stuff
	{
		while(next) // next weapon selection
		{
			wanted_weapon = (wanted_weapon+1)%NUM_WEAPONS;
			if(weapons[wanted_weapon].got)
				next--;
		}
	}

	if(prev < 128) // make sure we only try sane stuff
	{
		while(prev) // prev weapon selection
		{
			wanted_weapon = (wanted_weapon-1)<0?NUM_WEAPONS-1:wanted_weapon-1;
			if(weapons[wanted_weapon].got)
				prev--;
		}
	}

	// direct weapon selection
	if(latest_input.wanted_weapon)
		wanted_weapon = input.wanted_weapon-1;

	// check for insane values
	if(wanted_weapon >= 0 && wanted_weapon < NUM_WEAPONS && wanted_weapon != active_weapon && weapons[wanted_weapon].got)
		queued_weapon = wanted_weapon;
	
	do_weaponswitch();
}

void PLAYER::fire_weapon()
{
	if(reload_timer != 0 || active_weapon == WEAPON_NINJA)
		return;
		
	do_weaponswitch();
	
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));
	
	bool fullauto = false;
	if(active_weapon == WEAPON_GRENADE || active_weapon == WEAPON_SHOTGUN || active_weapon == WEAPON_RIFLE)
		fullauto = true;


	// check if we gonna fire
	bool will_fire = false;
	if(count_input(latest_previnput.fire, latest_input.fire).presses) will_fire = true;
	if(fullauto && (latest_input.fire&1) && weapons[active_weapon].ammo) will_fire = true;
	if(!will_fire)
		return;
		
	// check for ammo
	if(!weapons[active_weapon].ammo)
	{
		create_sound(pos, SOUND_WEAPON_NOAMMO);
		return;
	}
	
	vec2 projectile_startpos = pos+direction*phys_size*0.75f;
	
	switch(active_weapon)
	{
		case WEAPON_HAMMER:
		{
			// reset objects hit
			numobjectshit = 0;
			create_sound(pos, SOUND_HAMMER_FIRE);
			
			int type = NETOBJTYPE_PLAYER_CHARACTER;
			ENTITY *ents[64];
			int num = world->find_entities(pos+direction*phys_size*0.75f, phys_size*0.5f, ents, 64, &type, 1);			

			for (int i = 0; i < num; i++)
			{
				PLAYER *target = (PLAYER*)ents[i];
				if (target == this)
					continue;
					
				// hit a player, give him damage and stuffs...
				vec2 fdir = normalize(ents[i]->pos - pos);

				// set his velocity to fast upward (for now)
				create_sound(pos, SOUND_HAMMER_HIT);
				ents[i]->take_damage(vec2(0,-1.0f), data->weapons.hammer.base->damage, client_id, active_weapon);
				vec2 dir;
				if (length(target->pos - pos) > 0.0f)
					dir = normalize(target->pos - pos);
				else
					dir = vec2(0,-1);
					
				target->core.vel += normalize(dir + vec2(0,-1.1f)) * 10.0f;
			}
			
		} break;

		case WEAPON_GUN:
		{
			PROJECTILE *proj = new PROJECTILE(WEAPON_GUN,
				client_id,
				projectile_startpos,
				direction,
				(int)(server_tickspeed()*tuning.gun_lifetime),
				this,
				1, 0, 0, -1, WEAPON_GUN);
				
			// pack the projectile and send it to the client directly
			NETOBJ_PROJECTILE p;
			proj->fill_info(&p);
			
			msg_pack_start(NETMSGTYPE_SV_EXTRAPROJECTILE, 0);
			msg_pack_int(1);
			for(unsigned i = 0; i < sizeof(NETOBJ_PROJECTILE)/sizeof(int); i++)
				msg_pack_int(((int *)&p)[i]);
			msg_pack_end();
			server_send_msg(client_id);
							
			create_sound(pos, SOUND_GUN_FIRE);
		} break;
		
		case WEAPON_SHOTGUN:
		{
			int shotspread = 2;

			msg_pack_start(NETMSGTYPE_SV_EXTRAPROJECTILE, 0);
			msg_pack_int(shotspread*2+1);
			
			for(int i = -shotspread; i <= shotspread; i++)
			{
				float spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
				float a = get_angle(direction);
				a += spreading[i+2];
				float v = 1-(abs(i)/(float)shotspread);
				float speed = mix((float)tuning.shotgun_speeddiff, 1.0f, v);
				PROJECTILE *proj = new PROJECTILE(WEAPON_SHOTGUN,
					client_id,
					projectile_startpos,
					vec2(cosf(a), sinf(a))*speed,
					(int)(server_tickspeed()*tuning.shotgun_lifetime),
					this,
					1, 0, 0, -1, WEAPON_SHOTGUN);
					
				// pack the projectile and send it to the client directly
				NETOBJ_PROJECTILE p;
				proj->fill_info(&p);
				
				for(unsigned i = 0; i < sizeof(NETOBJ_PROJECTILE)/sizeof(int); i++)
					msg_pack_int(((int *)&p)[i]);
			}

			msg_pack_end();
			server_send_msg(client_id);					
			
			create_sound(pos, SOUND_SHOTGUN_FIRE);
		} break;

		case WEAPON_GRENADE:
		{
			PROJECTILE *proj = new PROJECTILE(WEAPON_GRENADE,
				client_id,
				projectile_startpos,
				direction,
				(int)(server_tickspeed()*tuning.grenade_lifetime),
				this,
				1, PROJECTILE::PROJECTILE_FLAGS_EXPLODE, 0, SOUND_GRENADE_EXPLODE, WEAPON_GRENADE);

			// pack the projectile and send it to the client directly
			NETOBJ_PROJECTILE p;
			proj->fill_info(&p);
			
			msg_pack_start(NETMSGTYPE_SV_EXTRAPROJECTILE, 0);
			msg_pack_int(1);
			for(unsigned i = 0; i < sizeof(NETOBJ_PROJECTILE)/sizeof(int); i++)
				msg_pack_int(((int *)&p)[i]);
			msg_pack_end();
			server_send_msg(client_id);

			create_sound(pos, SOUND_GRENADE_FIRE);
		} break;
		
		case WEAPON_RIFLE:
		{
			new LASER(pos, direction, tuning.laser_reach, this);
			create_sound(pos, SOUND_RIFLE_FIRE);
		} break;
		
	}

	if(weapons[active_weapon].ammo > 0) // -1 == unlimited
		weapons[active_weapon].ammo--;
	attack_tick = server_tick();
	reload_timer = data->weapons.id[active_weapon].firedelay * server_tickspeed() / 1000;
}

int PLAYER::handle_weapons()
{
	vec2 direction = normalize(vec2(latest_input.target_x, latest_input.target_y));

	if(config.dbg_stress)
	{
		for(int i = 0; i < NUM_WEAPONS; i++)
		{
			weapons[i].got = true;
			weapons[i].ammo = 10;
		}

		if(reload_timer) // twice as fast reload
			reload_timer--;
	}

	// check reload timer
	if(reload_timer)
	{
		reload_timer--;
		return 0;
	}
	
	if (active_weapon == WEAPON_NINJA)
	{
		// don't update other weapons while ninja is active
		return handle_ninja();
	}

	// fire weapon, if wanted
	fire_weapon();

	// ammo regen
	int ammoregentime = data->weapons.id[active_weapon].ammoregentime;
	if(ammoregentime)
	{
		// If equipped and not active, regen ammo?
		if (reload_timer <= 0)
		{
			if (weapons[active_weapon].ammoregenstart < 0)
				weapons[active_weapon].ammoregenstart = server_tick();

			if ((server_tick() - weapons[active_weapon].ammoregenstart) >= ammoregentime * server_tickspeed() / 1000)
			{
				// Add some ammo
				weapons[active_weapon].ammo = min(weapons[active_weapon].ammo + 1, 10);
				weapons[active_weapon].ammoregenstart = -1;
			}
		}
		else
		{
			weapons[active_weapon].ammoregenstart = -1;
		}
	}
	
	return 0;
}

void PLAYER::on_direct_input(NETOBJ_PLAYER_INPUT *new_input)
{
	mem_copy(&latest_previnput, &latest_input, sizeof(latest_input));
	mem_copy(&latest_input, new_input, sizeof(latest_input));
	if(num_inputs > 2 && team != -1 && !dead)
	{
		handle_weaponswitch();
		fire_weapon();
	}
}

void PLAYER::tick()
{
	server_setclientscore(client_id, score);

	// grab latest input
	/*
	{
		int size = 0;
		int *input = server_latestinput(client_id, &size);
		if(input)
		{
			mem_copy(&latest_previnput, &latest_input, sizeof(latest_input));
			mem_copy(&latest_input, input, sizeof(latest_input));
		}
	}*/
	
	// check if we have enough input
	// this is to prevent initial weird clicks
	if(num_inputs < 2)
	{
		latest_previnput = latest_input;
		previnput = input;
	}
	
	// do latency stuff
	{
		CLIENT_INFO info;
		if(server_getclientinfo(client_id, &info))
		{
			latency_accum += info.latency;
			latency_accum_max = max(latency_accum_max, info.latency);
			latency_accum_min = min(latency_accum_min, info.latency);
		}

		if(server_tick()%server_tickspeed() == 0)
		{
			latency_avg = latency_accum/server_tickspeed();
			latency_max = latency_accum_max;
			latency_min = latency_accum_min;
			latency_accum = 0;
			latency_accum_min = 1000;
			latency_accum_max = 0;
		}
	}

	// enable / disable physics
	if(team == -1 || dead)
	{
		world->core.players[client_id] = 0;
		clear_flag(FLAG_PHYSICS);
	}
	else
	{
		world->core.players[client_id] = &core;
		set_flag(FLAG_PHYSICS);
	}

	// spectator
	if(team == -1)
		return;

	if(spawning)
		try_respawn();

	// TODO: rework the input to be more robust
	if(dead)
	{
		if(server_tick()-die_tick >= server_tickspeed()/2 && count_input(latest_previnput.fire, latest_input.fire).presses)
			die_tick = -1;
		if(server_tick()-die_tick >= server_tickspeed()*5) // auto respawn after 3 sec
			respawn();
		//if((input.fire&1) && server_tick()-die_tick >= server_tickspeed()/2) // auto respawn after 0.5 sec
			//respawn();
		return;
	}

	//player_core core;
	//core.pos = pos;
	//core.jumped = jumped;
	core.input = input;
	core.tick();

	// handle weapons
	handle_weapons();

	player_state = input.player_state;

	// Previnput
	previnput = input;
	return;
}

void PLAYER::tick_defered()
{
	if(!dead)
	{
		vec2 start_pos = core.pos;
		vec2 start_vel = core.vel;
		bool stuck_before = test_box(core.pos, vec2(28.0f, 28.0f));
		
		core.move();
		bool stuck_after_move = test_box(core.pos, vec2(28.0f, 28.0f));
		core.quantize();
		bool stuck_after_quant = test_box(core.pos, vec2(28.0f, 28.0f));
		pos = core.pos;
		
		if(!stuck_before && (stuck_after_move || stuck_after_quant))
		{
			dbg_msg("player", "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x", 
				stuck_before,
				stuck_after_move,
				stuck_after_quant,
				start_pos.x, start_pos.y,
				start_vel.x, start_vel.y,
				*((unsigned *)&start_pos.x), *((unsigned *)&start_pos.y),
				*((unsigned *)&start_vel.x), *((unsigned *)&start_vel.y));
		}

		int events = core.triggered_events;
		int mask = cmask_all_except_one(client_id);
		
		if(events&COREEVENT_GROUND_JUMP) create_sound(pos, SOUND_PLAYER_JUMP, mask);
		if(events&COREEVENT_AIR_JUMP)
		{
			create_sound(pos, SOUND_PLAYER_AIRJUMP, mask);
			NETEVENT_COMMON *c = (NETEVENT_COMMON *)::events.create(NETEVENTTYPE_AIRJUMP, sizeof(NETEVENT_COMMON), mask);
			if(c)
			{
				c->x = (int)pos.x;
				c->y = (int)pos.y;
			}
		}
		
		//if(events&COREEVENT_HOOK_LAUNCH) snd_play_random(CHN_WORLD, SOUND_HOOK_LOOP, 1.0f, pos);
		if(events&COREEVENT_HOOK_ATTACH_PLAYER) create_sound(pos, SOUND_HOOK_ATTACH_PLAYER, cmask_all());
		if(events&COREEVENT_HOOK_ATTACH_GROUND) create_sound(pos, SOUND_HOOK_ATTACH_GROUND, mask);
		//if(events&COREEVENT_HOOK_RETRACT) snd_play_random(CHN_WORLD, SOUND_PLAYER_JUMP, 1.0f, pos);
		
	}
	
	if(team == -1)
	{
		pos.x = input.target_x;
		pos.y = input.target_y;
	}
}

void PLAYER::die(int killer, int weapon)
{
	if (dead || team == -1)
		return;

	int mode_special = gamecontroller->on_player_death(this, get_player(killer), weapon);

	dbg_msg("game", "kill killer='%d:%s' victim='%d:%s' weapon=%d special=%d",
		killer, server_clientname(killer),
		client_id, server_clientname(client_id), weapon, mode_special);

	// send the kill message
	NETMSG_SV_KILLMSG msg;
	msg.killer = killer;
	msg.victim = client_id;
	msg.weapon = weapon;
	msg.mode_special = mode_special;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(-1);

	// a nice sound
	create_sound(pos, SOUND_PLAYER_DIE);

	// set dead state
	die_pos = pos;
	dead = true;
	die_tick = server_tick();
	clear_flag(FLAG_PHYSICS);
	create_death(pos, client_id);
}

bool PLAYER::take_damage(vec2 force, int dmg, int from, int weapon)
{
	core.vel += force;
	
	if(gamecontroller->is_friendly_fire(client_id, from) && !config.sv_teamdamage)
		return false;

	// player only inflicts half damage on self
	if(from == client_id)
		dmg = max(1, dmg/2);

	// CTF and TDM (TODO: check for FF)
	//if (gameobj->gametype != GAMETYPE_DM && from >= 0 && players[from].team == team)
		//return false;

	damage_taken++;

	// create healthmod indicator
	if(server_tick() < damage_taken_tick+25)
	{
		// make sure that the damage indicators doesn't group together
		create_damageind(pos, damage_taken*0.25f, dmg);
	}
	else
	{
		damage_taken = 0;
		create_damageind(pos, 0, dmg);
	}

	if(dmg)
	{
		if(armor)
		{
			if(dmg > 1)
			{
				health--;
				dmg--;
			}
			
			if(dmg > armor)
			{
				dmg -= armor;
				armor = 0;
			}
			else
			{
				armor -= dmg;
				dmg = 0;
			}
		}
		
		health -= dmg;
	}

	damage_taken_tick = server_tick();

	// do damage hit sound
	if(from >= 0 && from != client_id)
		create_sound(get_player(from)->pos, SOUND_HIT, cmask_one(from));

	// check for death
	if(health <= 0)
	{
		die(from, weapon);

		// set attacker's face to happy (taunt!)
		if (from >= 0 && from != client_id)
		{
			PLAYER *p = get_player(from);

			p->emote_type = EMOTE_HAPPY;
			p->emote_stop = server_tick() + server_tickspeed();
		}

		return false;
	}

	if (dmg > 2)
		create_sound(pos, SOUND_PLAYER_PAIN_LONG);
	else
		create_sound(pos, SOUND_PLAYER_PAIN_SHORT);

	emote_type = EMOTE_PAIN;
	emote_stop = server_tick() + 500 * server_tickspeed() / 1000;

	// spawn blood?
	return true;
}

void PLAYER::snap(int snaping_client)
{
	if(1)
	{
		NETOBJ_PLAYER_INFO *info = (NETOBJ_PLAYER_INFO *)snap_new_item(NETOBJTYPE_PLAYER_INFO, client_id, sizeof(NETOBJ_PLAYER_INFO));

		info->latency = latency_min;
		info->latency_flux = latency_max-latency_min;
		info->local = 0;
		info->cid = client_id;
		info->score = score;
		info->team = team;

		if(client_id == snaping_client)
			info->local = 1;
	}

	if(!dead && health > 0 && team >= 0 && distance(players[snaping_client].pos, pos) < 1000.0f)
	{
		NETOBJ_PLAYER_CHARACTER *character = (NETOBJ_PLAYER_CHARACTER *)snap_new_item(NETOBJTYPE_PLAYER_CHARACTER, client_id, sizeof(NETOBJ_PLAYER_CHARACTER));

		core.write(character);

		// this is to make sure that players that are just standing still
		// isn't sent. this is because the physics keep bouncing between
		// 0-128 when just standing.
		// TODO: fix the physics so this isn't needed
		if(snaping_client != client_id && abs(character->vy) < 256.0f)
			character->vy = 0;

		if (emote_stop < server_tick())
		{
			emote_type = EMOTE_NORMAL;
			emote_stop = -1;
		}

		character->emote = emote_type;

		character->ammocount = 0;
		character->health = 0;
		character->armor = 0;
		
		character->weapon = active_weapon;
		character->attacktick = attack_tick;

		character->wanted_direction = input.direction;
		/*
		if(input.left && !input.right)
			character->wanted_direction = -1;
		else if(!input.left && input.right)
			character->wanted_direction = 1;*/


		if(client_id == snaping_client)
		{
			character->health = health;
			character->armor = armor;
			if(weapons[active_weapon].ammo > 0)
				character->ammocount = weapons[active_weapon].ammo;
		}

		if (character->emote == EMOTE_NORMAL)
		{
			if(250 - ((server_tick() - last_action)%(250)) < 5)
				character->emote = EMOTE_BLINK;
		}

		character->player_state = player_state;
	}
}

PLAYER *players;

//////////////////////////////////////////////////
// powerup
//////////////////////////////////////////////////
PICKUP::PICKUP(int _type, int _subtype)
: ENTITY(NETOBJTYPE_PICKUP)
{
	type = _type;
	subtype = _subtype;
	proximity_radius = phys_size;

	reset();

	// TODO: should this be done here?
	world->insert_entity(this);
}

void PICKUP::reset()
{
	if (data->pickups[type].spawndelay > 0)
		spawntick = server_tick() + server_tickspeed() * data->pickups[type].spawndelay;
	else
		spawntick = -1;
}


void send_weapon_pickup(int cid, int weapon);

void PICKUP::tick()
{
	// wait for respawn
	if(spawntick > 0)
	{
		if(server_tick() > spawntick)
		{
			// respawn
			spawntick = -1;

			if(type == POWERUP_WEAPON)
				create_sound(pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}
	// Check if a player intersected us
	PLAYER* pplayer = closest_player(pos, 20.0f, 0);
	if (pplayer)
	{
		// player picked us up, is someone was hooking us, let them go
		int respawntime = -1;
		switch (type)
		{
		case POWERUP_HEALTH:
			if(pplayer->health < 10)
			{
				create_sound(pos, SOUND_PICKUP_HEALTH);
				pplayer->health = min(10, pplayer->health + 1);
				respawntime = data->pickups[type].respawntime;
			}
			break;
		case POWERUP_ARMOR:
			if(pplayer->armor < 10)
			{
				create_sound(pos, SOUND_PICKUP_ARMOR);
				pplayer->armor = min(10, pplayer->armor + 1);
				respawntime = data->pickups[type].respawntime;
			}
			break;

		case POWERUP_WEAPON:
			if(subtype >= 0 && subtype < NUM_WEAPONS)
			{
				if(pplayer->weapons[subtype].ammo < data->weapons.id[subtype].maxammo || !pplayer->weapons[subtype].got)
				{
					pplayer->weapons[subtype].got = true;
					pplayer->weapons[subtype].ammo = min(data->weapons.id[subtype].maxammo, pplayer->weapons[subtype].ammo + 10);
					respawntime = data->pickups[type].respawntime;

					// TODO: data compiler should take care of stuff like this
					if(subtype == WEAPON_GRENADE)
						create_sound(pos, SOUND_PICKUP_GRENADE);
					else if(subtype == WEAPON_SHOTGUN)
						create_sound(pos, SOUND_PICKUP_SHOTGUN);
					else if(subtype == WEAPON_RIFLE)
						create_sound(pos, SOUND_PICKUP_SHOTGUN);

                    send_weapon_pickup(pplayer->client_id, subtype);
				}
			}
			break;
		case POWERUP_NINJA:
			{
				// activate ninja on target player
				pplayer->ninja.activationtick = server_tick();
				pplayer->weapons[WEAPON_NINJA].got = true;
				pplayer->last_weapon = pplayer->active_weapon;
				pplayer->active_weapon = WEAPON_NINJA;
				respawntime = data->pickups[type].respawntime;
				create_sound(pos, SOUND_PICKUP_NINJA);

				// loop through all players, setting their emotes
				ENTITY *ents[64];
				const int types[] = {NETOBJTYPE_PLAYER_CHARACTER};
				int num = world->find_entities(vec2(0, 0), 1000000, ents, 64, types, 1);
				for (int i = 0; i < num; i++)
				{
					PLAYER *p = (PLAYER *)ents[i];
					if (p != pplayer)
					{
						p->emote_type = EMOTE_SURPRISE;
						p->emote_stop = server_tick() + server_tickspeed();
					}
				}

				pplayer->emote_type = EMOTE_ANGRY;
				pplayer->emote_stop = server_tick() + 1200 * server_tickspeed() / 1000;
				
				break;
			}
		default:
			break;
		};

		if(respawntime >= 0)
		{
			dbg_msg("game", "pickup player='%d:%s' item=%d/%d",
				pplayer->client_id, server_clientname(pplayer->client_id), type, subtype);
			spawntick = server_tick() + server_tickspeed() * respawntime;
		}
	}
}

void PICKUP::snap(int snapping_client)
{
	if(spawntick != -1)
		return;

	NETOBJ_PICKUP *up = (NETOBJ_PICKUP *)snap_new_item(NETOBJTYPE_PICKUP, id, sizeof(NETOBJ_PICKUP));
	up->x = (int)pos.x;
	up->y = (int)pos.y;
	up->type = type; // TODO: two diffrent types? what gives?
	up->subtype = subtype;
}

// POWERUP END ///////////////////////

PLAYER *get_player(int index)
{
	return &players[index];
}

void create_damageind(vec2 p, float angle, int amount)
{
	float a = 3 * 3.14159f / 2 + angle;
	//float a = get_angle(dir);
	float s = a-pi/3;
	float e = a+pi/3;
	for(int i = 0; i < amount; i++)
	{
		float f = mix(s, e, float(i+1)/float(amount+2));
		NETEVENT_DAMAGEIND *ev = (NETEVENT_DAMAGEIND *)events.create(NETEVENTTYPE_DAMAGEIND, sizeof(NETEVENT_DAMAGEIND));
		if(ev)
		{
			ev->x = (int)p.x;
			ev->y = (int)p.y;
			ev->angle = (int)(f*256.0f);
		}
	}
}

void create_explosion(vec2 p, int owner, int weapon, bool bnodamage)
{
	// create the event
	NETEVENT_EXPLOSION *ev = (NETEVENT_EXPLOSION *)events.create(NETEVENTTYPE_EXPLOSION, sizeof(NETEVENT_EXPLOSION));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}

	if (!bnodamage)
	{
		// deal damage
		ENTITY *ents[64];
		float radius = 128.0f;
		float innerradius = 42.0f;

		int num = world->find_entities(p, radius, ents, 64);
		for(int i = 0; i < num; i++)
		{
			vec2 diff = ents[i]->pos - p;
			vec2 forcedir(0,1);
			float l = length(diff);
			if(l)
				forcedir = normalize(diff);
			l = 1-clamp((l-innerradius)/(radius-innerradius), 0.0f, 1.0f);
			float dmg = 6 * l;
			if((int)dmg)
				ents[i]->take_damage(forcedir*dmg*2, (int)dmg, owner, weapon);
		}
	}
}

/*
void create_smoke(vec2 p)
{
	// create the event
	EV_EXPLOSION *ev = (EV_EXPLOSION *)events.create(EVENT_SMOKE, sizeof(EV_EXPLOSION));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}*/

void create_playerspawn(vec2 p)
{
	// create the event
	NETEVENT_SPAWN *ev = (NETEVENT_SPAWN *)events.create(NETEVENTTYPE_SPAWN, sizeof(NETEVENT_SPAWN));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
	}
}

void create_death(vec2 p, int cid)
{
	// create the event
	NETEVENT_DEATH *ev = (NETEVENT_DEATH *)events.create(NETEVENTTYPE_DEATH, sizeof(NETEVENT_DEATH));
	if(ev)
	{
		ev->x = (int)p.x;
		ev->y = (int)p.y;
		ev->cid = cid;
	}
}

void create_sound(vec2 pos, int sound, int mask)
{
	if (sound < 0)
		return;

	// create a sound
	NETEVENT_SOUNDWORLD *ev = (NETEVENT_SOUNDWORLD *)events.create(NETEVENTTYPE_SOUNDWORLD, sizeof(NETEVENT_SOUNDWORLD), mask);
	if(ev)
	{
		ev->x = (int)pos.x;
		ev->y = (int)pos.y;
		ev->soundid = sound;
	}
}

void create_sound_global(int sound, int target)
{
	if (sound < 0)
		return;

	NETMSG_SV_SOUNDGLOBAL msg;
	msg.soundid = sound;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(target);
}

// TODO: should be more general
PLAYER *intersect_player(vec2 pos0, vec2 pos1, float radius, vec2& new_pos, ENTITY *notthis)
{
	// Find other players
	float closest_len = distance(pos0, pos1) * 100.0f;
	vec2 line_dir = normalize(pos1-pos0);
	PLAYER *closest = 0;
		
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i].client_id < 0 || (ENTITY *)&players[i] == notthis)
			continue;
			
		if(!(players[i].flags&ENTITY::FLAG_PHYSICS))
			continue;

		vec2 intersect_pos = closest_point_on_line(pos0, pos1, players[i].pos);
		float len = distance(players[i].pos, intersect_pos);
		if(len < PLAYER::phys_size+radius)
		{
			if(len < closest_len)
			{
				new_pos = intersect_pos;
				closest_len = len;
				closest = &players[i];
			}
		}
	}
	
	return closest;
}


PLAYER *closest_player(vec2 pos, float radius, ENTITY *notthis)
{
	// Find other players
	float closest_range = radius*2;
	PLAYER *closest = 0;
		
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(players[i].client_id < 0 || (ENTITY *)&players[i] == notthis)
			continue;
			
		if(!(players[i].flags&ENTITY::FLAG_PHYSICS))
			continue;

		float len = distance(pos, players[i].pos);
		if(len < PLAYER::phys_size+radius)
		{
			if(len < closest_range)
			{
				closest_range = len;
				closest = &players[i];
			}
		}
	}
	
	return closest;
}

// Server hooks
void mods_tick()
{
	world->core.tuning = tuning;
	world->tick();

	if(world->paused) // make sure that the game object always updates
		gamecontroller->tick();
}

void mods_snap(int client_id)
{
	world->snap(client_id);
	events.snap(client_id);
}

void mods_client_direct_input(int client_id, void *input)
{
	if(!world->paused)
		players[client_id].on_direct_input((NETOBJ_PLAYER_INPUT *)input);
	
	/*
	if(i->fire)
	{
		msg_pack_start(MSG_EXTRA_PROJECTILE, 0);
		msg_pack_end();
		server_send_msg(client_id);
	}*/
}

void mods_client_predicted_input(int client_id, void *input)
{
	if(!world->paused)
	{
		if (memcmp(&players[client_id].input, input, sizeof(NETOBJ_PLAYER_INPUT)) != 0)
			players[client_id].last_action = server_tick();

		//players[client_id].previnput = players[client_id].input;
		players[client_id].input = *(NETOBJ_PLAYER_INPUT*)input;
		players[client_id].num_inputs++;
		
		if(players[client_id].input.target_x == 0 && players[client_id].input.target_y == 0)
			players[client_id].input.target_y = -1;
	}
}


void mods_client_enter(int client_id)
{
	world->insert_entity(&players[client_id]);
	players[client_id].respawn();
	dbg_msg("game", "join player='%d:%s'", client_id, server_clientname(client_id));


	char buf[512];
	str_format(buf, sizeof(buf), "%s entered and joined the %s", server_clientname(client_id), get_team_name(players[client_id].team));
	send_chat(-1, CHAT_ALL, buf); 

	dbg_msg("game", "team_join player='%d:%s' team=%d", client_id, server_clientname(client_id), players[client_id].team);
}

void mods_connected(int client_id)
{
	players[client_id].init();
	players[client_id].client_id = client_id;
	
	// Check which team the player should be on
	if(config.sv_tournament_mode)
		players[client_id].team = -1;
	else
		players[client_id].team = gamecontroller->get_auto_team(client_id);

	// send motd
	NETMSG_SV_MOTD msg;
	msg.message = config.sv_motd;
	msg.pack(MSGFLAG_VITAL);
	server_send_msg(client_id);
}

void mods_client_drop(int client_id)
{
	char buf[512];
	str_format(buf, sizeof(buf),  "%s has left the game", server_clientname(client_id));
	send_chat(-1, CHAT_ALL, buf);

	dbg_msg("game", "leave player='%d:%s'", client_id, server_clientname(client_id));

	gamecontroller->on_player_death(&players[client_id], 0, -1);
	world->remove_entity(&players[client_id]);
	world->core.players[client_id] = 0x0;
	players[client_id].client_id = -1;
}

void mods_message(int msgtype, int client_id)
{
	void *rawmsg = netmsg_secure_unpack(msgtype);
	if(!rawmsg)
	{
		dbg_msg("server", "dropped weird message '%s' (%d), failed on '%s'", netmsg_get_name(msgtype), msgtype, netmsg_failed_on());
		return;
	}
	
	if(msgtype == NETMSGTYPE_CL_SAY)
	{
		NETMSG_CL_SAY *msg = (NETMSG_CL_SAY *)rawmsg;
		int team = msg->team;
		if(team)
			team = players[client_id].team;
		else
			team = CHAT_ALL;
		
		if(config.sv_spamprotection && players[client_id].last_chat+time_freq() > time_get())
		{
			// consider this as spam
		}
		else
		{
			players[client_id].last_chat = time_get();
			send_chat(client_id, team, msg->message);
		}
	}
	else if (msgtype == NETMSGTYPE_CL_SETTEAM)
	{
		NETMSG_CL_SETTEAM *msg = (NETMSG_CL_SETTEAM *)rawmsg;

		// Switch team on given client and kill/respawn him
		if(gamecontroller->can_join_team(msg->team, client_id))
			players[client_id].set_team(msg->team);
		else
		{
			char buf[128];
			str_format(buf, sizeof(buf), "Only %d active players are allowed", config.sv_max_clients-config.sv_spectator_slots);
			send_broadcast(buf, client_id);
		}
	}
	else if (msgtype == NETMSGTYPE_CL_CHANGEINFO || msgtype == NETMSGTYPE_CL_STARTINFO)
	{
		NETMSG_CL_CHANGEINFO *msg = (NETMSG_CL_CHANGEINFO *)rawmsg;
		players[client_id].use_custom_color = msg->use_custom_color;
		players[client_id].color_body = msg->color_body;
		players[client_id].color_feet = msg->color_feet;

		// check for invalid chars
		/*
		unsigned char *p = (unsigned char *)name;
		while (*p)
		{
			if(*p < 32)
				*p = ' ';
			p++;
		}*/

		// copy old name
		char oldname[MAX_NAME_LENGTH];
		str_copy(oldname, server_clientname(client_id), MAX_NAME_LENGTH);
		
		server_setclientname(client_id, msg->name);
		if(msgtype == NETMSGTYPE_CL_CHANGEINFO && strcmp(oldname, server_clientname(client_id)) != 0)
		{
			char chattext[256];
			str_format(chattext, sizeof(chattext), "%s changed name to %s", oldname, server_clientname(client_id));
			send_chat(-1, CHAT_ALL, chattext);
		}
		
		// set skin
		str_copy(players[client_id].skin_name, msg->skin, sizeof(players[client_id].skin_name));
		
		gamecontroller->on_player_info_change(&players[client_id]);
		
		if(msgtype == NETMSGTYPE_CL_STARTINFO)
		{
			// a client that connected!
			
			// send all info to this client
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				if(players[i].client_id != -1)
					send_info(i, client_id);
			}

			// send tuning parameters to client
			send_tuning_params(client_id);
			
			//
			NETMSG_SV_READYTOENTER m;
			m.pack(MSGFLAG_VITAL|MSGFLAG_FLUSH);
			server_send_msg(client_id);			
		}
		
		send_info(client_id, -1);
	}
	else if (msgtype == NETMSGTYPE_CL_EMOTICON)
	{
		NETMSG_CL_EMOTICON *msg = (NETMSG_CL_EMOTICON *)rawmsg;
		send_emoticon(client_id, msg->emoticon);
	}
	else if (msgtype == NETMSGTYPE_CL_KILL)
	{
		PLAYER *pplayer = get_player(client_id);
		pplayer->die(client_id, -1);
	}
}

extern unsigned char internal_data[];


static void con_tune_param(void *result, void *user_data)
{
	const char *param_name = console_arg_string(result, 0);
	float new_value = console_arg_float(result, 1);

	if(tuning.set(param_name, new_value))
	{
		dbg_msg("tuning", "%s changed to %.2f", param_name, new_value);
		send_tuning_params(-1);
	}
	else
		console_print("No such tuning parameter");
}

static void con_tune_reset(void *result, void *user_data)
{
	TUNING_PARAMS p;
	tuning = p;
	send_tuning_params(-1);
	console_print("tuning reset");
}

static void con_tune_dump(void *result, void *user_data)
{
	for(int i = 0; i < tuning.num(); i++)
	{
		float v;
		tuning.get(i, &v);
		dbg_msg("tuning", "%s %.2f", tuning.names[i], v);
	}
}


static void con_restart(void *result, void *user_data)
{
	if(console_arg_num(result))
		gamecontroller->do_warmup(console_arg_int(result, 0));
	else
		gamecontroller->startround();
}

static void con_broadcast(void *result, void *user_data)
{
	send_broadcast(console_arg_string(result, 0), -1);
}

static void con_say(void *result, void *user_data)
{
	send_chat(-1, CHAT_ALL, console_arg_string(result, 0));
}

static void con_set_team(void *result, void *user_data)
{
	int client_id = clamp(console_arg_int(result, 0), 0, (int)MAX_CLIENTS);
	int team = clamp(console_arg_int(result, 1), -1, 1);
	
	dbg_msg("", "%d %d", client_id, team);
	
	if(players[client_id].client_id != client_id)
		return;
	
	players[client_id].set_team(team);
}

void mods_console_init()
{
	MACRO_REGISTER_COMMAND("tune", "si", con_tune_param, 0);
	MACRO_REGISTER_COMMAND("tune_reset", "", con_tune_reset, 0);
	MACRO_REGISTER_COMMAND("tune_dump", "", con_tune_dump, 0);

	MACRO_REGISTER_COMMAND("restart", "?i", con_restart, 0);
	MACRO_REGISTER_COMMAND("broadcast", "r", con_broadcast, 0);
	MACRO_REGISTER_COMMAND("say", "r", con_say, 0);
	MACRO_REGISTER_COMMAND("set_team", "ii", con_set_team, 0);
}

void mods_init()
{
	//if(!data) /* only load once */
		//data = load_data_from_memory(internal_data);

	layers_init();
	col_init();

	world = new GAMEWORLD;
	players = new PLAYER[MAX_CLIENTS];

	// select gametype
	if(strcmp(config.sv_gametype, "ctf") == 0)
		gamecontroller = new GAMECONTROLLER_CTF;
	else if(strcmp(config.sv_gametype, "tdm") == 0)
		gamecontroller = new GAMECONTROLLER_TDM;
	else
		gamecontroller = new GAMECONTROLLER_DM;

	// setup core world
	for(int i = 0; i < MAX_CLIENTS; i++)
		players[i].core.world = &world->core;

	// create all entities from the game layer
	MAPITEM_LAYER_TILEMAP *tmap = layers_game_layer();
	TILE *tiles = (TILE *)map_get_data(tmap->data);
	
	num_spawn_points[0] = 0;
	num_spawn_points[1] = 0;
	num_spawn_points[2] = 0;
	
	for(int y = 0; y < tmap->height; y++)
	{
		for(int x = 0; x < tmap->width; x++)
		{
			int index = tiles[y*tmap->width+x].index - ENTITY_OFFSET;
			vec2 pos(x*32.0f+16.0f, y*32.0f+16.0f);
			gamecontroller->on_entity(index, pos);
		}
	}

	world->insert_entity(gamecontroller);

	if(config.dbg_dummies)
	{
		for(int i = 0; i < config.dbg_dummies ; i++)
		{
			mods_connected(MAX_CLIENTS-i-1);
			mods_client_enter(MAX_CLIENTS-i-1);
			if(gamecontroller->gametype != GAMETYPE_DM)
				players[MAX_CLIENTS-i-1].team = i&1;
		}
	}
}

void mods_shutdown()
{
	delete [] players;
	delete gamecontroller;
	delete world;
	gamecontroller = 0;
	players = 0;
	world = 0;
}

void mods_presnap() {}
void mods_postsnap()
{
	events.clear();
}

extern "C" const char *mods_net_version() { return GAME_NETVERSION; }
extern "C" const char *mods_version() { return GAME_VERSION; }
