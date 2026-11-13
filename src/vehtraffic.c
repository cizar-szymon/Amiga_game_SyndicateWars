/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vehtraffic.c
 *     Pathfinding triangulation support required by the 3D engine.
 * @par Purpose:
 *     Implement functions for handling pathfinding triangulation.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "vehtraffic.h"

#include <stdlib.h>
#include <assert.h>
#include "bfutility.h"
#include "bigmap.h"
#include "building.h"
#include "game.h"
#include "game_speed.h"
#include "thing.h"
#include "vehicle.h"
#include "swlog.h"
#include "fncheck.h"
/******************************************************************************/
struct TrafficNode *game_traffic_nodes = NULL;
ushort next_traffic_node = 1;

extern uint dword_1C8470;
extern int dword_1C8474[32];

short tnode_find_used_link(struct TrafficNode *p_tnode)
{
    short i;
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] != 0)
            return i;
    }
    return -1;
}

void tnode_unlink_thing(short tnode, ThingIdx thing)
{
    struct TrafficNode *p_tnode;
    int i;

    p_tnode = &game_traffic_nodes[tnode];
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] == thing)
            p_tnode->UTraffic.Link[i] = 0;
    }
}

void tnode_all_unlink_thing(ThingIdx thing)
{
    short tnode;
    for (tnode = 1; tnode < next_traffic_node; tnode++) {
        tnode_unlink_thing(tnode, thing);
    }
}

#ifdef SW_FNCHK
static short get_next_tnode_asm(struct Thing *p_vehicle, struct TrafficNode *p_tnode)
{
    int ret;
    asm volatile (
      "call ASM_get_next_tnode\n"
        : "=a" (ret) : "a" (p_vehicle), "d" (p_tnode) : "cc", "memory");
    return ret;
}
#endif

/** Does a link lead somewhere a vehicle can go on: to a station, or to a
 * traffic node with a link not marked 0x80 - itself or, if all are marked,
 * via its last link.  Remade from good_node.
 */
static TbBool good_node(short link)
{
    struct TrafficNode *p_tnode;
    int k, last;
    short link2;

    if (link > 0)
        return true;
    p_tnode = &game_traffic_nodes[-link];
    last = -1;
    for (k = 0; k < 4; k++)
    {
        if (p_tnode->UTraffic.Link[k] == 0)
            continue;
        if ((p_tnode->UTraffic.Flags[k] & 0x80) == 0)
            return true;
        last = k;
    }
    if (last < 0)
        return false;
    link2 = p_tnode->UTraffic.Link[last];
    if (link2 > 0)
        return true;
    p_tnode = &game_traffic_nodes[-link2];
    for (k = 0; k < 4; k++)
    {
        if ((p_tnode->UTraffic.Link[k] != 0) && ((p_tnode->UTraffic.Flags[k] & 0x80) == 0))
            return true;
    }
    return false;
}

/** The vehicle's turn history (LeisurePlace): shifted on each node, bit 1
 * set for a turn into a marked link. */
static void tnode_history_shift(struct Thing *p_vehicle, TbBool marked)
{
    p_vehicle->U.UVehicle.LeisurePlace = (ushort)((short)p_vehicle->U.UVehicle.LeisurePlace >> 1);
    if (marked)
        p_vehicle->U.UVehicle.LeisurePlace |= 0x02;
}

/** Is the link usable: not to a traffic node with flags 0x1800. */
static TbBool tnode_link_open(short link)
{
    if (link >= 0)
        return true;
    return ((game_traffic_nodes[-link].Flags & 0x1800) == 0);
}

/** Chooses the link a vehicle on a traffic node leaves by: the only one,
 * else a random one fitting the turn rules (up to 8 tries), else the first
 * open one.  Remade from get_next_tnode.
 */
