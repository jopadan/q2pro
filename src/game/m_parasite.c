/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
/*
==============================================================================

parasite

==============================================================================
*/

#include "g_local.h"
#include "m_parasite.h"

static int  sound_pain1;
static int  sound_pain2;
static int  sound_die;
static int  sound_launch;
static int  sound_impact;
static int  sound_suck;
static int  sound_reelin;
static int  sound_sight;
static int  sound_tap;
static int  sound_scratch;

void parasite_stand(edict_t *self);
void parasite_start_run(edict_t *self);
void parasite_start_walk(edict_t *self);

static void parasite_run(edict_t *self);
static void parasite_walk(edict_t *self);
static void parasite_do_fidget(edict_t *self);
static void parasite_refidget(edict_t *self);

static void parasite_launch(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_launch, 1, ATTN_NORM, 0);
}

static void parasite_reel_in(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_reelin, 1, ATTN_NORM, 0);
}

void parasite_sight(edict_t *self, edict_t *other)
{
    gi.sound(self, CHAN_WEAPON, sound_sight, 1, ATTN_NORM, 0);
}

static void parasite_tap(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_tap, 1, ATTN_IDLE, 0);
}

static void parasite_scratch(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_scratch, 1, ATTN_IDLE, 0);
}

static const mframe_t parasite_frames_start_fidget[] = {
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL }
};
const mmove_t parasite_move_start_fidget = {FRAME_stand18, FRAME_stand21, parasite_frames_start_fidget, parasite_do_fidget};

static const mframe_t parasite_frames_fidget[] = {
    { ai_stand, 0, parasite_scratch },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_scratch },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL }
};
const mmove_t parasite_move_fidget = {FRAME_stand22, FRAME_stand27, parasite_frames_fidget, parasite_refidget};

static const mframe_t parasite_frames_end_fidget[] = {
    { ai_stand, 0, parasite_scratch },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL }
};
const mmove_t parasite_move_end_fidget = {FRAME_stand28, FRAME_stand35, parasite_frames_end_fidget, parasite_stand};

static void parasite_do_fidget(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_fidget;
}

static void parasite_refidget(edict_t *self)
{
    if (random() <= 0.8f)
        self->monsterinfo.currentmove = &parasite_move_fidget;
    else
        self->monsterinfo.currentmove = &parasite_move_end_fidget;
}

void parasite_idle(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_start_fidget;
}

static const mframe_t parasite_frames_stand[] = {
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap },
    { ai_stand, 0, NULL },
    { ai_stand, 0, parasite_tap }
};
const mmove_t parasite_move_stand = {FRAME_stand01, FRAME_stand17, parasite_frames_stand, parasite_stand};

void parasite_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_stand;
}

static const mframe_t parasite_frames_run[] = {
    { ai_run, 30, NULL },
    { ai_run, 30, NULL },
    { ai_run, 22, NULL },
    { ai_run, 19, NULL },
    { ai_run, 24, NULL },
    { ai_run, 28, NULL },
    { ai_run, 25, NULL }
};
const mmove_t parasite_move_run = {FRAME_run03, FRAME_run09, parasite_frames_run, NULL};

static const mframe_t parasite_frames_start_run[] = {
    { ai_run, 0,  NULL },
    { ai_run, 30, NULL },
};
const mmove_t parasite_move_start_run = {FRAME_run01, FRAME_run02, parasite_frames_start_run, parasite_run};

static const mframe_t parasite_frames_stop_run[] = {
    { ai_run, 20, NULL },
    { ai_run, 20, NULL },
    { ai_run, 12, NULL },
    { ai_run, 10, NULL },
    { ai_run, 0,  NULL },
    { ai_run, 0,  NULL }
};
const mmove_t parasite_move_stop_run = {FRAME_run10, FRAME_run15, parasite_frames_stop_run, NULL};

