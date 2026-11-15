/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file plyr_packet.c
 *     Players User Input handling.
 * @par Purpose:
 *     Implement functions for storing user input and building packets
 *     which will later influence the game world.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     11 Dec 2024 - 01 Feb 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "plyr_packet.h"

#include <assert.h>
#include <stdlib.h>
#include "bfkeybd.h"
#include "bfscd.h"
#include "bfmemut.h"
#include "bfmusic.h"
#include "bfutility.h"
#include "ssampply.h"

#include "bigmap.h"
#include "command.h"
#include "bmcrater.h"
#include "display.h"
#include "drawtext_wrp.h"
#include "game_bstype.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "guitext.h"
#include "hud_panel.h"
#include "hud_target.h"
#include "keyboard.h"
#include "network.h"
#include "packet.h"
#include "player.h"
#include "plyr_net.h"
#include "research.h"
#include "sound.h"
#include "thing.h"
#include "swlog.h"
#include "fncheck.h"
#include "people.h"
#include "weapon.h"
#include "frame_sprani.h"
#include "enginsngobjs.h"
#include "engintrns.h"
#include "vehicle.h"
/******************************************************************************/
struct ShortPacket shpackets[8];

/******************************************************************************/

void net_player_leave(PlayerIdx plyr)
{
    kill_my_players(plyr);
    if ((plyr == net_host_player_no) || (plyr == local_player_no) || (nsvc.I.Type != NetSvc_IPX))
    {
        ingame.DisplayMode = DpM_PURPLEMNU;
        StopCD();
        StopAllSamples();
        SetMusicVolume(100, 0);
        LbNetworkSessionStop(local_player_no);
        if (nsvc.I.Type != NetSvc_IPX && net_serial_uses_modem)
            LbNetworkHangUp();
    }
    else
    {
        net_players_num--;
        player_message_fmt(plyr, "%s %s", net_player_names[plyr], gui_strings[GSTR_NET_LEFT_GAME]);
        LbNetworkSessionStop(plyr);
        ingame.InNetGame_UNSURE &= ~(1 << plyr);
    }
}

void player_agent_select(PlayerIdx plyr, ThingIdx person)
{
    if (person == (ThingIdx)players[plyr].DirectControl[mouser])
        return;
    if (plyr == local_player_no)
    {
        struct Thing *p_person;
        ushort smp;

        p_person = &things[person];
        if (p_person->SubType == SubTT_PERS_AGENT)
            smp = 44; // 'selected' speech
        else
            smp = 46;
        play_disk_sample(0, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
    if (person != (ThingIdx)players[plyr].DirectControl[0])
    {
        player_change_person(person, plyr);
    }
}

/** Alter relative position, limiting its vector length to 256.
 *
 * Relative position change needs to be capped, as packet creation function does
 * not care too much about exceeding the limit. Normalizing here, on packet receive
 * side, also makes sure cheating is not possible.
 */
void packet_rel_position_3d_normalize(short *p_x, short *p_y, short *p_z)
{
    int len;
    len = map_distance_deltas_fast(*p_x, *p_y, *p_z);
    if (len < 256) { // do not increase vector length if below max
        len = 256;
    }
    *p_x = (*p_x) * 256 / len;
    *p_y = (*p_y) * 256 / len;
    *p_z = (*p_z) * 256 / len;
}


void set_person_animmode_run(struct Thing *p_person);
void build_navigate_path_to_face_xz(struct Thing *p_thing, short face, int x, int z);
int start_goto_vehicle(struct Thing *p_vehicle, int x, int z);
ushort build_navigate_path(struct Thing *p_thing, int x, int z, int face);
void thing_goto_point(struct Thing *p_thing, short x, short y, short z);

#ifdef SW_FNCHK
static void thing_goto_point_fast_asm(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    asm volatile (
      "push %4\n"
      "call ASM_thing_goto_point_fast\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z), "g" (plyr) : "cc", "memory");
}

static void thing_goto_point_asm(struct Thing *p_thing, short x, short y, short z)
{
    asm volatile (
      "call ASM_thing_goto_point\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z) : "cc", "memory");
}
#endif

/** Agents of the player protecting the leader start running with him, if
 * they have stamina to spare.
 * Remade from set_player_peeps_to_run.
 */
short get_next_player_agent(ushort player);
void should_i_get_item(struct Thing *p_person, struct SimpleThing *p_sthing);
void build_navigate_path_to_face(struct Thing *p_thing, short face);
int can_i_see_building(struct Thing *p_i, struct Thing *p_thing, int max_dist, ushort flags);
void get_soul(struct Thing *p_dead, struct Thing *p_person);
int map_things_unkn_func_04(short subtype);
struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z);
short find_peep_in_area(struct Thing *p_me, struct Command *p_cmd);
void init_laser_guided(struct Thing *p_owner, ushort count);
ubyte all_group_arrived(ushort group, short x, short y, short z, int radius);
ubyte all_group_arrived_square(ushort group, short x, short z, short x2, int z2);
ubyte group_not_seen(ushort group);
void person_intel(struct Thing *p_person);
void mech_unkn_func_05(int owntng, int a2, int a3);
ubyte track_target(struct Thing *p_mgun);
void rotate_object(struct Thing *p_object);
void init_mgun_explode(struct Thing *p_thing);

void set_player_peeps_to_run(struct Thing *p_leader, ushort plyr)
{
    ushort plagent;

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if ((p_agent == NULL) || (p_agent == p_leader))
            continue;
        if ((p_agent->Flag & TngF_Destroyed) != 0)
            continue;
        if ((ushort)p_agent->State != PerSt_PROTECT_PERSON)
            continue;
        if (p_agent->GotoThingIndex != (ushort)p_leader->ThingOffset)
            continue;
        if (p_agent->U.UPerson.Stamina <= (p_agent->U.UPerson.MaxStamina >> 2) + 64)
            continue;
        p_agent->U.UPerson.Target2 = 0;
        p_agent->Flag &= ~TngF_TriggerUse;
        p_agent->PTarget = NULL;
        set_person_animmode_run(p_agent);
        p_agent->Flag2 |= 0x80000000;
    }
}

/** Agents of the player protecting the given one lose their paths.
 * Remade from clear_player_protectors_paths.
 */
static void clear_player_protectors_paths(struct Thing *p_leader)
{
    ushort plyr, plagent;

    plyr = p_leader->U.UPerson.ComCur >> 2;
    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if (p_agent == NULL)
            continue;
        if ((ushort)p_agent->State != PerSt_PROTECT_PERSON)
            continue;
        if (p_agent->GotoThingIndex != (ushort)p_leader->ThingOffset)
            continue;
        remove_path(p_agent);
        p_agent->U.UPerson.ComTimer = -1;
    }
}

/** Animation of a person after AnimMode changed. */
static void thing_goto_set_anim_frames(struct Thing *p_thing)
{
    ushort base;

    base = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode];
    p_thing->StartFrame = base - 1;
    p_thing->Frame = nstart_ani[base + p_thing->U.UPerson.Angle];
}