static short get_next_tnode_c(struct Thing *p_vehicle, struct TrafficNode *p_tnode)
{
    int k, n, last, tries;

    n = 0;
    last = 0;
    for (k = 0; k < 4; k++)
    {
        if (p_tnode->UTraffic.Link[k] != 0) {
            last = k;
            n++;
        }
    }
    if (n == 0)
        return 0;
    if (n == 1)
    {
        p_vehicle->U.UVehicle.WorkPlace |= 0x0004;
        tnode_history_shift(p_vehicle, (p_tnode->UTraffic.Flags[last] & 0x80) != 0);
        return last;
    }
    for (tries = 0; tries < 8; tries++)
    {
        short link;

        k = LbRandomAnyShort() & 3;
        link = p_tnode->UTraffic.Link[k];
        if (link == 0)
            continue;
        if (!tnode_link_open(link))
            continue;
        if ((p_tnode->UTraffic.Flags[k] & 0x80) == 0)
        {
            tnode_history_shift(p_vehicle, false);
            return k;
        }
        {
            struct TrafficNode *p_tnode2;
            short history;

            // also when the link leads to a station - as the original
            p_tnode2 = &game_traffic_nodes[-link];
            if ((p_tnode2->Flags & 0x40) != 0)
                continue;
            history = (short)p_vehicle->U.UVehicle.LeisurePlace;
            if (history == 2)
            {
                TbBool ok;

                ok = false;
                if (link > 0) {
                    ok = true;
                } else {
                    int j;
                    for (j = 0; j < 4; j++) {
                        if ((p_tnode2->UTraffic.Link[j] != 0) &&
                          ((p_tnode2->UTraffic.Flags[j] & 0x80) == 0)) {
                            ok = true;
                            break;
                        }
                    }
                }
                if (!ok)
                    continue;
            }
            else if (history == 3)
            {
                continue;
            }
            else if (!good_node(link))
            {
                continue;
            }
            tnode_history_shift(p_vehicle, true);
            return k;
        }
    }
    for (k = 0; k < 4; k++)
    {
        short link = p_tnode->UTraffic.Link[k];
        if ((link != 0) && ((p_tnode->UTraffic.Flags[k] & 0x80) == 0) && tnode_link_open(link)) {
            tnode_history_shift(p_vehicle, false);
            return k;
        }
    }
    for (k = 0; k < 4; k++)
    {
        short link = p_tnode->UTraffic.Link[k];
        if ((link != 0) && tnode_link_open(link)) {
            tnode_history_shift(p_vehicle, true);
            return k;
        }
    }
    for (k = 0; k < 4; k++)
    {
        if (p_tnode->UTraffic.Link[k] != 0) {
            tnode_history_shift(p_vehicle, (p_tnode->UTraffic.Flags[k] & 0x80) != 0);
            return k;
        }
    }
    return 0;
}

short get_next_tnode(struct Thing *p_vehicle, struct TrafficNode *p_tnode)
{
    short ret;
    FNCHK_RET("get_next_tnode", ret, get_next_tnode_asm(p_vehicle, p_tnode),
      get_next_tnode_c(p_vehicle, p_tnode));
    return ret;
}

void add_agphase1_lock(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.TNode;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags |= TNdF_Unkn0040;
    p_tnode->GateLink = p_vehicle->ThingOffset;
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgPhase1;
}

void remove_agphase1_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase1;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agphase1_lock(struct Thing *p_vehicle)
{
    remove_agphase1_node_lock(p_vehicle, p_vehicle->U.UVehicle.TNode);
}

void promote_agphase1_lock(struct Thing *p_vehicle)
{
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgPhase2;
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase1;
}

void remove_agphase2_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase2;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agphase2_lock(struct Thing *p_vehicle)
{
    remove_agphase2_node_lock(p_vehicle, p_vehicle->U.UVehicle.Dummy4b);
}

void promote_agphase2_lock(struct Thing *p_vehicle)
{
    p_vehicle->U.UVehicle.Agok = p_vehicle->U.UVehicle.Dummy4b;
    p_vehicle->U.UVehicle.WorkPlace &= ~(VWPFlg_HasAgPhase2|VWPFlg_HasAgok);
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgok;
}

void add_agok_lock(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.Agok;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags |= TNdF_Unkn0040;
    p_tnode->GateLink = p_vehicle->ThingOffset;
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgok;
}

void remove_agok_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgok;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agok_lock(struct Thing *p_vehicle)
{
    remove_agok_node_lock(p_vehicle, p_vehicle->U.UVehicle.Agok);
}

void remove_locks(struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_remove_locks\n"
        : : "a" (p_vehicle));
    return;