void parasite_start_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &parasite_move_stand;
    else
        self->monsterinfo.currentmove = &parasite_move_start_run;
}

static void parasite_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &parasite_move_stand;
    else
        self->monsterinfo.currentmove = &parasite_move_run;
}

static const mframe_t parasite_frames_walk[] = {
    { ai_walk, 30, NULL },
    { ai_walk, 30, NULL },
    { ai_walk, 22, NULL },
    { ai_walk, 19, NULL },
    { ai_walk, 24, NULL },
    { ai_walk, 28, NULL },
    { ai_walk, 25, NULL }
};
const mmove_t parasite_move_walk = {FRAME_run03, FRAME_run09, parasite_frames_walk, parasite_walk};

static const mframe_t parasite_frames_start_walk[] = {
    { ai_walk, 0, NULL },
    { ai_walk, 30, parasite_walk }
};
const mmove_t parasite_move_start_walk = {FRAME_run01, FRAME_run02, parasite_frames_start_walk, NULL};

static const mframe_t parasite_frames_stop_walk[] = {
    { ai_walk, 20, NULL },
    { ai_walk, 20,    NULL },
    { ai_walk, 12, NULL },
    { ai_walk, 10, NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL }
};
const mmove_t parasite_move_stop_walk = {FRAME_run10, FRAME_run15, parasite_frames_stop_walk, NULL};

void parasite_start_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_start_walk;
}

static void parasite_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_walk;
}

static const mframe_t parasite_frames_pain1[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 6, NULL },
    { ai_move, 16, NULL },
    { ai_move, -6, NULL },
    { ai_move, -7, NULL },
    { ai_move, 0, NULL }
};
const mmove_t parasite_move_pain1 = {FRAME_pain101, FRAME_pain111, parasite_frames_pain1, parasite_start_run};

void parasite_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (level.framenum < self->pain_debounce_framenum)
        return;

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    if (skill->value == 3)
        return;     // no pain anims in nightmare

    if (random() < 0.5f)
        gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

    self->monsterinfo.currentmove = &parasite_move_pain1;
}

static bool parasite_drain_attack_ok(vec3_t start, vec3_t end)
{
    vec3_t  dir, angles;

    // check for max distance
    VectorSubtract(start, end, dir);
    if (VectorLength(dir) > 256)
        return false;

    // check for min/max pitch
    vectoangles(dir, angles);
    if (angles[0] < -180)
        angles[0] += 360;
    if (fabsf(angles[0]) > 30)
        return false;

    return true;
}

static void parasite_drain_attack(edict_t *self)
{
    vec3_t  offset, start, f, r, end, dir;
    trace_t tr;
    int damage;

    AngleVectors(self->s.angles, f, r, NULL);
    VectorSet(offset, 24, 0, 6);
    G_ProjectSource(self->s.origin, offset, f, r, start);

    VectorCopy(self->enemy->s.origin, end);
    if (!parasite_drain_attack_ok(start, end)) {
        end[2] = self->enemy->s.origin[2] + self->enemy->maxs[2] - 8;
        if (!parasite_drain_attack_ok(start, end)) {
            end[2] = self->enemy->s.origin[2] + self->enemy->mins[2] + 8;
            if (!parasite_drain_attack_ok(start, end))
                return;
        }
    }
    VectorCopy(self->enemy->s.origin, end);

    tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);
    if (tr.ent != self->enemy)
        return;

    if (self->s.frame == FRAME_drain03) {
        damage = 5;
        gi.sound(self->enemy, CHAN_AUTO, sound_impact, 1, ATTN_NORM, 0);
    } else {
        if (self->s.frame == FRAME_drain04)
            gi.sound(self, CHAN_WEAPON, sound_suck, 1, ATTN_NORM, 0);
        damage = 2;
    }

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_PARASITE_ATTACK);
    gi.WriteShort(self - g_edicts);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(self->s.origin, MULTICAST_PVS);

    VectorSubtract(start, end, dir);
    T_Damage(self->enemy, self, self, dir, self->enemy->s.origin, vec3_origin, damage, 0, DAMAGE_NO_KNOCKBACK, MOD_UNKNOWN);
}