/** Send a thing (a person, or the vehicle it drives) to a point.
 * Remade from thing_goto_point; y is not used.
 */
static void thing_goto_point_c(struct Thing *p_thing, short x, short y, short z)
{
    ushort state;

    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_StationrSht | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00020000 | TgF2_Unkn0040);
    if ((ushort)p_thing->State == PerSt_DEAD)
        return;
    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_thing);

    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        if (start_goto_vehicle(&things[p_thing->U.UPerson.Vehicle], x, z) == 0) {
            p_thing->Flag2 |= TgF2_Unkn0040;
            return;
        }
        p_thing->U.UPerson.GotoX = x;
        p_thing->U.UPerson.GotoZ = z;
        return;
    }
    if ((p_thing->Flag & TngF_SelectedAgent) != 0)
        clear_player_protectors_paths(p_thing);
    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = 0;
    p_thing->Flag2 &= ~TgF2_Unkn00020000;

    state = (ushort)p_thing->State & 0x7FFF;
    switch (state)
    {
    case PerSt_NONE:
    case PerSt_GOTO_POINT:
    case PerSt_WAIT:
    case PerSt_GOTO_PERSON:
    case PerSt_GET_ITEM:
    case PerSt_USE_VEHICLE:
        break;
    case PerSt_DRIVING_VEHICLE:
        start_goto_vehicle(&things[p_thing->U.UPerson.Vehicle], x, z);
        return;
    default:
        return;
    }

    p_thing->U.UPerson.ComTimer = -1;
    p_thing->Timer1 = 48;
    p_thing->StartTimer1 = 48;
    p_thing->SubState = 0;
    p_thing->U.UPerson.Target2 = 0;
    p_thing->U.UPerson.GotoX = x;
    p_thing->U.UPerson.GotoZ = z;
    p_thing->State = PerSt_GOTO_POINT;
    p_thing->U.UPerson.ComRange = 1;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);
    p_thing->U.UPerson.PathIndex = 0;
    build_navigate_path(p_thing, x, z, 0);

    if ((p_thing->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if (p_thing->U.UPerson.AnimMode == ANIM_PERS_Unkn19)
            return;
        p_thing->U.UPerson.AnimMode = ANIM_PERS_Unkn19;
        thing_goto_set_anim_frames(p_thing);
    }
    else
    {
        if (p_thing->U.UPerson.AnimMode != ANIM_PERS_Unkn19)
            return;
        p_thing->U.UPerson.AnimMode = gun_out_anim(p_thing, 0);
        thing_goto_set_anim_frames(p_thing);
    }
}

/** Send a thing to a point, running; the protectors of a player's agent
 * run with it.  Remade from thing_goto_point_fast.
 */
static void thing_goto_point_fast_c(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00080000 | TgF2_Unkn00020000 | TgF2_Unkn0040);
    p_thing->Flag2 |= TgF2_Unkn00080000;
    thing_goto_point(p_thing, x, y, z);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}

void thing_goto_point_fast(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    FNCHK_VOID("thing_goto_point_fast", thing_goto_point_fast_asm(p_thing, x, y, z, plyr),
      thing_goto_point_fast_c(p_thing, x, y, z, plyr));
}

void thing_goto_point(struct Thing *p_thing, short x, short y, short z)
{
    FNCHK_VOID("thing_goto_point", thing_goto_point_asm(p_thing, x, y, z),
      thing_goto_point_c(p_thing, x, y, z));
}

#ifdef SW_FNCHK
static void thing_goto_point_rel_asm(struct Thing *p_thing, short x, short y, short z)
{
    asm volatile (
      "call ASM_thing_goto_point_rel\n"
        : "+a" (p_thing), "+d" (x), "+b" (y), "+c" (z) : : "cc", "memory");
}

static void thing_goto_point_rel_fast_asm(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    int stkargs[1];

    stkargs[0] = plyr;
    asm volatile (
      "push 0(%4)\n"
      "call ASM_thing_goto_point_rel_fast\n"
        : "+a" (p_thing), "+d" (x), "+b" (y), "+c" (z) : "S" (stkargs) : "cc", "memory");
}

static void thing_goto_point_on_face_asm(struct Thing *p_thing, short x, short z, short face)
{
    asm volatile (
      "call ASM_thing_goto_point_on_face\n"
        : "+a" (p_thing), "+d" (x), "+b" (z), "+c" (face) : : "cc", "memory");
}

static void thing_goto_point_on_face_fast_asm(struct Thing *p_thing, short x, short z, short face, int plyr)
{
    int stkargs[1];

    stkargs[0] = plyr;
    asm volatile (
      "push 0(%4)\n"
      "call ASM_thing_goto_point_on_face_fast\n"
        : "+a" (p_thing), "+d" (x), "+b" (z), "+c" (face) : "S" (stkargs) : "cc", "memory");
}
#endif

/** After the walk/run mode changed: the run animation (mode 19) when
 * running, else back from it to the weapon animation. */
static void thing_goto_update_run_anim(struct Thing *p_thing)
{
    if ((p_thing->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if (p_thing->U.UPerson.AnimMode == ANIM_PERS_Unkn19)
            return;
        p_thing->U.UPerson.AnimMode = ANIM_PERS_Unkn19;
    }
    else
    {
        if (p_thing->U.UPerson.AnimMode != ANIM_PERS_Unkn19)
            return;
        p_thing->U.UPerson.AnimMode = gun_out_anim(p_thing, 0);
    }
    thing_goto_set_anim_frames(p_thing);
}

/** Moves a person in the direction (x,z) (keyboard / pad control): the
 * direction goes to VX/VZ and the state becomes 35; a person firing a
 * stationary weapon only turns.  Remade from thing_goto_point_rel. */
static void thing_goto_point_rel_c(struct Thing *p_thing, short x, short y, short z)
{
    ushort state;

    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00020000 | TgF2_Unkn0040);
    if ((p_thing->Flag2 & TgF2_KnockedOut) != 0)
        return;
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;
    if ((p_thing->Flag & TngF_StationrSht) != 0)
    {
        ubyte dir;

        dir = ((((short)angle_between_points(0, 0, x, z) + 0x80) >> 8) + 8) & 7;
        if (dir == p_thing->U.UPerson.Angle)
            return;
        p_thing->U.UPerson.Angle = dir;
        p_thing->Frame = nstart_ani[people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode]
          + dir];
        return;
    }
    if (((p_thing->Flag2 & TgF2_Unkn0001) != 0) && ((p_thing->Flag & TngF_TriggerUse) == 0))
        finalise_razor_wire(p_thing);
    if ((p_thing->Flag & TngF_InVehicle) != 0)
        return;
    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = 0;
    state = (ushort)p_thing->State;
    if (((state & 0x7FFF) != PerSt_GOTO_POINT) && ((state & 0x7FFF) != 0x23) && (state != 5))
        return;
    p_thing->StartTimer1 = 48;
    p_thing->Flag2 &= ~TgF2_Unkn00020000;
    p_thing->State = (state & 0x8000) | 0x23;
    p_thing->VX = x;
    p_thing->VZ = z;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);
    thing_goto_update_run_anim(p_thing);
}