#endif
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
        remove_agphase1_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
        remove_agphase2_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
        remove_agok_lock(p_vehicle);
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
}

void remove_locks_apart_from_agok(struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_remove_locks_apart_from_agok\n"
        : : "a" (p_vehicle));
    return;
#endif
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
        remove_agphase1_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
        remove_agphase2_lock(p_vehicle);
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
}

void update_workplace_unkn0001_flag(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.TNode;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    if ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0)
        p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0001;
    else
        p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0001;
}

static TbBool check_person_close_on_mapel(struct MyMapElement *p_mapel)
{
    ThingIdx thing;

    thing = p_mapel->Child;
    while (thing != 0)
    {
      if (thing > 0)
      {
          struct Thing *p_thing;
          p_thing = &things[thing];
          switch (p_thing->Type)
          {
          case TT_PERSON:
              if ((p_thing->State != PerSt_DEAD)
                && (p_thing->State != PerSt_PERSON_BURNING)
                && (p_thing->State != PerSt_DIEING)
                //&& ((p_thing->Flag & TngF_InVehicle) == 0) -- people in vehicles should not be within the list
                && ((p_thing->Flag2 & TgF2_KnockedOut) == 0))
                  return true;
              break;
          default:
              break;
          }
          thing = p_thing->Next;
      }
      else
      {
          struct SimpleThing *p_sthing;
          p_sthing = &sthings[thing];
          thing = p_sthing->Next;
      }
    }
    return false;
}

int check_person_close(struct TrafficNode *p_tnode)
{
#if 0
    int ret;
    asm volatile (
      "call ASM_check_person_close\n"
        : "=r" (ret) : "a" (p_tnode));
    return ret;
#endif
    int tile_ctr_x, tile_ctr_z;
    int tlno;
    int tile_x;

    tile_ctr_z = p_tnode->Z >> 8;
    tile_ctr_x = p_tnode->X >> 8;

    for (tile_x = tile_ctr_x - 1; tile_x <= tile_ctr_x + 1; tile_x++)
    {
        tlno = 128 * (tile_ctr_z - 1);
        while (tlno < 128 * tile_ctr_z + 256)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[tlno + tile_x];
            if (check_person_close_on_mapel(p_mapel))
                return true;
            tlno += 128;
        }
    }
    return 0;
}

void update_vehicle_elevation(struct Thing *p_vehicle, short statn)
{
    short angleX;

    angleX = 0;
    if (statn > 0)
    {
        struct Thing *p_station;
        short nxstatn;

        p_station = &things[statn];
        if ((p_station->Flag & TngF_Destroyed) != 0)
        {
            LOGERR("Crashing vehicle %d due to destroyed station %d",
              p_vehicle->ThingOffset, statn);
            start_crashing(p_vehicle);
            return;
        }
        if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
            nxstatn = p_station->U.UObject.NextThing;
        else
            nxstatn = p_station->U.UObject.PrevThing;

        if (nxstatn > 0)
        {
            int cux, cuy, cuz, nxx, nxy, nxz, faxz;
            struct Thing *p_nxstation;
            int dt_x, dt_y, dt_z;

            p_nxstation = &things[nxstatn];
            cux = (p_station->X >> 8) - 5 * p_station->U.UObject.OffX;
            nxx = (p_nxstation->X >> 8) - 5 * p_nxstation->U.UObject.OffX;
            cuz = (p_station->Z >> 8) - 5 * p_station->U.UObject.OffZ;
            nxz = (p_nxstation->Z >> 8) - 5 * p_nxstation->U.UObject.OffZ;
            cuy = (p_station->Y >> 3) + 3840;
            nxy = (p_nxstation->Y >> 3) + 3840;

            dt_x = (cux - nxx);
            dt_y = (cuy - nxy) >> 5;
            dt_z = (cuz - nxz);
            faxz = map_distance_deltas_fast(dt_x, dt_y, dt_z);
            angleX = arctan(abs(dt_y), faxz) - 1024;
            if (dt_y < 0)
                angleX = -angleX;
            p_vehicle->Y += (cuy - p_vehicle->Y) >> 3;
        }
    }

    if ((p_vehicle->State == VehSt_GOTO_LOC) && ((gameturn & 3) == 0)
      && ((p_vehicle->U.UVehicle.Dummy4a & 0x3F) > 2))
    {
        int dx, dz;

        dx = abs(p_vehicle->U.UVehicle.destx - (p_vehicle->X >> 8));
        dz = abs(p_vehicle->U.UVehicle.destz - (p_vehicle->Z >> 8));
        if (dx <= dz)
            dx >>= 1;
        else
            dz >>= 1;
        if (dx + dz < 768)
            p_vehicle->State = VehSt_UNKN_3C;
    }
    if (p_vehicle->U.UVehicle.AngleX < angleX)
        p_vehicle->U.UVehicle.AngleX += 4;
    if (p_vehicle->U.UVehicle.AngleX > angleX)
        p_vehicle->U.UVehicle.AngleX -= 4;
}

