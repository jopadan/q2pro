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
// cl_fx.c -- entity effects parsing and management

#include "client.h"
#include "shared/m_flash.h"

static void CL_LogoutEffect(const vec3_t org, int color);

static vec3_t avelocities[NUMVERTEXNORMALS];

static cvar_t *cl_lerp_lightstyles;
static cvar_t *cl_muzzlelight_time;

/*
==============================================================

LIGHT STYLE MANAGEMENT

==============================================================
*/

typedef struct {
    int     length;
    float   map[MAX_QPATH - 1];
} clightstyle_t;

static clightstyle_t    cl_lightstyles[MAX_LIGHTSTYLES];

static void CL_ClearLightStyles(void)
{
    memset(cl_lightstyles, 0, sizeof(cl_lightstyles));
}

/*
================
CL_SetLightStyle
================
*/
void CL_SetLightStyle(int index, const char *s)
{
    int     i;
    clightstyle_t   *ls;

    ls = &cl_lightstyles[index];
    ls->length = strlen(s);
    Q_assert(ls->length < MAX_QPATH);

    for (i = 0; i < ls->length; i++)
        ls->map[i] = (float)(s[i] - 'a') / (float)('m' - 'a');
}

/*
================
CL_AddLightStyles
================
*/
void CL_AddLightStyles(void)
{
    int     i, ofs = cl.time / 100;
    clightstyle_t   *ls;

    if (cl_lerp_lightstyles->integer) {
        float f = (cl.time % 100) * 0.01f;
        float b = 1.0f - f;

        for (i = 0, ls = cl_lightstyles; i < MAX_LIGHTSTYLES; i++, ls++) {
            float value = 1.0f;

            if (ls->length > 1)
                value = ls->map[ofs % ls->length] * b + ls->map[(ofs + 1) % ls->length] * f;
            else if (ls->length)
                value = ls->map[0];

            V_AddLightStyle(i, value);
        }
    } else {
        for (i = 0, ls = cl_lightstyles; i < MAX_LIGHTSTYLES; i++, ls++) {
            float value = ls->length ? ls->map[ofs % ls->length] : 1.0f;
            V_AddLightStyle(i, value);
        }
    }
}

/*
==============================================================

DLIGHT MANAGEMENT

==============================================================
*/

static cdlight_t       cl_dlights[MAX_DLIGHTS];

static void CL_ClearDlights(void)
{
    memset(cl_dlights, 0, sizeof(cl_dlights));
}

/*
===============
CL_AllocDlight
===============
*/
cdlight_t *CL_AllocDlight(int key)
{
    int     i;
    cdlight_t   *dl;

// first look for an exact key match
    if (key) {
        dl = cl_dlights;
        for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
            if (dl->key == key) {
                memset(dl, 0, sizeof(*dl));
                dl->key = key;
                return dl;
            }
        }
    }

// then look for anything else
    dl = cl_dlights;
    for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
        if (dl->die < cl.time) {
            memset(dl, 0, sizeof(*dl));
            dl->key = key;
            return dl;
        }
    }

    dl = &cl_dlights[0];
    memset(dl, 0, sizeof(*dl));
    dl->key = key;
    return dl;
}

/*
===============
CL_AddDLights
===============
*/
void CL_AddDLights(void)
{
    int         i;
    cdlight_t   *dl;

    dl = cl_dlights;
    for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
        if (dl->die < cl.time)
            continue;
        V_AddLight(dl->origin, dl->radius,
                   dl->color[0], dl->color[1], dl->color[2]);
    }
}

// ==============================================================