/** As thing_goto_point_rel, running; the protectors of a player's agent run
 * with it.  Remade from thing_goto_point_rel_fast. */
static void thing_goto_point_rel_fast_c(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00080000 | TgF2_Unkn00020000 | TgF2_Unkn0040);
    p_thing->Flag2 |= TgF2_Unkn00080000;
    // the asm calls this through the C wrapper, which normalizes
    packet_rel_position_3d_normalize(&x, &y, &z);
    thing_goto_point_rel_c(p_thing, x, y, z);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}

/** Point where a vehicle sent to a road face goes: the far end of the
 * chain of road pieces of the face's building.
 * Remade from find_bezier_nav_point. */
static void find_bezier_nav_point(short face, struct Thing *p_vehicle, short *p_x, short *p_z)
{
    struct Thing *p_road, *p_first, *p_last;
    ushort obj;
    s32 dx, dz, dist_first, dist_last;

    if (face > 0)
        obj = game_object_faces3[face].Object;
    else
        obj = game_object_faces4[-face].Object;
    p_road = &things[game_objects[obj].ThingNo];
    p_first = p_road;
    while (p_first->U.UObject.PrevThing > 0)
        p_first = &things[p_first->U.UObject.PrevThing];
    p_last = p_road;
    while (p_last->U.UObject.NextThing > 0)
        p_last = &things[p_last->U.UObject.NextThing];
    dx = (p_vehicle->X - p_first->X) >> 9;
    dz = (p_vehicle->Z - p_first->Z) >> 9;
    dist_first = dx * dx + dz * dz;
    dx = (p_vehicle->X - p_last->X) >> 9;
    dz = (p_vehicle->Z - p_last->Z) >> 9;
    dist_last = dx * dx + dz * dz;
    if (dist_first < dist_last) {
        *p_x = p_last->X >> 8;
        *p_z = p_last->Z >> 8;
    } else {
        *p_x = p_first->X >> 8;
        *p_z = p_first->Z >> 8;
    }
}

/** Sends a person to point (x,z) on a face of an object (negative face:
 * quad face); a person in a vehicle sends the vehicle, on a road face to
 * the far end of the road.  Remade from thing_goto_point_on_face. */
static void thing_goto_point_on_face_c(struct Thing *p_thing, short x, short z, short face)
{
    ushort state;

    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Unkn0001)) != 0)
        return;
    if ((p_thing->Flag & (TngF_StationrSht | TngF_Destroyed)) != 0)
        return;
    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_thing);
    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;
        short gx, gz;

        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        gx = x;
        gz = z;
        if (face < 0)
        {
            ushort obj;

            obj = game_object_faces4[-face].Object;
            if (things[game_objects[obj].ThingNo].SubType == 0x15)
                find_bezier_nav_point(face, p_vehicle, &gx, &gz);
        }
        else if (face == 0)
            return;
        p_vehicle->Flag &= ~TngF_PlayerAgent;
        start_goto_vehicle(p_vehicle, gx, gz);
        return;
    }
    p_thing->Flag2 &= ~TgF2_Unkn00020000;
    state = (ushort)p_thing->State & 0x7FFF;
    if ((state > PerSt_GOTO_POINT) && (state != 5) && (state != 0x10) &&
      (state != 0x2C) && (state != 0x2D))
        return;
    p_thing->State = PerSt_GOTO_POINT;
    p_thing->U.UPerson.ComTimer = -1;
    p_thing->Timer1 = 48;
    p_thing->StartTimer1 = 48;
    p_thing->SubState = 0;
    p_thing->U.UPerson.ComRange = 1;
    p_thing->U.UPerson.GotoX = x;
    p_thing->U.UPerson.GotoZ = z;
    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = face;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);
    p_thing->U.UPerson.PathIndex = 0;
    build_navigate_path_to_face_xz(p_thing, (short)-face, x, z);
    thing_goto_update_run_anim(p_thing);
}

/** As thing_goto_point_on_face, running; the protectors of a player's
 * agent run with it.  Remade from thing_goto_point_on_face_fast. */
static void thing_goto_point_on_face_fast_c(struct Thing *p_thing, short x, short z, short face, int plyr)
{
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 |= TgF2_Unkn00080000;
    thing_goto_point_on_face_c(p_thing, x, z, face);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}

void thing_goto_point_rel(struct Thing *p_thing, short x, short y, short z)
{
    // The normalization stays outside of the remade function
    packet_rel_position_3d_normalize(&x, &y, &z);
    FNCHK_VOID("thing_goto_point_rel", thing_goto_point_rel_asm(p_thing, x, y, z),
      thing_goto_point_rel_c(p_thing, x, y, z));
}

void thing_goto_point_rel_fast(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    FNCHK_VOID("thing_goto_point_rel_fast", thing_goto_point_rel_fast_asm(p_thing, x, y, z, plyr),
      thing_goto_point_rel_fast_c(p_thing, x, y, z, plyr));
}

void thing_goto_point_on_face_fast(struct Thing *p_thing, short x, short z, short face, int plyr)
{
    FNCHK_VOID("thing_goto_point_on_face_fast",
      thing_goto_point_on_face_fast_asm(p_thing, x, z, face, plyr),
      thing_goto_point_on_face_fast_c(p_thing, x, z, face, plyr));
}

void thing_goto_point_on_face(struct Thing *p_thing, short x, short z, short face)
{
    FNCHK_VOID("thing_goto_point_on_face", thing_goto_point_on_face_asm(p_thing, x, z, face),
      thing_goto_point_on_face_c(p_thing, x, z, face));
}

#ifdef SW_FNCHK
static ubyte select_new_weapon_asm(ushort index, short dir)
{
    int ret;
    asm volatile ("call ASM_select_new_weapon\n"
        : "=a" (ret), "+d" (dir) : "0" (index) : "cc", "memory");
    return ret;
}
#endif