void update_vehicle_speed_and_direction(struct Thing *p_vehicle, int tndt_x, int tndt_z, long nxdist_sq)
{
    short angleY, angDY;

    angleY = p_vehicle->U.UVehicle.AngleY - arctan(tndt_x, tndt_z);
    if (angleY < -1024)
        angleY += 2048;
    else if (angleY > 1024)
        angleY -= 2048;

    if (angleY > 0)
    {
        if (angleY >= 50)
          angDY = (2048 - (p_vehicle->Speed >> 3)) >> 2;
        else
          angDY = 2 * angleY;
        angDY = -angDY;
    }
    else
    {
        if (angleY <= -50)
            angDY = (2048 - (p_vehicle->Speed >> 3)) >> 2;
        else
            angDY = -2 * angleY;
    }
    p_vehicle->U.UVehicle.AngleDY = angDY;

    if ((angleY > 512) || (angleY < -512))
        angleY = 512;
    if (p_vehicle->SubState == 2)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = nxdist_sq >= 0x8000 ? 2048 - 4 * abs(angleY) : 512;
        if (p_vehicle->U.UVehicle.ReqdSpeed < 10)
            p_vehicle->U.UVehicle.ReqdSpeed = 10;
    }
}

void process_vehicle_stop_for_pedestrians(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    struct Thing *p_gtveh;

    // Only Traffic Nodes support stopping for pedestrians
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return;

    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
    {
        if (p_vehicle->State == VehSt_GOTO_LOC || p_vehicle->State == VehSt_WANDER)
        {
            if ((p_tnode->Flags & TNdF_Unkn0040) != 0)
            {
                LOGDBG("Stopping %s thing %d rqspeed %d due to TNode %d flags",
                  vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                  (int)p_vehicle->U.UVehicle.ReqdSpeed, (int)p_vehicle->U.UVehicle.TNode);

                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_gtveh = &things[p_tnode->GateLink];
                if ((p_gtveh->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
                {
                    struct TrafficNode *p_tztnode;

                    p_tztnode = &game_traffic_nodes[-p_gtveh->U.UVehicle.TNode];
                    if ((p_tztnode->Flags & TNdF_Unkn0040) != 0)
                    {
                        if (p_tztnode->GateLink == p_vehicle->ThingOffset)
                        {
                            if (p_vehicle->U.UVehicle.Dummy4b == p_gtveh->U.UVehicle.TNode) {
                                remove_agphase2_lock(p_vehicle);
                            } else {
                                remove_agok_node_lock(p_vehicle, p_gtveh->U.UVehicle.TNode);
                            }
                            if (p_gtveh->U.UVehicle.Dummy4b == p_vehicle->U.UVehicle.TNode) {
                                remove_agphase2_lock(p_gtveh);
                            } else {
                                remove_agok_node_lock(p_gtveh, p_vehicle->U.UVehicle.TNode);
                            }
                        }
                    }
                }
            }
            else
            {
                remove_locks_apart_from_agok(p_vehicle);
                add_agphase1_lock(p_vehicle);
                p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
            }
        }
    }

    if (((p_tnode->Flags & TNdF_Unkn0008) != 0) && check_person_close(p_tnode))
    {
        if (p_vehicle->U.UVehicle.PissedOffWithWaiting < 100)
        {
            if (p_vehicle->U.UVehicle.PissedOffWithWaiting == 0) {
                p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
            }
            if (p_vehicle->U.UVehicle.ZebraOldHealth == p_vehicle->Health) {
                LOGDBG("Stopping %s thing %d rqspeed %d due to TNode %d near pedestrian",
                  vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                  (int)p_vehicle->U.UVehicle.ReqdSpeed, (int)p_vehicle->U.UVehicle.TNode);

                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
                p_vehicle->U.UVehicle.PissedOffWithWaiting++;
            } else {
                // Lost health while waiting - stop the wait immediatelly
                p_vehicle->U.UVehicle.PissedOffWithWaiting = 200;
            }
        }
    }
    else
    {
        p_vehicle->U.UVehicle.PissedOffWithWaiting = 0;
    }
}

void vehicle_workplace_states_finish(struct Thing *p_vehicle)
{
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
    {
        struct TrafficNode *p_dmtnode;
        int dx, dz;

        p_dmtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Dummy4b];
        dx = (p_vehicle->X >> 8) - p_dmtnode->X;
        dz = (p_vehicle->Z >> 8) - p_dmtnode->Z;
        if (dz * dz + dx * dx > 0x20000)
        {
            remove_agphase2_lock(p_vehicle);
        }
    }

    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
    {
        struct TrafficNode *p_agtnode;
        int dx, dz;

        p_agtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Agok];
        dx = (p_vehicle->X >> 8) - p_agtnode->X;
        dz = (p_vehicle->Z >> 8) - p_agtnode->Z;
        if (dz * dz + dx * dx > 0x20000)
        {
            remove_agok_lock(p_vehicle);
        }
    }
}