/*
==============
CL_MuzzleFlash
==============
*/
void CL_MuzzleFlash(void)
{
    vec3_t      fv, rv;
    cdlight_t   *dl;
    centity_t   *pl;
    float       volume;
    char        soundname[MAX_QPATH];

#if USE_DEBUG
    if (developer->integer)
        CL_CheckEntityPresent(mz.entity, "muzzleflash");
#endif

    pl = &cl_entities[mz.entity];

    dl = CL_AllocDlight(mz.entity);
    VectorCopy(pl->current.origin,  dl->origin);
    AngleVectors(pl->current.angles, fv, rv, NULL);
    VectorMA(dl->origin, 18, fv, dl->origin);
    VectorMA(dl->origin, 16, rv, dl->origin);
    dl->radius = 100 * (2 - mz.silenced) + (Q_rand() & 31);
    dl->die = cl.time + Cvar_ClampInteger(cl_muzzlelight_time, 0, 1000);

    volume = 1.0f - 0.8f * mz.silenced;

    switch (mz.weapon) {
    case MZ_BLASTER:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_BLAST, (const vec3_t) { 27.0f, 7.4f, -6.6f }, 8.0f);
        break;
    case MZ_BLUEHYPERBLASTER:
        VectorSet(dl->color, 0, 0, 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
        break;
    case MZ_HYPERBLASTER:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_BLAST, (const vec3_t) { 23.5f, 6.0f, -6.0f }, 9.0f);
        break;
    case MZ_MACHINEGUN:
        VectorSet(dl->color, 1, 1, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_MACHN, (const vec3_t) { 29.0f, 9.7f, -8.0f }, 12.0f);
        break;
    case MZ_SHOTGUN:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/shotgf1b.wav"), volume, ATTN_NORM, 0);
        S_StartSound(NULL, mz.entity, CHAN_AUTO,   S_RegisterSound("weapons/shotgr1b.wav"), volume, ATTN_NORM, 0.1f);
        CL_AddWeaponMuzzleFX(MFLASH_SHOTG, (const vec3_t) { 26.5f, 8.6f, -9.5f }, 12.0f);
        break;
    case MZ_SSHOTGUN:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/sshotf1b.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_SHOTG2, (const vec3_t) { 25.0f, 7.0f, -5.5f }, 12.0f);
        break;
    case MZ_CHAINGUN1:
        dl->radius = 200 + (Q_rand() & 31);
        VectorSet(dl->color, 1, 0.25f, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_MACHN, (const vec3_t) { 29.0f, 9.7f, -10.0f }, 12.0f);
        break;
    case MZ_CHAINGUN2:
        dl->radius = 225 + (Q_rand() & 31);
        VectorSet(dl->color, 1, 0.5f, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.05f);
        CL_AddWeaponMuzzleFX(MFLASH_MACHN, (const vec3_t) { 29.0f, 9.7f, -10.0f }, 16.0f);
        break;
    case MZ_CHAINGUN3:
        dl->radius = 250 + (Q_rand() & 31);
        VectorSet(dl->color, 1, 1, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.033f);
        Q_snprintf(soundname, sizeof(soundname), "weapons/machgf%ib.wav", (Q_rand() % 5) + 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), volume, ATTN_NORM, 0.066f);
        CL_AddWeaponMuzzleFX(MFLASH_MACHN, (const vec3_t) { 29.0f, 9.7f, -10.0f }, 20.0f);
        break;
    case MZ_RAILGUN:
        VectorSet(dl->color, 0.5f, 0.5f, 1.0f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/railgf1a.wav"), volume, ATTN_NORM, 0);
        S_StartSound(NULL, mz.entity, CHAN_AUTO,   S_RegisterSound("weapons/railgr1b.wav"), volume, ATTN_NORM, 0.4f);
        CL_AddWeaponMuzzleFX(MFLASH_RAIL, (const vec3_t) { 20.0f, 5.2f, -7.0f }, 12.0f);
        break;
    case MZ_ROCKET:
        VectorSet(dl->color, 1, 0.5f, 0.2f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/rocklf1a.wav"), volume, ATTN_NORM, 0);
        S_StartSound(NULL, mz.entity, CHAN_AUTO,   S_RegisterSound("weapons/rocklr1b.wav"), volume, ATTN_NORM, 0.1f);
        CL_AddWeaponMuzzleFX(MFLASH_ROCKET, (const vec3_t) { 20.8f, 5.0f, -11.0f }, 10.0f);
        break;
    case MZ_GRENADE:
        VectorSet(dl->color, 1, 0.5f, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
        S_StartSound(NULL, mz.entity, CHAN_AUTO,   S_RegisterSound("weapons/grenlr1b.wav"), volume, ATTN_NORM, 0.1f);
        CL_AddWeaponMuzzleFX(MFLASH_LAUNCH, (const vec3_t) { 18.0f, 6.0f, -6.5f }, 9.0f);
        break;
    case MZ_BFG:
        VectorSet(dl->color, 0, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/bfg__f1y.wav"), volume, ATTN_NORM, 0);
        break;
    case MZ_BFG2:
        VectorSet(dl->color, 0, 1, 0);
        CL_AddWeaponMuzzleFX(MFLASH_BFG, (const vec3_t) { 18.0f, 8.0f, -7.5f }, 16.0f);
        break;

    case MZ_LOGIN:
        VectorSet(dl->color, 0, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
        CL_LogoutEffect(pl->current.origin, 0xd0);  // green
        break;
    case MZ_LOGOUT:
        VectorSet(dl->color, 1, 0, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
        CL_LogoutEffect(pl->current.origin, 0x40);  // red
        break;
    case MZ_RESPAWN:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), 1, ATTN_NORM, 0);
        CL_LogoutEffect(pl->current.origin, 0xe0);  // yellow
        break;
    case MZ_PHALANX:
        VectorSet(dl->color, 1, 0.5f, 0.5f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/plasshot.wav"), volume, ATTN_NORM, 0);
        break;
    case MZ_PHALANX2:
        VectorSet(dl->color, 1, 0.5f, 0.5f);
        CL_AddWeaponMuzzleFX(MFLASH_ROCKET, (const vec3_t) { 18.0f, 10.0f, -6.0f }, 9.0f);
        break;
    case MZ_IONRIPPER:
        VectorSet(dl->color, 1, 0.5f, 0.5f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/rippfire.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_BOOMER, (const vec3_t) { 24.0f, 3.8f, -5.5f }, 15.0f);
        break;

    case MZ_PROX:
        VectorSet(dl->color, 1, 0.5f, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/grenlf1a.wav"), volume, ATTN_NORM, 0);
        S_StartSound(NULL, mz.entity, CHAN_AUTO,   S_RegisterSound("weapons/proxlr1a.wav"), volume, ATTN_NORM, 0.1f);
        CL_AddWeaponMuzzleFX(MFLASH_LAUNCH, (const vec3_t) { 18.0f, 6.0f, -6.5f }, 9.0f);
        break;
    case MZ_ETF_RIFLE:
        VectorSet(dl->color, 0.9f, 0.7f, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/nail1.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_ETF_RIFLE, (const vec3_t) { 24.0f, 5.25f, -5.5f }, 4.0f);
        break;
    case MZ_SHOTGUN2:
        // remaster overloads this as MZ_ETF_RIFLE_2
        if (cl.csr.extended) {
            VectorSet(dl->color, 0.9f, 0.7f, 0);
            S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/nail1.wav"), volume, ATTN_NORM, 0);
            CL_AddWeaponMuzzleFX(MFLASH_ETF_RIFLE, (const vec3_t) { 24.0f, 4.0f, -5.5f }, 4.0f);
        } else {
            VectorSet(dl->color, 1, 1, 0);
            S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/shotg2.wav"), volume, ATTN_NORM, 0);
        }
        break;
    case MZ_HEATBEAM:
        VectorSet(dl->color, 1, 1, 0);
        dl->die = cl.time + 100;
//      S_StartSound (NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/bfg__l1a.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_BEAMER, (const vec3_t) { 18.0f, 6.0f, -8.5f }, 16.0f);
        break;
    case MZ_BLASTER2:
        VectorSet(dl->color, 0, 1, 0);
        // FIXME - different sound for blaster2 ??
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/blastf1a.wav"), volume, ATTN_NORM, 0);
        break;
    case MZ_TRACKER:
        // negative flashes handled the same in gl/soft until CL_AddDLights
        VectorSet(dl->color, -1, -1, -1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), volume, ATTN_NORM, 0);
        CL_AddWeaponMuzzleFX(MFLASH_DIST, (const vec3_t) { 18.0f, 6.0f, -6.5f }, 10.0f);
        break;
    case MZ_NUKE1:
        VectorSet(dl->color, 1, 0, 0);
        dl->die = cl.time + 100;
        break;
    case MZ_NUKE2:
        VectorSet(dl->color, 1, 1, 0);
        dl->die = cl.time + 100;
        break;
    case MZ_NUKE4:
        VectorSet(dl->color, 0, 0, 1);
        dl->die = cl.time + 100;
        break;
    case MZ_NUKE8:
        VectorSet(dl->color, 0, 1, 1);
        dl->die = cl.time + 100;
        break;
    }

    if (cl_dlight_hacks->integer & DLHACK_NO_MUZZLEFLASH) {
        switch (mz.weapon) {
        case MZ_MACHINEGUN:
        case MZ_CHAINGUN1:
        case MZ_CHAINGUN2:
        case MZ_CHAINGUN3:
            memset(dl, 0, sizeof(*dl));
            break;
        }
    }
}


/*
==============
CL_MuzzleFlash2
==============
*/
void CL_MuzzleFlash2(void)
{
    centity_t   *ent;
    vec3_t      ofs, origin, flash_origin;
    cdlight_t   *dl;
    vec3_t      forward, right;
    char        soundname[MAX_QPATH];
    float       scale;

    // locate the origin
    ent = &cl_entities[mz.entity];
    AngleVectors(ent->current.angles, forward, right, NULL);

    scale = ent->current.scale;
    if (!scale)
        scale = 1.0f;

    VectorScale(monster_flash_offset[mz.weapon], scale, ofs);
    origin[0] = ent->current.origin[0] + forward[0] * ofs[0] + right[0] * ofs[1];
    origin[1] = ent->current.origin[1] + forward[1] * ofs[0] + right[1] * ofs[1];
    origin[2] = ent->current.origin[2] + forward[2] * ofs[0] + right[2] * ofs[1] + ofs[2];

    VectorMA(origin, 4.0f * scale, forward, flash_origin);

    dl = CL_AllocDlight(mz.entity);
    VectorCopy(origin,  dl->origin);
    dl->radius = 200 + (Q_rand() & 31);
    dl->die = cl.time + Cvar_ClampInteger(cl_muzzlelight_time, 0, 1000);

    switch (mz.weapon) {
    case MZ2_INFANTRY_MACHINEGUN_1:
    case MZ2_INFANTRY_MACHINEGUN_2:
    case MZ2_INFANTRY_MACHINEGUN_3:
    case MZ2_INFANTRY_MACHINEGUN_4:
    case MZ2_INFANTRY_MACHINEGUN_5:
    case MZ2_INFANTRY_MACHINEGUN_6:
    case MZ2_INFANTRY_MACHINEGUN_7:
    case MZ2_INFANTRY_MACHINEGUN_8:
    case MZ2_INFANTRY_MACHINEGUN_9:
    case MZ2_INFANTRY_MACHINEGUN_10:
    case MZ2_INFANTRY_MACHINEGUN_11:
    case MZ2_INFANTRY_MACHINEGUN_12:
    case MZ2_INFANTRY_MACHINEGUN_13:
    case MZ2_INFANTRY_MACHINEGUN_14:
    case MZ2_INFANTRY_MACHINEGUN_15:
    case MZ2_INFANTRY_MACHINEGUN_16:
    case MZ2_INFANTRY_MACHINEGUN_17:
    case MZ2_INFANTRY_MACHINEGUN_18:
    case MZ2_INFANTRY_MACHINEGUN_19:
    case MZ2_INFANTRY_MACHINEGUN_20:
    case MZ2_INFANTRY_MACHINEGUN_21:
    case MZ2_INFANTRY_MACHINEGUN_22:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 18.0f * scale);
        break;

    case MZ2_SOLDIER_MACHINEGUN_1:
    case MZ2_SOLDIER_MACHINEGUN_2:
    case MZ2_SOLDIER_MACHINEGUN_3:
    case MZ2_SOLDIER_MACHINEGUN_4:
    case MZ2_SOLDIER_MACHINEGUN_5:
    case MZ2_SOLDIER_MACHINEGUN_6:
    case MZ2_SOLDIER_MACHINEGUN_7:
    case MZ2_SOLDIER_MACHINEGUN_8:
    case MZ2_SOLDIER_MACHINEGUN_9:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("soldier/solatck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 13.0f * scale);
        break;

    case MZ2_GUNNER_MACHINEGUN_1:
    case MZ2_GUNNER_MACHINEGUN_2:
    case MZ2_GUNNER_MACHINEGUN_3:
    case MZ2_GUNNER_MACHINEGUN_4:
    case MZ2_GUNNER_MACHINEGUN_5:
    case MZ2_GUNNER_MACHINEGUN_6:
    case MZ2_GUNNER_MACHINEGUN_7:
    case MZ2_GUNNER_MACHINEGUN_8:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("gunner/gunatck2.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 24.0f * scale);
        break;

    case MZ2_ACTOR_MACHINEGUN_1:
    case MZ2_SUPERTANK_MACHINEGUN_1:
    case MZ2_SUPERTANK_MACHINEGUN_2:
    case MZ2_SUPERTANK_MACHINEGUN_3:
    case MZ2_SUPERTANK_MACHINEGUN_4:
    case MZ2_SUPERTANK_MACHINEGUN_5:
    case MZ2_SUPERTANK_MACHINEGUN_6:
    case MZ2_TURRET_MACHINEGUN:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 32.0f * scale);
        break;

    case MZ2_BOSS2_MACHINEGUN_L1:
    case MZ2_BOSS2_MACHINEGUN_L2:
    case MZ2_BOSS2_MACHINEGUN_L3:
    case MZ2_BOSS2_MACHINEGUN_L4:
    case MZ2_BOSS2_MACHINEGUN_L5:
    case MZ2_CARRIER_MACHINEGUN_L1:
    case MZ2_CARRIER_MACHINEGUN_L2:
        VectorSet(dl->color, 1, 1, 0);
        if (cl.csr.extended && mz.weapon == MZ2_BOSS2_MACHINEGUN_L2) {
            S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NONE, 0);
            CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 12.0f * scale);
        } else {
            CL_ParticleEffect(origin, vec3_origin, 0, 40);
            CL_SmokeAndFlash(origin);
            S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("infantry/infatck1.wav"), 1, ATTN_NONE, 0);
            CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 32.0f * scale);
        }
        break;

    case MZ2_SOLDIER_BLASTER_1:
    case MZ2_SOLDIER_BLASTER_2:
    case MZ2_SOLDIER_BLASTER_3:
    case MZ2_SOLDIER_BLASTER_4:
    case MZ2_SOLDIER_BLASTER_5:
    case MZ2_SOLDIER_BLASTER_6:
    case MZ2_SOLDIER_BLASTER_7:
    case MZ2_SOLDIER_BLASTER_8:
    case MZ2_SOLDIER_BLASTER_9:
    case MZ2_TURRET_BLASTER:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("soldier/solatck2.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 8.0f * scale);
        break;

    case MZ2_FLYER_BLASTER_1:
    case MZ2_FLYER_BLASTER_2:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 8.0f * scale);
        break;

    case MZ2_MEDIC_BLASTER_1:
    case MZ2_MEDIC_HYPERBLASTER1_1:
    case MZ2_MEDIC_HYPERBLASTER1_2:
    case MZ2_MEDIC_HYPERBLASTER1_3:
    case MZ2_MEDIC_HYPERBLASTER1_4:
    case MZ2_MEDIC_HYPERBLASTER1_5:
    case MZ2_MEDIC_HYPERBLASTER1_6:
    case MZ2_MEDIC_HYPERBLASTER1_7:
    case MZ2_MEDIC_HYPERBLASTER1_8:
    case MZ2_MEDIC_HYPERBLASTER1_9:
    case MZ2_MEDIC_HYPERBLASTER1_10:
    case MZ2_MEDIC_HYPERBLASTER1_11:
    case MZ2_MEDIC_HYPERBLASTER1_12:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("medic/medatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 8.0f * scale);
        break;

    case MZ2_HOVER_BLASTER_1:
    case MZ2_HOVER_BLASTER_2:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("hover/hovatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 8.0f * scale);
        break;

    case MZ2_FLOAT_BLASTER_1:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("floater/fltatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 8.0f * scale);
        break;

    case MZ2_SOLDIER_SHOTGUN_1:
    case MZ2_SOLDIER_SHOTGUN_2:
    case MZ2_SOLDIER_SHOTGUN_3:
    case MZ2_SOLDIER_SHOTGUN_4:
    case MZ2_SOLDIER_SHOTGUN_5:
    case MZ2_SOLDIER_SHOTGUN_6:
    case MZ2_SOLDIER_SHOTGUN_7:
    case MZ2_SOLDIER_SHOTGUN_8:
    case MZ2_SOLDIER_SHOTGUN_9:
        VectorSet(dl->color, 1, 1, 0);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("soldier/solatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_SHOTG, 0, 17.0f * scale);
        break;

    case MZ2_TANK_BLASTER_1:
    case MZ2_TANK_BLASTER_2:
    case MZ2_TANK_BLASTER_3:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 24.0f * scale);
        break;

    case MZ2_TANK_MACHINEGUN_1:
    case MZ2_TANK_MACHINEGUN_2:
    case MZ2_TANK_MACHINEGUN_3:
    case MZ2_TANK_MACHINEGUN_4:
    case MZ2_TANK_MACHINEGUN_5:
    case MZ2_TANK_MACHINEGUN_6:
    case MZ2_TANK_MACHINEGUN_7:
    case MZ2_TANK_MACHINEGUN_8:
    case MZ2_TANK_MACHINEGUN_9:
    case MZ2_TANK_MACHINEGUN_10:
    case MZ2_TANK_MACHINEGUN_11:
    case MZ2_TANK_MACHINEGUN_12:
    case MZ2_TANK_MACHINEGUN_13:
    case MZ2_TANK_MACHINEGUN_14:
    case MZ2_TANK_MACHINEGUN_15:
    case MZ2_TANK_MACHINEGUN_16:
    case MZ2_TANK_MACHINEGUN_17:
    case MZ2_TANK_MACHINEGUN_18:
    case MZ2_TANK_MACHINEGUN_19:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        Q_snprintf(soundname, sizeof(soundname), "tank/tnkatk2%c.wav", 'a' + Q_rand() % 5);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound(soundname), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 20.0f * scale);
        break;

    case MZ2_CHICK_ROCKET_1:
    case MZ2_TURRET_ROCKET:
        VectorSet(dl->color, 1, 0.5f, 0.2f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("chick/chkatck2.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_ROCKET, 0, 16.0f * scale);
        break;

    case MZ2_TANK_ROCKET_1:
    case MZ2_TANK_ROCKET_2:
    case MZ2_TANK_ROCKET_3:
        VectorSet(dl->color, 1, 0.5f, 0.2f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("tank/tnkatck1.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_ROCKET, 0, 28.0f * scale);
        break;

    case MZ2_SUPERTANK_ROCKET_1:
    case MZ2_SUPERTANK_ROCKET_2:
    case MZ2_SUPERTANK_ROCKET_3:
    case MZ2_BOSS2_ROCKET_1:
    case MZ2_BOSS2_ROCKET_2:
    case MZ2_BOSS2_ROCKET_3:
    case MZ2_BOSS2_ROCKET_4:
    case MZ2_CARRIER_ROCKET_1:
//  case MZ2_CARRIER_ROCKET_2:
//  case MZ2_CARRIER_ROCKET_3:
//  case MZ2_CARRIER_ROCKET_4:
        VectorSet(dl->color, 1, 0.5f, 0.2f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("tank/rocket.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_ROCKET, 0, 28.0f * scale);
        break;

    case MZ2_GUNNER_GRENADE_1:
    case MZ2_GUNNER_GRENADE_2:
    case MZ2_GUNNER_GRENADE_3:
    case MZ2_GUNNER_GRENADE_4:
    case MZ2_GUNNER_GRENADE2_1:
    case MZ2_GUNNER_GRENADE2_2:
    case MZ2_GUNNER_GRENADE2_3:
    case MZ2_GUNNER_GRENADE2_4:
    case MZ2_SUPERTANK_GRENADE_1:
    case MZ2_SUPERTANK_GRENADE_2:
        VectorSet(dl->color, 1, 0.5f, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("gunner/gunatck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_LAUNCH, 0, 18.0f * scale);
        break;

    case MZ2_GLADIATOR_RAILGUN_1:
    case MZ2_CARRIER_RAILGUN:
    case MZ2_WIDOW_RAIL:
    case MZ2_MAKRON_RAILGUN_1:
    case MZ2_ARACHNID_RAIL1:
    case MZ2_ARACHNID_RAIL2:
    case MZ2_ARACHNID_RAIL_UP1:
    case MZ2_ARACHNID_RAIL_UP2:
        VectorSet(dl->color, 0.5f, 0.5f, 1.0f);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_RAIL, 0, 32.0f * scale);
        break;

    case MZ2_MAKRON_BFG:
        VectorSet(dl->color, 0.5f, 1, 0.5f);
        //S_StartSound (NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("makron/bfg_fire.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BFG, 0, 64.0f * scale);
        break;

    case MZ2_MAKRON_BLASTER_1:
    case MZ2_MAKRON_BLASTER_2:
    case MZ2_MAKRON_BLASTER_3:
    case MZ2_MAKRON_BLASTER_4:
    case MZ2_MAKRON_BLASTER_5:
    case MZ2_MAKRON_BLASTER_6:
    case MZ2_MAKRON_BLASTER_7:
    case MZ2_MAKRON_BLASTER_8:
    case MZ2_MAKRON_BLASTER_9:
    case MZ2_MAKRON_BLASTER_10:
    case MZ2_MAKRON_BLASTER_11:
    case MZ2_MAKRON_BLASTER_12:
    case MZ2_MAKRON_BLASTER_13:
    case MZ2_MAKRON_BLASTER_14:
    case MZ2_MAKRON_BLASTER_15:
    case MZ2_MAKRON_BLASTER_16:
    case MZ2_MAKRON_BLASTER_17:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("makron/blaster.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 22.0f * scale);
        break;

    case MZ2_JORG_MACHINEGUN_L1:
    case MZ2_JORG_MACHINEGUN_L2:
    case MZ2_JORG_MACHINEGUN_L3:
    case MZ2_JORG_MACHINEGUN_L4:
    case MZ2_JORG_MACHINEGUN_L5:
    case MZ2_JORG_MACHINEGUN_L6:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("boss3/xfire.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 32.0f * scale);
        break;

    case MZ2_JORG_MACHINEGUN_R1:
    case MZ2_JORG_MACHINEGUN_R2:
    case MZ2_JORG_MACHINEGUN_R3:
    case MZ2_JORG_MACHINEGUN_R4:
    case MZ2_JORG_MACHINEGUN_R5:
    case MZ2_JORG_MACHINEGUN_R6:
        VectorSet(dl->color, 1, 1, 0);
        CL_ParticleEffect(origin, vec3_origin, 0, 40);
        CL_SmokeAndFlash(origin);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 32.0f * scale);
        break;

    case MZ2_JORG_BFG_1:
        VectorSet(dl->color, 0.5f, 1, 0.5f);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BFG, 0, 64.0f * scale);
        break;

    case MZ2_BOSS2_MACHINEGUN_R1:
    case MZ2_BOSS2_MACHINEGUN_R2:
    case MZ2_BOSS2_MACHINEGUN_R3:
    case MZ2_BOSS2_MACHINEGUN_R4:
    case MZ2_BOSS2_MACHINEGUN_R5:
    case MZ2_CARRIER_MACHINEGUN_R1:
    case MZ2_CARRIER_MACHINEGUN_R2:
        if (cl.csr.extended && mz.weapon == MZ2_BOSS2_MACHINEGUN_R2) {
            S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("flyer/flyatck3.wav"), 1, ATTN_NONE, 0);
            CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 12.0f * scale);
        } else {
            VectorSet(dl->color, 1, 1, 0);
            CL_ParticleEffect(origin, vec3_origin, 0, 40);
            CL_SmokeAndFlash(origin);
            CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_MACHN, 0, 32.0f * scale);
        }
        break;

    case MZ2_STALKER_BLASTER:
    case MZ2_DAEDALUS_BLASTER:
    case MZ2_DAEDALUS_BLASTER_2:
    case MZ2_MEDIC_BLASTER_2:
    case MZ2_WIDOW_BLASTER:
    case MZ2_WIDOW_BLASTER_SWEEP1:
    case MZ2_WIDOW_BLASTER_SWEEP2:
    case MZ2_WIDOW_BLASTER_SWEEP3:
    case MZ2_WIDOW_BLASTER_SWEEP4:
    case MZ2_WIDOW_BLASTER_SWEEP5:
    case MZ2_WIDOW_BLASTER_SWEEP6:
    case MZ2_WIDOW_BLASTER_SWEEP7:
    case MZ2_WIDOW_BLASTER_SWEEP8:
    case MZ2_WIDOW_BLASTER_SWEEP9:
    case MZ2_WIDOW_BLASTER_100:
    case MZ2_WIDOW_BLASTER_90:
    case MZ2_WIDOW_BLASTER_80:
    case MZ2_WIDOW_BLASTER_70:
    case MZ2_WIDOW_BLASTER_60:
    case MZ2_WIDOW_BLASTER_50:
    case MZ2_WIDOW_BLASTER_40:
    case MZ2_WIDOW_BLASTER_30:
    case MZ2_WIDOW_BLASTER_20:
    case MZ2_WIDOW_BLASTER_10:
    case MZ2_WIDOW_BLASTER_0:
    case MZ2_WIDOW_BLASTER_10L:
    case MZ2_WIDOW_BLASTER_20L:
    case MZ2_WIDOW_BLASTER_30L:
    case MZ2_WIDOW_BLASTER_40L:
    case MZ2_WIDOW_BLASTER_50L:
    case MZ2_WIDOW_BLASTER_60L:
    case MZ2_WIDOW_BLASTER_70L:
    case MZ2_WIDOW_RUN_1:
    case MZ2_WIDOW_RUN_2:
    case MZ2_WIDOW_RUN_3:
    case MZ2_WIDOW_RUN_4:
    case MZ2_WIDOW_RUN_5:
    case MZ2_WIDOW_RUN_6:
    case MZ2_WIDOW_RUN_7:
    case MZ2_WIDOW_RUN_8:
    case MZ2_MEDIC_HYPERBLASTER2_1:
    case MZ2_MEDIC_HYPERBLASTER2_2:
    case MZ2_MEDIC_HYPERBLASTER2_3:
    case MZ2_MEDIC_HYPERBLASTER2_4:
    case MZ2_MEDIC_HYPERBLASTER2_5:
    case MZ2_MEDIC_HYPERBLASTER2_6:
    case MZ2_MEDIC_HYPERBLASTER2_7:
    case MZ2_MEDIC_HYPERBLASTER2_8:
    case MZ2_MEDIC_HYPERBLASTER2_9:
    case MZ2_MEDIC_HYPERBLASTER2_10:
    case MZ2_MEDIC_HYPERBLASTER2_11:
    case MZ2_MEDIC_HYPERBLASTER2_12:
        VectorSet(dl->color, 0, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("tank/tnkatck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 2, 22.0f * scale);
        break;

    case MZ2_WIDOW_DISRUPTOR:
        VectorSet(dl->color, -1, -1, -1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/disint2.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_DIST, 0, 32.0f * scale);
        break;

    case MZ2_WIDOW_PLASMABEAM:
    case MZ2_WIDOW2_BEAMER_1:
    case MZ2_WIDOW2_BEAMER_2:
    case MZ2_WIDOW2_BEAMER_3:
    case MZ2_WIDOW2_BEAMER_4:
    case MZ2_WIDOW2_BEAMER_5:
    case MZ2_WIDOW2_BEAM_SWEEP_1:
    case MZ2_WIDOW2_BEAM_SWEEP_2:
    case MZ2_WIDOW2_BEAM_SWEEP_3:
    case MZ2_WIDOW2_BEAM_SWEEP_4:
    case MZ2_WIDOW2_BEAM_SWEEP_5:
    case MZ2_WIDOW2_BEAM_SWEEP_6:
    case MZ2_WIDOW2_BEAM_SWEEP_7:
    case MZ2_WIDOW2_BEAM_SWEEP_8:
    case MZ2_WIDOW2_BEAM_SWEEP_9:
    case MZ2_WIDOW2_BEAM_SWEEP_10:
    case MZ2_WIDOW2_BEAM_SWEEP_11:
        dl->radius = 300 + (Q_rand() & 100);
        VectorSet(dl->color, 1, 1, 0);
        dl->die = cl.time + 200;
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BEAMER, 0, 32.0f * scale);
        break;

    case MZ2_SOLDIER_RIPPER_1:
    case MZ2_SOLDIER_RIPPER_2:
    case MZ2_SOLDIER_RIPPER_3:
    case MZ2_SOLDIER_RIPPER_4:
    case MZ2_SOLDIER_RIPPER_5:
    case MZ2_SOLDIER_RIPPER_6:
    case MZ2_SOLDIER_RIPPER_7:
    case MZ2_SOLDIER_RIPPER_8:
    case MZ2_SOLDIER_RIPPER_9:
        VectorSet(dl->color, 1, 0.5f, 0.5f);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/rippfire.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BOOMER, 0, 32.0f * scale);
        break;

    case MZ2_SOLDIER_HYPERGUN_1:
    case MZ2_SOLDIER_HYPERGUN_2:
    case MZ2_SOLDIER_HYPERGUN_3:
    case MZ2_SOLDIER_HYPERGUN_4:
    case MZ2_SOLDIER_HYPERGUN_5:
    case MZ2_SOLDIER_HYPERGUN_6:
    case MZ2_SOLDIER_HYPERGUN_7:
    case MZ2_SOLDIER_HYPERGUN_8:
    case MZ2_SOLDIER_HYPERGUN_9:
        VectorSet(dl->color, 0, 0, 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 1, 8.0f * scale);
        break;

    case MZ2_GUARDIAN_BLASTER:
        VectorSet(dl->color, 1, 1, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("weapons/hyprbf1a.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_BLAST, 0, 16.0f * scale);
        break;

    case MZ2_GUNCMDR_CHAINGUN_1:
    case MZ2_GUNCMDR_CHAINGUN_2:
        VectorSet(dl->color, 0, 0, 1);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("guncmdr/gcdratck2.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_ETF_RIFLE, 0, 16.0f * scale);
        break;

    case MZ2_GUNCMDR_GRENADE_MORTAR_1:
    case MZ2_GUNCMDR_GRENADE_MORTAR_2:
    case MZ2_GUNCMDR_GRENADE_MORTAR_3:
    case MZ2_GUNCMDR_GRENADE_FRONT_1:
    case MZ2_GUNCMDR_GRENADE_FRONT_2:
    case MZ2_GUNCMDR_GRENADE_FRONT_3:
        VectorSet(dl->color, 1, 0.5f, 0);
        S_StartSound(NULL, mz.entity, CHAN_WEAPON, S_RegisterSound("guncmdr/gcdratck3.wav"), 1, ATTN_NORM, 0);
        CL_AddMuzzleFX(flash_origin, ent->current.angles, MFLASH_LAUNCH, 0, 18.0f * scale);
        break;
    }
}

/*
==============================================================

PARTICLE MANAGEMENT

==============================================================
*/

static cparticle_t  *active_particles, *free_particles;

static cparticle_t  particles[MAX_PARTICLES];

static void CL_ClearParticles(void)
{
    int     i;

    free_particles = &particles[0];
    active_particles = NULL;

    for (i = 0; i < MAX_PARTICLES - 1; i++)
        particles[i].next = &particles[i + 1];
    particles[i].next = NULL;
}

cparticle_t *CL_AllocParticle(void)
{
    cparticle_t *p;

    if (!free_particles)
        return NULL;
    p = free_particles;
    free_particles = p->next;
    p->next = active_particles;
    active_particles = p;

    p->scale = 1.0f;
    return p;
}

/*
===============
CL_ParticleEffect

Wall impact puffs
===============
*/
void CL_ParticleEffect(const vec3_t org, const vec3_t dir, int color, int count)
{
    int         i, j;
    cparticle_t *p;
    float       d;

    for (i = 0; i < count; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = color + (Q_rand() & 7);

        d = Q_rand() & 31;
        for (j = 0; j < 3; j++) {
            p->org[j] = org[j] + ((int)(Q_rand() & 7) - 4) + d * dir[j];
            p->vel[j] = crand() * 20;
        }

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
    }
}


/*
===============
CL_ParticleEffect2
===============
*/
void CL_ParticleEffect2(const vec3_t org, const vec3_t dir, int color, int count)
{
    int         i, j;
    cparticle_t *p;
    float       d;

    for (i = 0; i < count; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = color;

        d = Q_rand() & 7;
        for (j = 0; j < 3; j++) {
            p->org[j] = org[j] + ((int)(Q_rand() & 7) - 4) + d * dir[j];
            p->vel[j] = crand() * 20;
        }

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
    }
}


/*
===============
CL_TeleporterParticles
===============
*/
void CL_TeleporterParticles(const vec3_t org)
{
    int         i, j;
    cparticle_t *p;

    for (i = 0; i < 8; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = 0xdb;

        for (j = 0; j < 2; j++) {
            p->org[j] = org[j] - 16 + (Q_rand() & 31);
            p->vel[j] = crand() * 14;
        }

        p->org[2] = org[2] - 8 + (Q_rand() & 7);
        p->vel[2] = 80 + (Q_rand() & 7);

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.5f;
    }
}


/*
===============
CL_LogoutEffect

===============
*/
static void CL_LogoutEffect(const vec3_t org, int color)
{
    int         i, j;
    cparticle_t *p;

    for (i = 0; i < 500; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;

        p->color = color + (Q_rand() & 7);

        p->org[0] = org[0] - 16 + frand() * 32;
        p->org[1] = org[1] - 16 + frand() * 32;
        p->org[2] = org[2] - 24 + frand() * 56;

        for (j = 0; j < 3; j++)
            p->vel[j] = crand() * 20;

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / (1.0f + frand() * 0.3f);
    }
}


/*
===============
CL_ItemRespawnParticles

===============
*/
void CL_ItemRespawnParticles(const vec3_t org)
{
    int         i, j;
    cparticle_t *p;

    for (i = 0; i < 64; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;

        p->color = 0xd4 + (Q_rand() & 3); // green

        p->org[0] = org[0] + crand() * 8;
        p->org[1] = org[1] + crand() * 8;
        p->org[2] = org[2] + crand() * 8;

        for (j = 0; j < 3; j++)
            p->vel[j] = crand() * 8;

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY * 0.2f;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / (1.0f + frand() * 0.3f);
    }
}


/*
===============
CL_ExplosionParticles
===============
*/
void CL_ExplosionParticles(const vec3_t org)
{
    int         i, j;
    cparticle_t *p;

    for (i = 0; i < 256; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = 0xe0 + (Q_rand() & 7);

        for (j = 0; j < 3; j++) {
            p->org[j] = org[j] + ((int)(Q_rand() % 32) - 16);
            p->vel[j] = (int)(Q_rand() % 384) - 192;
        }

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
    }
}

/*
===============
CL_BigTeleportParticles
===============
*/
void CL_BigTeleportParticles(const vec3_t org)
{
    static const byte   colortable[4] = {2 * 8, 13 * 8, 21 * 8, 18 * 8};
    int         i;
    cparticle_t *p;
    float       angle, dist;

    for (i = 0; i < 4096; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;

        p->color = colortable[Q_rand() & 3];

        angle = (Q_rand() & 1023) * (M_PIf * 2 / 1023);
        dist = Q_rand() & 31;
        p->org[0] = org[0] + cosf(angle) * dist;
        p->vel[0] = cosf(angle) * (70 + (Q_rand() & 63));
        p->accel[0] = -cosf(angle) * 100;

        p->org[1] = org[1] + sinf(angle) * dist;
        p->vel[1] = sinf(angle) * (70 + (Q_rand() & 63));
        p->accel[1] = -sinf(angle) * 100;

        p->org[2] = org[2] + 8 + (Q_rand() % 90);
        p->vel[2] = -100 + (int)(Q_rand() & 31);
        p->accel[2] = PARTICLE_GRAVITY * 4;
        p->alpha = 1.0f;

        p->alphavel = -0.3f / (0.5f + frand() * 0.3f);
    }
}


/*
===============
CL_BlasterParticles

Wall impact puffs
===============
*/
void CL_BlasterParticles(const vec3_t org, const vec3_t dir)
{
    int         i, j;
    cparticle_t *p;
    float       d;

    for (i = 0; i < 40; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = 0xe0 + (Q_rand() & 7);

        d = Q_rand() & 15;
        for (j = 0; j < 3; j++) {
            p->org[j] = org[j] + ((int)(Q_rand() & 7) - 4) + d * dir[j];
            p->vel[j] = dir[j] * 30 + crand() * 40;
        }

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -1.0f / (0.5f + frand() * 0.3f);
    }
}


/*
===============
CL_BlasterTrail

===============
*/
void CL_BlasterTrail(centity_t *ent, const vec3_t end)
{
    vec3_t      move;
    vec3_t      vec;
    int         i, j, count;
    cparticle_t *p;
    const int   dec = 5;

    VectorSubtract(end, ent->lerp_origin, vec);
    count = VectorNormalize(vec) / dec;
    if (!count)
        return;

    VectorCopy(ent->lerp_origin, move);
    VectorScale(vec, dec, vec);

    for (i = 0; i < count; i++) {
        p = CL_AllocParticle();
        if (!p)
            break;
        VectorClear(p->accel);

        p->time = cl.time;

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (0.3f + frand() * 0.2f);
        p->color = 0xe0;
        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + crand();
            p->vel[j] = crand() * 5;
        }

        VectorAdd(move, vec, move);
    }

    VectorCopy(move, ent->lerp_origin);
}

/*
===============
CL_FlagTrail

===============
*/
void CL_FlagTrail(centity_t *ent, const vec3_t end, int color)
{
    vec3_t      move;
    vec3_t      vec;
    int         i, j, count;
    cparticle_t *p;
    const int   dec = 5;

    VectorSubtract(end, ent->lerp_origin, vec);
    count = VectorNormalize(vec) / dec;
    if (!count)
        return;

    VectorCopy(ent->lerp_origin, move);
    VectorScale(vec, dec, vec);

    for (i = 0; i < count; i++) {
        p = CL_AllocParticle();
        if (!p)
            break;
        VectorClear(p->accel);

        p->time = cl.time;

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (0.8f + frand() * 0.2f);
        p->color = color;
        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + crand() * 16;
            p->vel[j] = crand() * 5;
        }

        VectorAdd(move, vec, move);
    }

    VectorCopy(move, ent->lerp_origin);
}

/*
===============
CL_DiminishingTrail

Now combined with CL_RocketTrail().
===============
*/
void CL_DiminishingTrail(centity_t *ent, const vec3_t end, diminishing_trail_t type)
{
    static const byte  colors[DT_COUNT] = { 0xe8, 0xdb, 0x04, 0x04, 0xd8 };
    static const float alphas[DT_COUNT] = { 0.4f, 0.4f, 0.2f, 0.2f, 0.4f };
    vec3_t      move;
    vec3_t      vec;
    int         i, j, count;
    cparticle_t *p;
    const float dec = 0.5f;
    float       orgscale;
    float       velscale;

    VectorSubtract(end, ent->lerp_origin, vec);
    count = VectorNormalize(vec) / dec;
    if (!count)
        return;

    VectorCopy(ent->lerp_origin, move);
    VectorScale(vec, dec, vec);

    if (ent->trailcount > 900) {
        orgscale = 4;
        velscale = 15;
    } else if (ent->trailcount > 800) {
        orgscale = 2;
        velscale = 10;
    } else {
        orgscale = 1;
        velscale = 5;
    }

    for (i = 0; i < count; i++) {
        // drop less particles as it flies
        if ((Q_rand() & 1023) < ent->trailcount) {
            p = CL_AllocParticle();
            if (!p)
                break;

            VectorClear(p->accel);
            p->time = cl.time;

            p->alpha = 1.0f;
            p->alphavel = -1.0f / (1 + frand() * alphas[type]);

            for (j = 0; j < 3; j++) {
                p->org[j] = move[j] + crand() * orgscale;
                p->vel[j] = crand() * velscale;
            }

            if (type >= DT_ROCKET)
                p->accel[2] = 20;
            else
                p->vel[2] -= PARTICLE_GRAVITY;

            if (type == DT_FIREBALL)
                p->color = colors[type] + (1024 - ent->trailcount) / 64;
            else
                p->color = colors[type] + (Q_rand() & 7);
        }

        // rocket fire (non-diminishing)
        if (type == DT_ROCKET && (Q_rand() & 15) == 0) {
            p = CL_AllocParticle();
            if (!p)
                break;

            VectorClear(p->accel);
            p->time = cl.time;

            p->alpha = 1.0f;
            p->alphavel = -1.0f / (1 + frand() * 0.2f);
            p->color = 0xdc + (Q_rand() & 3);
            for (j = 0; j < 3; j++) {
                p->org[j] = move[j] + crand() * 5;
                p->vel[j] = crand() * 20;
            }
            p->accel[2] = -PARTICLE_GRAVITY;
        }

        ent->trailcount -= 5;
        if (ent->trailcount < 100)
            ent->trailcount = 100;
        VectorAdd(move, vec, move);
    }

    VectorCopy(move, ent->lerp_origin);
}

/*
===============
CL_RailTrail

===============
*/
void CL_OldRailTrail(void)
{
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         j;
    cparticle_t *p;
    float       dec;
    vec3_t      right, up;
    int         i;
    float       d, c, s;
    vec3_t      dir;

    VectorCopy(te.pos1, move);
    VectorSubtract(te.pos2, te.pos1, vec);
    len = VectorNormalize(vec);

    MakeNormalVectors(vec, right, up);

    for (i = 0; i < len; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        VectorClear(p->accel);

        d = i * 0.1f;
        c = cosf(d);
        s = sinf(d);

        VectorScale(right, c, dir);
        VectorMA(dir, s, up, dir);

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (1 + frand() * 0.2f);
        p->color = 0x74 + (Q_rand() & 7);
        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + dir[j] * 3;
            p->vel[j] = dir[j] * 6;
        }

        VectorAdd(move, vec, move);
    }

    dec = 0.75f;
    VectorScale(vec, dec, vec);
    VectorCopy(te.pos1, move);

    while (len > 0) {
        len -= dec;

        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        VectorClear(p->accel);

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (0.6f + frand() * 0.2f);
        p->color = Q_rand() & 15;

        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + crand() * 3;
            p->vel[j] = crand() * 3;
        }

        VectorAdd(move, vec, move);
    }
}


/*
===============
CL_BubbleTrail

===============
*/
void CL_BubbleTrail(const vec3_t start, const vec3_t end)
{
    vec3_t      move;
    vec3_t      vec;
    float       len;
    int         i, j;
    cparticle_t *p;
    float       dec;

    VectorCopy(start, move);
    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);

    dec = 32;
    VectorScale(vec, dec, vec);

    for (i = 0; i < len; i += dec) {
        p = CL_AllocParticle();
        if (!p)
            return;

        VectorClear(p->accel);
        p->time = cl.time;

        p->alpha = 1.0f;
        p->alphavel = -1.0f / (1 + frand() * 0.2f);
        p->color = 4 + (Q_rand() & 7);
        for (j = 0; j < 3; j++) {
            p->org[j] = move[j] + crand() * 2;
            p->vel[j] = crand() * 5;
        }
        p->vel[2] += 6;

        VectorAdd(move, vec, move);
    }
}


/*
===============
CL_FlyParticles
===============
*/

#define BEAMLENGTH  16

static void CL_FlyParticles(const vec3_t origin, int count)
{
    int         i;
    cparticle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    if (count > NUMVERTEXNORMALS)
        count = NUMVERTEXNORMALS;

    ltime = cl.time * 0.001f;
    for (i = 0; i < count; i += 2) {
        p = CL_AllocParticle();
        if (!p)
            return;

        angle = ltime * avelocities[i][0];
        sy = sinf(angle);
        cy = cosf(angle);
        angle = ltime * avelocities[i][1];
        sp = sinf(angle);
        cp = cosf(angle);

        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;

        p->time = cl.time;

        dist = sinf(ltime + i) * 64;
        p->org[0] = origin[0] + bytedirs[i][0] * dist + forward[0] * BEAMLENGTH;
        p->org[1] = origin[1] + bytedirs[i][1] * dist + forward[1] * BEAMLENGTH;
        p->org[2] = origin[2] + bytedirs[i][2] * dist + forward[2] * BEAMLENGTH;

        VectorClear(p->vel);
        VectorClear(p->accel);

        p->color = 0;

        p->alpha = 1;
        p->alphavel = INSTANT_PARTICLE;
    }
}

void CL_FlyEffect(centity_t *ent, const vec3_t origin)
{
    int     n;
    int     count;
    int     starttime;

    if (ent->fly_stoptime < cl.time) {
        starttime = cl.time;
        ent->fly_stoptime = cl.time + 60000;
    } else {
        starttime = ent->fly_stoptime - 60000;
    }

    n = cl.time - starttime;
    if (n < 20000)
        count = n * NUMVERTEXNORMALS / 20000;
    else {
        n = ent->fly_stoptime - cl.time;
        if (n < 20000)
            count = n * NUMVERTEXNORMALS / 20000;
        else
            count = NUMVERTEXNORMALS;
    }

    CL_FlyParticles(origin, count);
}


/*
===============
CL_BfgParticles
===============
*/
void CL_BfgParticles(const entity_t *ent)
{
    int         i;
    cparticle_t *p;
    float       angle;
    float       sp, sy, cp, cy;
    vec3_t      forward;
    float       dist;
    float       ltime;

    ltime = cl.time * 0.001f;
    for (i = 0; i < NUMVERTEXNORMALS; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        angle = ltime * avelocities[i][0];
        sy = sinf(angle);
        cy = cosf(angle);
        angle = ltime * avelocities[i][1];
        sp = sinf(angle);
        cp = cosf(angle);

        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;

        p->time = cl.time;

        dist = sinf(ltime + i) * 64;
        p->org[0] = ent->origin[0] + bytedirs[i][0] * dist + forward[0] * BEAMLENGTH;
        p->org[1] = ent->origin[1] + bytedirs[i][1] * dist + forward[1] * BEAMLENGTH;
        p->org[2] = ent->origin[2] + bytedirs[i][2] * dist + forward[2] * BEAMLENGTH;

        VectorClear(p->vel);
        VectorClear(p->accel);

        dist = Distance(p->org, ent->origin) / 90.0f;
        p->color = floorf(0xd0 + dist * 7);

        p->alpha = 1.0f - dist;
        p->alphavel = INSTANT_PARTICLE;
    }
}


/*
===============
CL_BFGExplosionParticles
===============
*/
//FIXME combined with CL_ExplosionParticles
void CL_BFGExplosionParticles(const vec3_t org)
{
    int         i, j;
    cparticle_t *p;

    for (i = 0; i < 256; i++) {
        p = CL_AllocParticle();
        if (!p)
            return;

        p->time = cl.time;
        p->color = 0xd0 + (Q_rand() & 7);

        for (j = 0; j < 3; j++) {
            p->org[j] = org[j] + ((int)(Q_rand() % 32) - 16);
            p->vel[j] = (int)(Q_rand() % 384) - 192;
        }

        p->accel[0] = p->accel[1] = 0;
        p->accel[2] = -PARTICLE_GRAVITY;
        p->alpha = 1.0f;

        p->alphavel = -0.8f / (0.5f + frand() * 0.3f);
    }
}


/*
===============
CL_TeleportParticles

===============
*/
void CL_TeleportParticles(const vec3_t org)
{
    int         i, j, k;
    cparticle_t *p;
    float       vel;
    vec3_t      dir;

    for (i = -16; i <= 16; i += 4)
        for (j = -16; j <= 16; j += 4)
            for (k = -16; k <= 32; k += 4) {
                p = CL_AllocParticle();
                if (!p)
                    return;

                p->time = cl.time;
                p->color = 7 + (Q_rand() & 7);

                p->alpha = 1.0f;
                p->alphavel = -1.0f / (0.3f + (Q_rand() & 7) * 0.02f);

                p->org[0] = org[0] + i + (Q_rand() & 3);
                p->org[1] = org[1] + j + (Q_rand() & 3);
                p->org[2] = org[2] + k + (Q_rand() & 3);

                dir[0] = j * 8;
                dir[1] = i * 8;
                dir[2] = k * 8;

                VectorNormalize(dir);
                vel = 50 + (Q_rand() & 63);
                VectorScale(dir, vel, p->vel);

                p->accel[0] = p->accel[1] = 0;
                p->accel[2] = -PARTICLE_GRAVITY;
            }
}

extern int          r_numparticles;
extern particle_t   r_particles[MAX_PARTICLES];

/*
===============
CL_AddParticles
===============
*/
void CL_AddParticles(void)
{
    cparticle_t     *p, *next;
    float           alpha;
    float           time, time2;
    cparticle_t     *active, *tail;
    particle_t      *part;

    active = NULL;
    tail = NULL;

    for (p = active_particles; p; p = next) {
        next = p->next;

        if (p->alphavel != INSTANT_PARTICLE) {
            time = (cl.time - p->time) * 0.001f;
            alpha = p->alpha + time * p->alphavel;
            if (alpha <= 0) {
                // faded out
                p->next = free_particles;
                free_particles = p;
                continue;
            }
        } else {
            time = 0.0f;
            alpha = p->alpha;
        }

        if (r_numparticles >= MAX_PARTICLES)
            break;
        part = &r_particles[r_numparticles++];

        p->next = NULL;
        if (!tail)
            active = tail = p;
        else {
            tail->next = p;
            tail = p;
        }

        time2 = time * time;

        part->origin[0] = p->org[0] + p->vel[0] * time + p->accel[0] * time2;
        part->origin[1] = p->org[1] + p->vel[1] * time + p->accel[1] * time2;
        part->origin[2] = p->org[2] + p->vel[2] * time + p->accel[2] * time2;

        part->rgba = p->rgba;
        part->color = p->color;
        part->alpha = min(alpha, 1.0f);
        part->scale = p->scale;

        if (p->alphavel == INSTANT_PARTICLE) {
            p->alphavel = 0.0f;
            p->alpha = 0.0f;
        }
    }

    active_particles = active;
}


/*
==============
CL_ClearEffects

==============
*/
void CL_ClearEffects(void)
{
    CL_ClearLightStyles();
    CL_ClearParticles();
    CL_ClearDlights();
}

void CL_InitEffects(void)
{
    int i, j;

    for (i = 0; i < NUMVERTEXNORMALS; i++)
        for (j = 0; j < 3; j++)
            avelocities[i][j] = (Q_rand() & 255) * 0.01f;

    cl_lerp_lightstyles = Cvar_Get("cl_lerp_lightstyles", "0", 0);
    cl_muzzlelight_time = Cvar_Get("cl_muzzlelight_time", "16", 0);
}