/** The next weapon type the person carries, going from its current one in
 * steps of `dir` (below 0 it wraps to 32); 0 when the steps leave 1..32,
 * the current one when nothing is found in 32 steps.
 * Remade from select_new_weapon. */
static ubyte select_new_weapon_c(ushort index, short dir)
{
    struct Thing *p_person;
    u32 weapons;
    short wep;
    ushort n;

    p_person = &things[index];
    if ((p_person->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_person);
    wep = (short)(p_person->U.UPerson.CurrentWeapon + dir);
    if (wep < 0)
        wep = 0x20;
    if ((wep < 1) || (wep > 0x20))
        return 0;
    weapons = p_person->U.UPerson.WeaponsCarried & ~0x400000;
    n = 0;
    while (((weapons & (1u << ((wep - 1) & 0x1F))) == 0) && (n < 0x20))
    {
        wep = (short)(wep + dir);
        if ((wep < 1) || (wep > 0x20))
            return 0;
        n++;
    }
    if (n >= 0x20)
        return p_person->U.UPerson.CurrentWeapon;
    return (ubyte)wep;
}

ubyte select_new_weapon(ushort index, short dir)
{
    ubyte ret;
    FNCHK_RET("select_new_weapon", ret, select_new_weapon_asm(index, dir),
      select_new_weapon_c(index, dir));
    return ret;
}

void unkn_player_group_add(sbyte a1, ubyte a2)
{
    asm volatile ("call ASM_unkn_player_group_add\n"
        :  : "a" (a1), "d" (a2));
}

void unkn_player_group_prot(sbyte a1, ubyte a2)
{
    asm volatile ("call ASM_unkn_player_group_prot\n"
        :  : "a" (a1), "d" (a2));
}

#ifdef SW_FNCHK
static void peep_change_weapon_asm(struct Thing *p_person)
{
    asm volatile (
      "call ASM_peep_change_weapon\n"
        : : "a" (p_person) : "cc", "memory");
}
#endif

/** A person changed weapon: the sounds of the old one stop, a player's
 * agent remembers the new one as its previous weapon.
 * Remade from peep_change_weapon.
 */
static void peep_change_weapon_c(struct Thing *p_person)
{
    ushort plagent;

    if ((p_person->Flag & TngF_WepCharging) != 0)
    {
        stop_sample_using_heap(p_person->ThingOffset, 7);
        stop_sample_using_heap(p_person->ThingOffset, 52);
    }
    if ((p_person->Flag2 & TgF2_Unkn0200) != 0)
    {
        plagent = ((p_person->Flag & TngF_PlayerAgent) != 0) ? (p_person->U.UPerson.ComCur & 3) : 0;
        ReleaseLoopedSample(p_person->ThingOffset, 80 + plagent);
        plagent = ((p_person->Flag & TngF_PlayerAgent) != 0) ? (p_person->U.UPerson.ComCur & 3) : 0;
        ReleaseLoopedSample(p_person->ThingOffset, 84 + plagent);
        ReleaseLoopedSample(p_person->ThingOffset, 14);
        p_person->Flag2 &= ~TgF2_Unkn0200;
    }
    if (((p_person->Flag & TngF_PlayerAgent) != 0) && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        players[p_person->U.UPerson.ComCur >> 2].PrevWeapon[p_person->U.UPerson.ComCur & 3] =
          p_person->U.UPerson.CurrentWeapon;
    }
}

void peep_change_weapon(struct Thing *p_person)
{
    FNCHK_VOID("peep_change_weapon", peep_change_weapon_asm(p_person), peep_change_weapon_c(p_person));
}

void player_agent_weapon_switch(PlayerIdx plyr, ThingIdx person, short shift)
{
    struct Thing *p_person;

    p_person = &things[person];

    p_person->U.UPerson.CurrentWeapon = select_new_weapon(person, shift);
    peep_change_weapon(p_person);
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        ushort smp;
        smp = weapon_sound_name_speech_index(p_person->U.UPerson.CurrentWeapon);
        play_disk_sample(local_player_no, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
}

StateChRes player_agent_init_drop_item(PlayerIdx plyr, struct Thing *p_person, ThingIdx item)
{
    StateChRes res;
    if ((item == 0) || (item == p_person->U.UPerson.CurrentWeapon)) {
        set_person_anim_mode(p_person, ANIM_PERS_IDLE);
    }
    if (p_person->State == PerSt_PROTECT_PERSON)
        p_person->Flag2 |= TgF2_Unkn10000000;

    res = person_init_drop_item_where_standing(p_person, item);
    //TODO we've just initiated the drop, the weapon is not subtracted yet; a bit early for speed recalc?
    p_person->Speed = calc_person_speed(p_person);
    return res;
}

void person_grp_switch_to_specific_weapon(struct Thing *p_person, PlayerIdx plyr,
  WeaponType wtype, ubyte first_flag)
{
    struct Thing *p_owntng;
    ushort plagent;
    ubyte flag;

    p_owntng = p_person;
    if (p_person->State == PerSt_PROTECT_PERSON)
        p_owntng = &things[p_person->Owner];

    peep_change_weapon(p_person);
    flag = thing_select_specific_weapon(p_person, wtype, first_flag);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
        play_disk_sample(local_player_no, 44, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if ((p_agent->State != PerSt_PROTECT_PERSON) || (p_agent->Owner != p_owntng->ThingOffset)) {
            if (p_agent != p_owntng)
                continue;
        }
        if (p_agent == p_person)
            continue;

        if (!person_carries_weapon(p_agent, wtype) || (flag == WepSel_HIDE))
        {
            stop_looped_weapon_sample(p_agent, p_agent->U.UPerson.CurrentWeapon);
            if (flag == WepSel_HIDE)
            {
                thing_deselect_weapon(p_agent);
            }
            else if (p_agent->U.UPerson.TempWeapon != WEP_NULL)
            {
                thing_select_specific_weapon(p_agent, p_agent->U.UPerson.TempWeapon, flag);
            }
            else
            {
                thing_select_best_weapon_for_range(p_agent, 1280);
            }
        }
        else
        {
            peep_change_weapon(p_agent);
            thing_select_specific_weapon(p_agent, wtype, flag);
        }
        p_agent->U.UPerson.TempWeapon = p_agent->U.UPerson.CurrentWeapon;
    }
}

void cheat_teleport_agent_to_gnd(struct Thing *p_person, MapCoord cor_x, MapCoord cor_z)
{
    remove_path(p_person);
    if (on_mapwho(p_person))
        delete_node(p_person);
    p_person->U.UPerson.OnFace = 0; // Can only teleport to ground
    p_person->X = MAPCOORD_TO_PRCCOORD(cor_x, 0);
    p_person->Z = MAPCOORD_TO_PRCCOORD(cor_z, 0);
    p_person->Y = alt_at_point(cor_x, cor_z);
    add_node_thing(p_person->ThingOffset);
}

void person_give_all_weapons(struct Thing *p_person)
{
    WeaponType wtype;

    for (wtype = WEP_TYPES_COUNT-1; wtype > WEP_NULL; wtype--)
    {
        struct WeaponDef *wdef;
        ulong wepflg;

        wdef = &weapon_defs[wtype];

        if ((wdef->Flags & WEPDFLG_CanPurchease) == 0)
            continue;

        wepflg = 1 << (wtype-1);
        p_person->U.UPerson.WeaponsCarried |= wepflg;
    }
    player_agent_set_weapon_quantities_max(p_person);
    person_weapons_update_previous(p_person);
}

void mark_all_weapons_researched(void)
{
    WeaponType wtype;

    for (wtype = WEP_TYPES_COUNT-1; wtype > WEP_NULL; wtype--)
    {
        struct WeaponDef *wdef;

        wdef = &weapon_defs[wtype];

        if ((wdef->Flags & WEPDFLG_CanPurchease) == 0)
            continue;

        research_weapon_complete(wtype);
    }
}

void resurrect_any_dead_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;

        if ((p_agent->Flag & TngF_Destroyed) != 0)
            person_resurrect(p_agent);
        else if (p_agent->State == PerSt_PERSON_BURNING)
            person_burning_stifle_fire(p_agent);
    }
}