void VNAV_process_bezier(struct Thing *p_road)
{
    asm volatile ("call ASM_VNAV_process_bezier\n"
        : : "a" (p_road));
}

void VNAV_preprocess_bezier_turns(ulong nturns)
{
    ulong turns;

    for (turns = nturns; turns > 0; turns--)
    {
        ThingIdx thing;
        ushort i;

        thing = things_used_head;
        i = things_used;
        while ((thing > 0) && (i > 0))
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if ((p_thing->Type == TT_BUILDING)
              && (p_thing->SubType == SubTT_BLD_BEZIER_ROAD))
                VNAV_process_bezier(p_thing);
            thing = p_thing->LinkChild;
            i--;
        }
    }
}

/** Second list of traffic start nodes: [0] is the count, entries at 1..16.
 */
extern s32 data_1c84f4[17];

#ifdef SW_FNCHK
static void VNAV_init_new_traffic_system_asm(void)
{
    asm volatile ("call ASM_VNAV_init_new_traffic_system\n"
        :  :  : "eax", "cc", "memory");
}
#endif

/** Check if any traffic node links to node `node` (stored as -node).
 */
static TbBool traffic_node_has_link_from(ushort node)
{
    ushort n;
    int k;

    for (n = 1; n < next_traffic_node; n++)
    {
        for (k = 0; k < 4; k++)
        {
            if (game_traffic_nodes[n].UTraffic.Link[k] == -(int)node)
                return true;
        }
    }
    return false;
}

/** Prepare traffic nodes: clear runtime flags, mark dead ends and nodes
 * no other node leads to, and build the lists of start nodes.
 * Node Flags 0xFF means an unused node.
 */
static void VNAV_init_new_traffic_system_c(void)
{
    ushort n;
    int k;

    dword_1C8470 = 0;
    data_1c84f4[0] = 0;

    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        p_tnode->Flags &= 0xFE3F;
    }
    // Nodes without links out
    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        for (k = 0; k < 4; k++) {
            if (p_tnode->UTraffic.Link[k] != 0)
                break;
        }
        if (k == 4)
            p_tnode->Flags |= 0x0080;
    }
    // Nodes without links in - starting points
    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        if ((p_tnode->Flags & 0x0600) != 0)
            continue;
        if (traffic_node_has_link_from(n))
            continue;
        p_tnode->Flags |= 0x0100;
        if (dword_1C8470 != 32)
            dword_1C8474[dword_1C8470++] = n;
    }
    // Isolated nodes are dropped
    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        if (((p_tnode->Flags & 0x0080) != 0) && ((p_tnode->Flags & 0x0100) != 0))
            p_tnode->Flags = 0xFF;
    }
    // Second kind of starting points
    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        if ((p_tnode->Flags & 0x0200) == 0)
            continue;
        if ((p_tnode->Flags & 0x0400) != 0)
            continue;
        if (traffic_node_has_link_from(n))
            continue;
        p_tnode->Flags |= 0x0100;
        if (data_1c84f4[0] < 16) {
            data_1c84f4[0]++;
            data_1c84f4[data_1c84f4[0]] = n;
        }
    }
    for (n = 1; n < next_traffic_node; n++)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[n];
        if (p_tnode->Flags == 0xFF)
            continue;
        if ((p_tnode->Flags & 0x8000) == 0)
            continue;
        for (k = 0; k < 4; k++)
            p_tnode->UTraffic.Flags[k] |= 0x40;
    }
}