static const mframe_t parasite_frames_drain[] = {
    { ai_charge, 0,   parasite_launch },
    { ai_charge, 0,   NULL },
    { ai_charge, 15,  parasite_drain_attack },          // Target hits
    { ai_charge, 0,   parasite_drain_attack },          // drain
    { ai_charge, 0,   parasite_drain_attack },          // drain
    { ai_charge, 0,   parasite_drain_attack },          // drain
    { ai_charge, 0,   parasite_drain_attack },          // drain
    { ai_charge, -2,  parasite_drain_attack },          // drain
    { ai_charge, -2,  parasite_drain_attack },          // drain
    { ai_charge, -3,  parasite_drain_attack },          // drain
    { ai_charge, -2,  parasite_drain_attack },          // drain
    { ai_charge, 0,   parasite_drain_attack },          // drain
    { ai_charge, -1,  parasite_drain_attack },          // drain
    { ai_charge, 0,   parasite_reel_in },               // let go
    { ai_charge, -2,  NULL },
    { ai_charge, -2,  NULL },
    { ai_charge, -3,  NULL },
    { ai_charge, 0,   NULL }
};
const mmove_t parasite_move_drain = {FRAME_drain01, FRAME_drain18, parasite_frames_drain, parasite_start_run};

void parasite_attack(edict_t *self)
{
    self->monsterinfo.currentmove = &parasite_move_drain;
}

/*
===
Death Stuff Starts
===
*/

static void parasite_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, -8);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

static const mframe_t parasite_frames_death[] = {
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL }
};
const mmove_t parasite_move_death = {FRAME_death101, FRAME_death107, parasite_frames_death, parasite_dead};

void parasite_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

// check for gib
    if (self->health <= self->gib_health) {
        gi.sound(self, CHAN_VOICE, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 2; n++)
            ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
        for (n = 0; n < 4; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

// regular death
    gi.sound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.currentmove = &parasite_move_death;
}

/*
===
End Death Stuff
===
*/

static void parasite_precache(void)
{
    sound_pain1 = gi.soundindex("parasite/parpain1.wav");
    sound_pain2 = gi.soundindex("parasite/parpain2.wav");
    sound_die = gi.soundindex("parasite/pardeth1.wav");
    sound_launch = gi.soundindex("parasite/paratck1.wav");
    sound_impact = gi.soundindex("parasite/paratck2.wav");
    sound_suck = gi.soundindex("parasite/paratck3.wav");
    sound_reelin = gi.soundindex("parasite/paratck4.wav");
    sound_sight = gi.soundindex("parasite/parsght1.wav");
    sound_tap = gi.soundindex("parasite/paridle1.wav");
    sound_scratch = gi.soundindex("parasite/paridle2.wav");
}

/*QUAKED monster_parasite (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_parasite(edict_t *self)
{
    if (deathmatch->value) {
        G_FreeEdict(self);
        return;
    }

    G_AddPrecache(parasite_precache);

    self->s.modelindex = gi.modelindex("models/monsters/parasite/tris.md2");
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, 24);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    self->health = 175;
    self->gib_health = -50;
    self->mass = 250;

    self->pain = parasite_pain;
    self->die = parasite_die;

    self->monsterinfo.stand = parasite_stand;
    self->monsterinfo.walk = parasite_start_walk;
    self->monsterinfo.run = parasite_start_run;
    self->monsterinfo.attack = parasite_attack;
    self->monsterinfo.sight = parasite_sight;
    self->monsterinfo.idle = parasite_idle;

    gi.linkentity(self);

    self->monsterinfo.currentmove = &parasite_move_stand;
    self->monsterinfo.scale = MODEL_SCALE;

    walkmonster_start(self);
}