void give_all_weapons_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_give_all_weapons(p_agent);
    }
    mark_all_weapons_researched();
}

void give_best_mods_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_give_best_mods(p_agent);
    }
}

void set_max_stats_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_set_helath_to_max_limit(p_agent);
        person_set_energy_to_max_limit(p_agent);
        person_set_persuade_power__to_allow_all(p_agent);
    }
}

int net_unkn_func_12(void *a1)
{
    int ret;
    asm volatile ("call ASM_net_unkn_func_12\n"
        : "=r" (ret) : "a" (a1));
    return ret;
}

void net_unkn_check_1(void)
{
    char locstr[100];
    ushort i, m;
    s32 check_val;
    char recvd2[8];
    char recvd3[8];
    char recvd[8];

    LbMemorySet(recvd, 0, 8);
    if ((PacketRecord_IsPlayback()) && in_network_game)
    {
        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            if (((1 << i) & ingame.InNetGame_UNSURE) != 0) {
                PacketRecord_Read(&packets[i], players[i].DoubleMode);
            }
        }
    }
    else if (nsvc.I.Type == NetSvc_IPX)
    {
        struct Packet *p_pckt;
        int ret;

        p_pckt = &packets[local_player_no];
        p_pckt->D1Seed = lbSeed;
        p_pckt->D2Check = ingame.fld_unkC4B;
        net_unkn_func_12(recvd2);
        ret = LbNetworkExchange(packets, sizeof(struct Packet));
        if (gameturn == 5) {
            LbNetworkSetTimeoutSec(15);
        }
        if (ret == -1)
        {
            net_unkn_func_12(recvd3);
            for (i = 0; i < PLAYERS_LIMIT; i++)
            {
                if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
                    continue;
                if (recvd2[i] == recvd3[i])
                    continue;

                sprintf(locstr, " Player >%s< Has Timed Out", net_player_names[i]);
                show_message(locstr);
                ingame.InNetGame_UNSURE &= ~(1 << i);
                if (i == local_player_no)
                {
                  show_message("You have Timed out from the system");
                  ingame.DisplayMode = DpM_PURPLEMNU;
                  StopCD();
                  StopAllSamples();
                }
                net_players_num--;
            }
        }
        else if (ret == -8)
        {
            show_message(" Host Connection Lost");
            ingame.InNetGame_UNSURE = 0;
            ingame.DisplayMode = DpM_PURPLEMNU;
            StopCD();
            StopAllSamples();
        }
    }
    else
    {
        struct Packet *p_pckt;
        struct ShortPacket *p_shpckt;

        p_pckt = &packets[local_player_no];
        p_shpckt = &shpackets[local_player_no];
        p_shpckt->Action = p_pckt->Action;
        p_shpckt->Data = p_pckt->Data;
        p_shpckt->X = p_pckt->X;
        p_shpckt->Y = p_pckt->Y;
        p_shpckt->Z = p_pckt->Z;
        p_shpckt->BCheck = ingame.fld_unkC4B;

        LbNetworkExchange(shpackets, sizeof(struct ShortPacket));

        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            struct Packet *p_pckt;
            struct ShortPacket *p_shpckt;

            p_pckt = &packets[i];
            p_shpckt = &shpackets[i];
            p_pckt->Action = p_shpckt->Action;
            p_pckt->Action2 = 0;
            p_pckt->Action3 = 0;
            p_pckt->Action4 = 0;
            p_pckt->Data = p_shpckt->Data;
            p_pckt->X = p_shpckt->X;
            p_pckt->Y = p_shpckt->Y;
            p_pckt->Z = p_shpckt->Z;
            p_pckt->D1Seed = lbSeed;
            p_pckt->D2Check = p_shpckt->BCheck;
        }
    }

    if (nsvc.I.Type == NetSvc_IPX)
        check_val = ingame.fld_unkC4B;
    else
        check_val = (ubyte)ingame.fld_unkC4B;
    if (nsvc.I.Type == NetSvc_IPX)
    {
        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
                continue;
            for (m = 0; m < PLAYERS_LIMIT; m++)
            {
                if ( ((1 << m) & ingame.InNetGame_UNSURE) == 0)
                    continue;
                if (packets[m].D2Check == packets[i].D2Check)
                    recvd[i]++;
            }
        }
    }

    for (i = 0; i < PLAYERS_LIMIT; i++)
    {
        if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
            continue;

        if (PacketRecord_IsRecord() && in_network_game && (net_host_player_no == local_player_no))
            PacketRecord_Write(&packets[i], players[i].DoubleMode);

        if ((nsvc.I.Type == NetSvc_IPX) && (recvd[i] == 1) && (net_players_num > 2))
        {
            sprintf(locstr, " Player >%s< is out of sync", net_player_names[i]);
            show_message(locstr);
        }
        if (check_val != packets[i].D2Check)
        {
          show_message(" CHECK ERROR ");
          clear_gamekey_pressed(GKey_MISSN_RESTART);
          in_network_game = 2;
          StopCD();
          test_missions(1);
          init_level_3d(1);
          restart_back_into_mission(ingame.CurrentMission);
          in_network_game = 1;
          return;
        }
    }
}