void VNAV_init_new_traffic_system(void)
{
    FNCHK_VOID("VNAV_init_new_traffic_system", VNAV_init_new_traffic_system_asm(),
      VNAV_init_new_traffic_system_c());
}

void traffic_unkn_func_01(void)
{
    asm volatile ("call ASM_traffic_unkn_func_01\n"
        :  :  : "eax" );
}

void process_next_tnode(struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_process_next_tnode\n"
        : : "a" (p_vehicle));
    return;
#endif
    short tnode;
    int tndt_x, tndt_z;
    int nxdist_sq; //< Squared distance to next traffic node or station

    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode == 0) {
        LOGERR("Crashing %s %d due to no TNode assigned",
          vehicle_type_name(p_vehicle->SubType), p_vehicle->ThingOffset);
        start_crashing(p_vehicle);
        return;
    }

    if ((p_vehicle->State == VehSt_UNKN_34 || p_vehicle->State == VehSt_UNKN_33)
      && (tnode == 0) && (p_vehicle->Speed == 0))
    {
        p_vehicle->State = VehSt_PARKED_PARAL;
        p_vehicle->U.UVehicle.AccelZ = 0;
        p_vehicle->U.UVehicle.AccelX = 0;
        p_vehicle->Flag |= TngF_Unkn0004;
        p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0100;
    }

    {
        int cux, cuz;

        if (tnode <= 0) // we've alteady made sure it's not 0
        {
            struct TrafficNode *p_tnode;

            p_tnode = &game_traffic_nodes[-tnode];
            if (p_vehicle->State == VehSt_UNKN_33) {
                cux = p_vehicle->U.UVehicle.Dummy5b;
                cuz = p_vehicle->U.UVehicle.Dummy5a;
            } else {
                cux = p_tnode->X;
                cuz = p_tnode->Z;
            }
        }
        else
        {
            struct Thing *p_station;

            p_station = &things[tnode];
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0) {
                cux = PRCCOORD_TO_MAPCOORD(p_station->X) - 5 * p_station->U.UObject.OffX;
                cuz = PRCCOORD_TO_MAPCOORD(p_station->Z) - 5 * p_station->U.UObject.OffZ;
            } else {
                cux = PRCCOORD_TO_MAPCOORD(p_station->X) + 5 * p_station->U.UObject.OffX;
                cuz = PRCCOORD_TO_MAPCOORD(p_station->Z) + 5 * p_station->U.UObject.OffZ;
            }
        }

        tndt_x = cux - PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        tndt_z = cuz - PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        nxdist_sq = tndt_z * tndt_z + tndt_x * tndt_x;
    }

    if (tnode > 0)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 1512;
    }
    else if (tnode < 0)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[-tnode];
        p_vehicle->U.UVehicle.ReqdSpeed = 1200 - 2 * abs(p_vehicle->U.UVehicle.AngleDY);
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
         && ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
         && (p_tnode->GateLink == p_vehicle->ThingOffset)) {
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
        }
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
         && ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) == 0)) {
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
        }
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
         && ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) == 0)) {
            remove_agphase1_lock(p_vehicle);
        }
    }

    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;
        struct Thing *p_gtveh;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && ((p_tnode->Flags & TNdF_Unkn8000) != 0) && (nxdist_sq < 0x90000))
        {
            p_gtveh = &things[p_tnode->GateLink];
            if ((p_gtveh->Flag & TngF_Unkn0040) == 0) {
                p_gtveh->Flag |= TngF_Unkn0020;
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_vehicle->U.UVehicle.AngleDY = 0;
                return;
            }
        }
    }

    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;
        struct Thing *p_gtveh;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && ((p_tnode->Flags & TNdF_Unkn8000) == 0) && (nxdist_sq < 0x10000))
        {
            p_gtveh = &things[p_tnode->GateLink];
            p_gtveh->Flag |= TngF_VehUnkn0100;
        }
    }

    if (tnode > 0)
    {
        struct Thing *p_station;
        short nxstatn;

        if (nxdist_sq < 0xC000)
        {
            p_station = &things[tnode];
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
                nxstatn = p_station->U.UObject.PrevThing;
            else
                nxstatn = p_station->U.UObject.NextThing;
            p_vehicle->U.UVehicle.TNode = nxstatn;
        }
    }

    TbBool do_state = false;
    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && (nxdist_sq < 0x10000))
            do_state = true;
        if ((p_tnode->GateLink == 0) && (nxdist_sq < 0x20000))
            do_state = true;
    }

    if (do_state)
    {
        struct TrafficNode *p_tnode;
        struct TrafficNode *p_nxtnode;

        p_tnode = &game_traffic_nodes[-tnode];
        switch (p_vehicle->State)
        {
        case VehSt_GOTO_LOC:
            update_workplace_unkn0001_flag(p_vehicle);
            if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
            {
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0) {
                    remove_agok_lock(p_vehicle);
                }
                promote_agphase2_lock(p_vehicle);
            }
            if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0) {
                promote_agphase1_lock(p_vehicle);
            }
            process_next_vnav_turn(p_vehicle);
            if (p_vehicle->U.UVehicle.TNode < 0)
            {
                p_nxtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                if ((p_nxtnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0) {
                    nxdist_sq = 0x7FFFFFFF;
                    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0008;
                }
            }
            break;
        case VehSt_UNKN_33:
            p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
            p_nxtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Dummy4b];
            if ((p_tnode->Flags & TNdF_Unkn0800) != 0)
            {
                int dx, dz;

                dx = p_tnode->X - p_nxtnode->X;
                dz = p_tnode->Z - p_nxtnode->Z;
                p_vehicle->U.UVehicle.AccelZ = 0;
                p_vehicle->U.UVehicle.AccelX = 0;
                p_vehicle->State = VehSt_UNKN_36;
                p_vehicle->U.UVehicle.Dummy5b = arctan(dx, dz);
                p_vehicle->U.UVehicle.Dummy5a = 0;
            }
            else
            {
                int simp_dist;
                int accX, accZ;
                int selX, selZ;
                int dx, dz;

                dx = p_tnode->X - p_nxtnode->X;
                dz = p_nxtnode->Z - p_tnode->Z;
                if (abs(dx) >= abs(dz)) {
                    simp_dist = abs(dx) + (abs(dz) >> 1);
                } else {
                    simp_dist = abs(dz) + (abs(dx) >> 1);
                }
                accX = 32 * dz;
                accZ = 32 * dx;
                if (simp_dist > 0) {
                    accX /= simp_dist;
                    accZ /= simp_dist;
                }
                selX = (p_vehicle->X >> 8) + 4 * accX + 4 * accZ + 8 * accX;
                selZ = (p_vehicle->Z >> 8) - 4 * accX + 4 * accZ + 8 * accZ;
                if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0100) != 0)
                  || !check_for_a_vehicle_here(selX, selZ, p_vehicle))
                {
                    dx = p_tnode->X - p_nxtnode->X;
                    dz = p_tnode->Z - p_nxtnode->Z;
                    p_vehicle->State = VehSt_UNKN_36;
                    p_vehicle->U.UVehicle.Dummy5a = 0;
                    p_vehicle->U.UVehicle.Dummy5b = arctan(dx, dz);
                    p_vehicle->U.UVehicle.AccelZ = accZ;
                    p_vehicle->U.UVehicle.AccelX = accX;
                    p_vehicle->U.UVehicle.Dummy5a |= 0x11;
                }
                else
                {
                    p_vehicle->U.UVehicle.Dummy5a -= 32 * accX;
                    p_vehicle->U.UVehicle.Dummy5b += 32 * accZ;
                    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0100;
                }
            }
            break;
        case VehSt_UNKN_34:
            tnode = p_vehicle->U.UVehicle.TNode;
            p_vehicle->U.UVehicle.Dummy4b = 0;
            p_vehicle->U.UVehicle.Dummy4a = tnode;
            p_vehicle->State = VehSt_UNKN_36;
            p_tnode = &game_traffic_nodes[-tnode];
            p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[0];
            break;
        default:
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0004;
            if ((p_tnode->Flags & TNdF_Unkn0080) == 0)
            {
                short lnk;

                update_workplace_unkn0001_flag(p_vehicle);
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
                {
                    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
                    {
                        remove_agok_lock(p_vehicle);
                    }
                    promote_agphase2_lock(p_vehicle);
                }

                p_vehicle->U.UVehicle.Dummy4a = 0;
                p_vehicle->U.UVehicle.Dummy4b = p_vehicle->U.UVehicle.TNode;
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
                {
                    promote_agphase1_lock(p_vehicle);
                }
                lnk = get_next_tnode(p_vehicle, p_tnode);
                p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[lnk];
                if (p_vehicle->U.UVehicle.TNode == 0) {
                    LOGERR("Crashing %s %d state %d.%d due to no next TNode in TNode %d link %d",
                       vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                       p_vehicle->State, p_vehicle->SubState, p_vehicle->U.UVehicle.Dummy4b, lnk);
                    start_crashing(p_vehicle);
                }

                if (p_vehicle->U.UVehicle.TNode < 0)
                {
                    struct TrafficNode *p_lntnode;

                    p_lntnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                    if (((p_tnode->UTraffic.Flags[lnk] & 0x80) != 0) ||
                      ((p_lntnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0))
                    {
                        nxdist_sq = 0x7FFFFFFF;
                        p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0008;
                    }
                }
                if ((p_tnode->UTraffic.Flags[lnk] & 0x01) != 0)
                    p_vehicle->Flag |= TngF_TriggerUse;
                else
                    p_vehicle->Flag &= ~TngF_TriggerUse;
            }
            else
            {
                int i, lnk;
                int dx, dz;

                dx = 0;
                dz = 1;

                for (i = 0; i < 8; i++)
                {
                    struct TrafficNode *p_lntnode;

                    p_vehicle->U.UVehicle.TNode = -dword_1C8474[LbRandomAnyShort() % dword_1C8470];
                    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];

                    lnk = tnode_find_used_link(p_tnode);
                    if (lnk < 0)
                        break;

                    p_lntnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[lnk]];
                    if ((p_lntnode->Flags & TNdF_Unkn0040) == 0)
                    {
                        dx = (p_lntnode->X << 8) - (p_tnode->X << 8);
                        dz = (p_lntnode->Z << 8) - (p_tnode->Z << 8);
                        p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                        move_mapwho(p_vehicle, p_tnode->X << 8, (p_tnode->Y + 15) << 8, p_tnode->Z << 8);
                        remove_locks(p_vehicle);
                        p_vehicle->U.UVehicle.Agok = p_tnode->UTraffic.Link[lnk];
                        add_agok_lock(p_vehicle);
                        break;
                    }
                }
                if (i >= 8) {
                    LOGERR("Crashing %s %d state %d.%d due to no random next TNode despite %d tries",
                      vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                      p_vehicle->State, p_vehicle->SubState, i);
                    start_crashing(p_vehicle);
                    return;
                }
                p_vehicle->U.UVehicle.AngleDY = 0;
                p_vehicle->U.UVehicle.AngleY = arctan(dx, dz);
            }
            break;
        }
    }

    update_vehicle_elevation(p_vehicle, tnode);

    update_vehicle_speed_and_direction(p_vehicle, tndt_x, tndt_z, nxdist_sq);

    if (nxdist_sq < 0xC0000)
    {
        process_vehicle_stop_for_pedestrians(p_vehicle);
    }

    vehicle_workplace_states_finish(p_vehicle);
}

/******************************************************************************/