void player_chat_message_add_key(ushort a1, int a2)
{
    asm volatile ("call ASM_player_chat_message_add_key\n"
        :  : "a" (a1), "d" (a2));
}

void plgroup_set_mood(PlayerIdx plyr, struct Thing *p_member, short mood)
{
    struct Thing *p_owntng;
    ushort plagent;

    p_owntng = p_member;
    if (p_member->State == PerSt_PROTECT_PERSON)
        p_owntng = &things[p_member->Owner];

    p_member->U.UPerson.Mood = limit_mood(p_member, mood);
    p_member->Speed = calc_person_speed(p_member);

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if ((p_agent <= &things[0]) || (p_agent >= &things[THINGS_LIMIT]))
            continue;

        if ((p_agent->State != PerSt_PROTECT_PERSON) || (p_agent->Owner != p_owntng->ThingOffset)) {
            if (p_agent != p_owntng)
                continue;
        }
        if (p_agent == p_member) // already updated
            continue;

        p_agent->U.UPerson.Mood = limit_mood(p_agent, mood);
        p_agent->Speed = calc_person_speed(p_agent);
    }
}

void player_agent_init_goto_gnd_point_abs(PlayerIdx plyr, struct Thing *p_person,
  MapCoord cor_x, MapCoord cor_y, MapCoord cor_z)
{
    if (plyr == local_player_no)
        show_goto_point(1);
    p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
    thing_goto_point(p_person, cor_x, cor_y, cor_z);
}

void player_agent_select_specific_weapon(PlayerIdx plyr, struct Thing *p_person,
  WeaponType wtype, ubyte flag)
{
    thing_select_specific_weapon(p_person, wtype, flag);
    peep_change_weapon(p_person);
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        ushort smp;
        smp = weapon_sound_name_speech_index(p_person->U.UPerson.CurrentWeapon);
        play_disk_sample(local_player_no, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
}

void process_packet(PlayerIdx plyr, struct Packet *p_pckt, ushort i)
{
    struct Thing *p_thing;
    struct Thing *p_sectng;
    int n;
    short result;

    result = PARes_EBADRQC;
    switch (p_pckt->Action & ~PActF_All)
    {
    case PAct_MISSN_ABORT:
        if (in_network_game) {
            net_player_leave(plyr);
            result = PARes_DONE;
            break;
        }
        exit_game = 1;
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_ABS:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        player_agent_init_goto_gnd_point_abs(plyr, p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_REL:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_goto_point_rel(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_SELECT_NEXT_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_weapon_switch(plyr, p_pckt->Data, 1);
        result = PARes_DONE;
        break;
    case PAct_DROP_SELC_WEAPON_SECR:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        if (p_thing->U.UPerson.CurrentWeapon == 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_init_drop_item(plyr, p_thing, 0);
        result = PARes_DONE;
        break;
    case PAct_PICKUP:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_pickup(p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_ENTER_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_enter_vehicle(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_LEAVE_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_attempt_to_leave_vehicle(p_thing);
        result = PARes_DONE;
        break;
    case PAct_SELECT_AGENT:
        p_sectng = get_thing_safe(p_pckt->X, TT_PERSON);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_sectng, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        player_agent_select(plyr, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_REL_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_goto_point_rel_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, plyr);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_GND_POINT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SELECT_PREV_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_weapon_switch(plyr, p_pckt->Data, -1);
        result = PARes_DONE;
        break;
    case PAct_PROTECT_INC:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        call_protect(p_thing, plyr);
        n = count_protect(p_thing, plyr);
        if (plyr == local_player_no && n)
            play_sample_using_heap(0, 61, FULL_VOL, EQUL_PAN, 90 + 5 * n, LOOP_NO, 3);
        result = PARes_DONE;
        break;
    case PAct_PROTECT_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        call_unprotect(p_thing, plyr, 0);
        n = count_protect(p_thing, plyr);
        if (plyr == local_player_no && n)
            play_sample_using_heap(0, 61, FULL_VOL, EQUL_PAN, 90 + 5 * n, LOOP_NO, 3);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_THING:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_thing(p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_GET_ITEM:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_get_item(p_thing, p_pckt->X, plyr);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_GND_PT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SELECT_SPECIFIC_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (p_pckt->Y > WepSel_HIDE) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_select_specific_weapon(plyr, p_thing, p_pckt->X, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_DROP_HELD_WEAPON_SECR:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_init_drop_item(plyr, p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_SET_MOOD:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        p_thing->U.UPerson.Mood = limit_mood(p_thing, p_pckt->X);
        p_thing->Speed = calc_person_speed(p_thing);
        result = PARes_DONE;
        break;
    case PAct_GO_ENTER_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        person_go_enter_vehicle(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_FOLLOW_PERSON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_PERSON);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_follow_person(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_CONTROL_MODE:
        if ((p_pckt->Data & ~UInpCtr_AllFlagsMask) >= UInpCtr_MODES_COUNT) {
            result = PARes_EINVAL;
            break;
        }
        user_input_control_mode_set(plyr, 0, p_pckt->Data);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_FACE_PT_ABS:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_on_face(p_thing, p_pckt->X, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_ABS_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, plyr);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_FACE_PT_ABS_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_on_face_fast(p_thing, p_pckt->X, p_pckt->Z, p_pckt->Y, plyr);
        result = PARes_DONE;
        break;
    case PAct_GO_ENTER_VEHICLE_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_go_enter_vehicle_fast(p_thing, p_sectng, plyr);
        result = PARes_DONE;
        break;
    case PAct_GET_ITEM_FAST:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_get_item_fast(p_thing, p_pckt->X, plyr);
        result = PARes_DONE;
        break;
    case PAct_SHIELD_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!person_can_toggle_supershield(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_supershield_toggle(p_thing);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_GND_PT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_GND_POINT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 1);
        result = PARes_DONE;
        break;
    case PAct_PEEPS_SCATTER:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        make_peeps_scatter(p_thing, p_pckt->X, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_SELECT_GRP_SPEC_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (p_pckt->Y > WepSel_SKIP) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_grp_switch_to_specific_weapon(p_thing, plyr, p_pckt->X, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_USE_MEDIKIT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!person_can_use_medikit(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_use_medikit(p_thing, plyr);
        result = PARes_DONE;
        break;
    case PAct_GROUP_SET_MOOD:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        plgroup_set_mood(plyr, p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_UNKGROUP_PROT: // Unfinished mess; remove pending
        unkn_player_group_prot(p_pckt->Data, plyr);
        result = PARes_DONE;
        break;
    case PAct_AGENT_UNKGROUP_ADD: // Unfinished mess; remove pending
        unkn_player_group_add(p_pckt->Data, plyr);
        result = PARes_DONE;
        break;
    case PAct_THERMAL_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!player_can_toggle_thermal(plyr)) {
            result = PARes_TNGBADST;
            break;
        }
        player_toggle_thermal(plyr);
        result = PARes_DONE;
        break;
    case PAct_CHAT_MESSAGE_KEY:
        player_chat_message_add_key(plyr, p_pckt->Data);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_FACE_POINT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 2);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_FACE_POINT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 3);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_FACE_PT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine(p_thing, p_pckt->X, 0, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_FACE_PT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine_fast(p_thing, p_pckt->X, 0, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_SELF_DESTRUCT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_self_destruct(p_thing);
        result = PARes_DONE;
        break;
    case PAct_CHEAT_AGENT_TELEPORT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        cheat_teleport_agent_to_gnd(p_thing, p_pckt->X, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_CHEAT_ALL_AGENTS:
        switch (p_pckt->Data)
        {
        case PCheatAA_RESURRECT_AND_WEPAPN:
            resurrect_any_dead_agents(plyr);
            give_all_weapons_to_all_agents(plyr);
            player_agents_clear_weapon_delays(plyr);
            result = PARes_DONE;
            break;
        case PCheatAA_BEEFUP_AND_MODS:
            result = PARes_DONE;
            give_best_mods_to_all_agents(plyr);
            set_max_stats_to_all_agents(plyr);
            break;
        default:
            result = PARes_EINVAL;
            break;
        }
        break;
    case PAct_NONE:
        result = PARes_DONE;
        break;
    }
    if ((debug_log_things & 0x02) != 0)
    {
        char locstr[192];

        snprint_packet(locstr, sizeof(locstr), p_pckt);

        if ((p_pckt->Action & ~PActF_All) == PAct_NONE)
            ; // no logging for empty packet
        else if (result <= PARes_SUCCESS)
            LOGSYNC_F("Player %d packet %s: %s", (int)plyr,
              locstr, get_packet_action_result_text(result));
        else
            LOGWARN("Player %d packet %s: %s", (int)plyr,
              locstr, get_packet_action_result_text(result));
    }
}

#ifdef SW_FNCHK
/** fncheck test aid (SW_TEST_CMDS set): every few turns the local player's
 * agents get commands which recorded games seldom have, so the remade
 * functions behind them run under fncheck. */
static struct Thing *test_cmds_nearest_thing(struct Thing *p_from, ubyte ttype,
  TbBool need_faces4)
{
    struct Thing *p_best;
    ThingIdx thing;
    s32 best;

    p_best = NULL;
    best = 0x7FFFFFFF;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing;
        s32 dx, dz, d;

        p_thing = &things[thing];
        if (p_thing->Type != ttype)
            continue;
        if (need_faces4 && (game_objects[p_thing->U.UObject.Object].NumbFaces4 == 0))
            continue;
        dx = (p_thing->X - p_from->X) >> 8;
        dz = (p_thing->Z - p_from->Z) >> 8;
        d = abs(dx) + abs(dz);
        if ((d < best) && (d > 0x100)) {
            best = d;
            p_best = p_thing;
        }
    }
    return p_best;
}

static void test_cmds_process(void)
{
    static int enabled = -1;
    PlayerInfo *p_player;
    struct Thing *p_agent;
    ushort n;

    if (enabled < 0)
        enabled = (getenv("SW_TEST_CMDS") != NULL);
    if (!enabled || (gameturn < 20))
        return;
    p_player = &players[local_player_no];
    n = (gameturn / 10) % playable_agents;
    p_agent = p_player->MyAgent[n];
    if ((p_agent == NULL) || ((p_agent->Flag & TngF_Destroyed) != 0))
        return;
    if ((gameturn & 3) == 0)
    {
        ThingIdx mgun;

        // aim the first mounted gun at the agent; blow it up at turn 900
        for (mgun = things_used_head; mgun > 0; mgun = things[mgun].LinkChild) {
            if ((things[mgun].Type == TT_BUILDING) && (things[mgun].SubType == 0x20) &&
              ((things[mgun].Flag & TngF_Destroyed) == 0))
                break;
        }
        if (mgun > 0)
        {
            things[mgun].PTarget = p_agent;
            track_target(&things[mgun]);
            rotate_object(&things[mgun]);
            if (gameturn == 900)
                init_mgun_explode(&things[mgun]);
        }
    }
    switch (gameturn % 120)
    {
    case 65:
        get_next_player_agent(local_player_no);
        break;
    case 60:
    case 61:
    case 62:
    case 63:
    {
        struct Thing *p_bld;

        p_bld = test_cmds_nearest_thing(p_agent, TT_BUILDING, false);
        if (p_bld != NULL)
            can_i_see_building(p_agent, p_bld, 0x100000 >> ((gameturn % 4) * 3), gameturn & 1);
        break;
    }
    case 66:
    {
        struct Thing *p_pers;

        p_pers = test_cmds_nearest_thing(p_agent, TT_PERSON, false);
        if (p_pers != NULL)
            get_soul(p_pers, p_agent);
        break;
    }
    case 67:
        if ((gameturn % 480) < 120)
            create_time_pod((p_agent->X >> 8) + 0x300, p_agent->Y >> 8, p_agent->Z >> 8, 50);
        break;
    case 68:
    case 69:
        on_mapwho(p_agent);
        break;
    case 72:
        map_things_unkn_func_04(0);
        break;
    case 80:
    case 81:
    case 82:
    {
        struct Thing *p_pers;
        ushort grp;
        short ax, az;

        p_pers = test_cmds_nearest_thing(p_agent, TT_PERSON, false);
        grp = (p_pers != NULL) ? p_pers->U.UPerson.Group : p_agent->U.UPerson.Group;
        ax = p_agent->X >> 8;
        az = p_agent->Z >> 8;
        all_group_arrived(grp, ax, p_agent->Y >> 8, az, 1 + (gameturn / 120) % 16);
        all_group_arrived_square(grp, ax - 0x400, az - 0x400, ax + 0x400, az + 0x400);
        group_not_seen(grp);
        if ((p_pers != NULL) && ((p_pers->Flag & TngF_PlayerAgent) == 0))
            person_intel(p_pers);
        if (unkn_mech_arr3 != NULL)
            mech_unkn_func_05(0, gameturn & 1, (gameturn / 120) % 4);
        break;
    }
    case 79:
        init_laser_guided(p_agent, 32);
        break;
    case 77:
    case 78:
    {
        static struct Command test_cmd;

        test_cmd.X = p_agent->X >> 8;
        test_cmd.Z = p_agent->Z >> 8;
        test_cmd.Arg1 = 1 + (gameturn / 120) % 12;
        test_cmd.Flags = (gameturn & 1) ? 0x08 : 0;
        find_peep_in_area(p_agent, &test_cmd);
        break;
    }
    case 76:
        if ((gameturn % 240) < 120)
        {
            struct SimpleThing *p_bomb;

            // a gas canister like from a grenade, subtype 4 or 5
            p_bomb = init_nuclear_bomb((p_agent->X >> 8) - 0x200, p_agent->Y >> 8, p_agent->Z >> 8);
            if (p_bomb != NULL) {
                p_bomb->Type = SmTT_CANISTER;
                p_bomb->SubType = ((gameturn % 480) < 240) ? 4 : 5;
                p_bomb->Owner2 = p_agent->ThingOffset;
            }
        }
        break;
    case 74:
    {
        struct Thing *p_pers;

        // a nearby person starts supporting the agent
        p_pers = NULL;
        {
            ThingIdx thing;
            s32 best = 0x7FFFFFFF;
            for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
            {
                struct Thing *p_thing = &things[thing];
                s32 d;
                if ((p_thing->Type != TT_PERSON) ||
                  ((p_thing->Flag & (TngF_PlayerAgent | TngF_Destroyed)) != 0))
                    continue;
                d = abs((p_thing->X - p_agent->X) >> 8) + abs((p_thing->Z - p_agent->Z) >> 8);
                if (d < best) {
                    best = d;
                    p_pers = p_thing;
                }
            }
        }
        if (p_pers != NULL) {
            p_pers->State = PerSt_SUPPORT_PERSON;
            p_pers->GotoThingIndex = p_agent->ThingOffset;
        }
        break;
    }
    case 73:
        map_things_unkn_func_04(0x20);
        break;
    case 20:
        create_crater((p_agent->X >> 16) + 2, (p_agent->Z >> 16) + 1, 20 + (gameturn % 50));
        break;
    case 50:
    case 110:
    {
        struct Thing *p_bld;
        struct SingleObject *p_obj;

        p_bld = test_cmds_nearest_thing(p_agent, TT_BUILDING, true);
        if (p_bld == NULL)
            break;
        p_obj = &game_objects[p_bld->U.UObject.Object];
        if (((gameturn % 240) < 120) || (p_obj->NumbFaces == 0))
            build_navigate_path_to_face(p_agent, p_obj->StartFace4 + (gameturn % p_obj->NumbFaces4));
        else
            build_navigate_path_to_face(p_agent, -(short)(p_obj->StartFace + (gameturn % p_obj->NumbFaces)));
        break;
    }
    case 35:
        call_protect(p_agent, local_player_no);
        count_protect(p_agent, local_player_no);
        break;
    case 95:
        call_unprotect(p_agent, local_player_no, ((gameturn % 480) < 240) ? 0 : 2);
        count_protect(p_agent, local_player_no);
        break;
    case 15:
    case 45:
    case 75:
        player_agent_weapon_switch(local_player_no, p_agent->ThingOffset,
          ((gameturn % 240) < 120) ? 1 : -1);
        break;
    case 5:
        make_peeps_scatter(p_agent, (p_agent->X >> 8) + 0x200, (p_agent->Z >> 8) - 0x100);
        break;
    case 25:
    case 85:
    {
        struct Thing *p_veh;

        p_veh = test_cmds_nearest_thing(p_agent, TT_VEHICLE, false);
        if (p_veh == NULL)
            break;
        if ((gameturn % 240) < 120)
            person_go_enter_vehicle(p_agent, p_veh);
        else
            person_go_enter_vehicle_fast(p_agent, p_veh, local_player_no);
        break;
    }
    case 55:
    case 115:
    {
        ThingIdx thing, best_thing;
        s32 best;

        best = 0x7FFFFFFF;
        best_thing = 0;
        for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
        {
            struct SimpleThing *p_sthing;
            s32 d;

            p_sthing = &sthings[thing];
            if (p_sthing->Type != SmTT_DROPPED_ITEM)
                continue;
            d = abs((p_sthing->X - p_agent->X) >> 8) + abs((p_sthing->Z - p_agent->Z) >> 8);
            if (d < best) {
                best = d;
                best_thing = thing;
            }
        }
        if (best_thing == 0)
            break;
        if ((gameturn % 240) < 120)
            person_init_get_item_fast(p_agent, best_thing, local_player_no);
        else
            should_i_get_item(p_agent, &sthings[best_thing]);
        break;
    }
    case 10:
    case 70:
    {
        short dx, dz;
        dx = lbSinTable[(gameturn * 37) & 0x7FF] >> 8;
        dz = lbSinTable[((gameturn * 37) + 512) & 0x7FF] >> 8;
        if ((gameturn % 240) < 120)
            thing_goto_point_rel(p_agent, dx, 0, dz);
        else
            thing_goto_point_rel_fast(p_agent, dx, 0, dz, local_player_no);
        break;
    }
    case 40:
    case 100:
    {
        struct Thing *p_bld;
        struct SingleObject *p_obj;
        short face;

        p_bld = test_cmds_nearest_thing(p_agent, TT_BUILDING, true);
        if (p_bld == NULL)
            break;
        p_obj = &game_objects[p_bld->U.UObject.Object];
        face = -(short)(p_obj->StartFace4 + (gameturn % p_obj->NumbFaces4));
        if ((gameturn % 240) < 120)
            thing_goto_point_on_face(p_agent, p_bld->X >> 8, p_bld->Z >> 8, face);
        else
            thing_goto_point_on_face_fast(p_agent, p_bld->X >> 8, p_bld->Z >> 8, face,
              local_player_no);
        break;
    }
    }
}
#endif

void process_packets(void)
{
    PlayerIdx plyr;

#ifdef SW_FNCHK
    test_cmds_process();
#endif

    if (in_network_game && (net_players_num > 1))
        net_unkn_check_1();

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        struct Packet *packet;
        ushort i;

        if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
            continue;
        packet = &packets[plyr];
        for (i = 0; i < LOCAL_USERS_MAX_COUNT; i++)
        {
            struct Thing *p_thing;

            p_thing = get_thing_safe(packet->Data, TT_PERSON);

            if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
                packet->Action = PAct_NONE;

            if (p_thing != INVALID_THING)
            {
                if ((packet->Action & PActF_TriggerUse) == 0)
                    p_thing->Flag &= ~TngF_TriggerUse;
                else
                    p_thing->Flag |= TngF_TriggerUse;
            }

            process_packet(plyr, packet, i);

            packet->Action = PAct_NONE;
            packet = (struct Packet *)((ubyte *)packet + 10);
        }
    }
}

/******************************************************************************/
