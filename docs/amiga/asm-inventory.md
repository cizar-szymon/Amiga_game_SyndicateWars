# Inwentaryzacja asemblera x86 (etap A0)

Stan na upstream `5c4b0d56` (5.11.2026, „Remade new_bang()”), analiza z 11.11.2026.
Narzędzie: `python tools/amiga/asm_inventory.py [--md PLIK] [--csv PLIK] [--dead PLIK]`.
Pełna tabela żywych funkcji: [`asm-inventory-table.md`](asm-inventory-table.md).

## Metoda

Analiza statyczna, bez buildu:

1. Każdy linkowany plik `.sx` (lista z `src/Makefile.am`) jest dzielony na funkcje (`GLOBAL_FUNC`,
   etykiety najwyższego poziomu w `.text`; etykiety `jump_*` należą do bieżącej funkcji) i obiekty
   danych (`.data`: tablice skoków `vtable_*`, `data_*`).
2. Z każdego obiektu zbierane są odwołania do symboli.
3. Korzenie: symbole asm użyte w żywym kodzie C/C++ (`src`, `bflibrary`, `bfsoundlib`, `bfsmacklib`,
   `swrendersoft`, `bfariadne`) po usunięciu komentarzy i bloków `#if 0`. Tak upstream wyłącza starą
   ścieżkę po przepisaniu funkcji (`asm volatile ("call ASM_...")` w `#if 0`).
4. Osiągalne = domknięcie grafu odwołań od korzeni.

Wynik to **górne oszacowanie**: funkcja C z `asm volatile`, której sama gra już nie woła, nadal liczy
się jako korzeń. Dokładną listę da mapa linkera / analiza wywołań w zbudowanym `syndwarsfx.exe`
(po zbudowaniu wersji referencyjnej).

## Wynik

| plik | funkcje | instrukcje | żywe funkcje | żywe instrukcje | moduł |
|---|---:|---:|---:|---:|---|
| `src/swars.sx` | 881 | 102 118 | 432 | **56 535** | logika gry: ludzie, pojazdy, rzeczy, ścieżki, budynki, eksplozje |
| `bfsmacklib/src/bfsmacklib_s.sx` | 315 | 7 953 | 296 | 6 693 | odtwarzacz Smacker (filmy) |
| `src/radica1/2/3.sx` | 59 | 6 565 | 49 | 6 298 | sieć (protokół gry sieciowej) |
| `src/netdos/netipx1/netser1.sx` | 88 | 3 775 | 39 | 2 374 | sieć DOS: IPX, port szeregowy |
| `src/bat_s.sx` | 35 | 2 779 | 8 | 1 148 | minigra „Breakout” (BAT) |
| `bfsoundlib/src/ailssa.sx` | 204 | 3 611 | 62 | 1 115 | Miles Sound System (emulacja) |
| `src/sound_smp_s.sx`, `sound_heap_s.sx` | 21 | 1 417 | 16 | 1 383 | dźwięk: próbki, sterta |
| `src/thing_expld_s.sx`, `thing_fire_s.sx` | 44 | 6 359 | 13 | 571 | eksplozje, ogień |
| `src/bflib_vidraw_s.sx` | 8 | 1 597 | 0 | 0 | (martwy) |
| **razem** | **1 655** | **136 174** | **915** | **~76 100** | |

- **740 funkcji (~60 tys. instrukcji) jest już martwych**: przepisane w C, ale asm nadal leży w plikach.
  Przy przenośnym buildzie po prostu ich nie linkujemy.
- Renderer 3D (`swrendersoft/`) i biblioteka `bflibrary` (poza DOS-owym joystickiem, który nie jest
  linkowany) są już w C.
- W `swars.sx` 165 żywych funkcji (~21 tys. instrukcji) jest wołanych tylko z asm (wewnętrzne
  podprocedury). Reszta ma bezpośredni korzeń w C. Najwięcej: `people.c` (68 funkcji, 8,5 tys. instr.),
  `vehicle.c` (35 / 5,5 tys.), `thing.c` (46 / 5,5 tys.), `game.c`, `pathtrig.c`, `building.c`.
- Tempo upstream: w 2026 ok. 170 commitów „Remade/Routed”, od sierpnia ok. 30 funkcji na miesiąc.
  Przy tym tempie sam `swars.sx` zniknie z gry za ok. 1–1,5 roku.

## Proponowana kolejność (do akceptacji)

Kod poza logiką gry na Amidze i tak zastępujemy, więc nie trzeba go wiernie przepisywać:

1. **Build przenośny bez modułów DOS** (flaga konfiguracji, np. `--disable-x86-asm-modules`):
   - sieć (`radica*`, `netdos`, `netipx1`, `netser1`, ~8,7 tys. instr.) → zaślepki w C
     (gra jednoosobowa; sieć ewentualnie później przez TCP, jak w Settlers / OpenDUNE),
   - Smacker (`bfsmacklib_s.sx`, ~6,7 tys.) → dekoder Smackera w C (np. libsmacker, LGPL;
     do sprawdzenia licencji) albo na początek pominięcie filmów,
   - Miles / dźwięk (`ailssa.sx`, `sound_*_s.sx`, ~2,5 tys.) → warstwa dźwięku w C (na Amidze Paula),
   - minigra BAT (~1,1 tys.) → przepisanie (mały, zamknięty moduł) albo wyłączenie.
2. **Logika gry `swars.sx` (~56,5 tys. instr.)** w kolejności modułów, równolegle z upstream
   i w uzgodnieniu z nim (Discord), żeby nie robić tej samej funkcji dwa razy:
   najpierw liście grafu (funkcje bez wywołań asm), potem moduły od najmniejszych zależności:
   `thing_expld`/`thing_fire` → `bmbang` → `weapon` → `building` → `pathtrig` → `vehicle` → `people`
   → `thing` → `game`.
3. Każda przepisana funkcja: build x86, ten sam przebieg gry z porównaniem stanu z wersją asm
   (do ustalenia: nagrywanie wejść + porównanie stanu co klatkę, jak lockstep w Vastar / Xain'd Sleena).

## Otwarte pytania

- Czy upstream ma narzędzie do porównywania stanu (asm vs C)? `swars-tests` sprawdzić po zbudowaniu.
- Dokładna lista żywych funkcji z mapy linkera (po zbudowaniu x86).

## Pokrycie asm w misjach kampanii 0 (13.11.2026)

Wersja PC z pomiarem pokrycia (`build_x86.sh cov`, zapis co 500 klatek), każda misja z `conf/miss000.ini`
uruchomiona na 45 s bez sterowania (`-q -W -m 0,N`). Kolumny: w ilu misjach, funkcja, przykładowe misje.
Pominięte: dźwięk, sieć, BAT, prymitywy Lb. Funkcje bez `ASM_` to etykiety wewnątrz asm.

```
 44 func_4225c                                    101 104 105 107 109 20 21 22
 44 ASM_VNAV_unkn_func_207                        101 104 105 107 109 20 21 22
 42 rotate_obj                                    101 104 105 107 109 20 21 22
 41 ASM_set_passengers_location                   101 104 105 109 20 21 22 23
 39 swoosh_veh                                    104 105 109 20 21 22 23 24
 36 ASM_process_parked_flyer                      101 104 105 107 109 20 21 24
 35 ASM_set_vehicle_alt                           104 105 107 109 20 21 22 23
 35 VNAV_find_nearnodes                           104 105 107 109 20 21 22 23
 35 VNAV_qwertyuiop                               104 105 107 109 20 21 22 23
 35 VNAV_find_start_dropoff_point                 104 105 107 109 20 21 22 23
 34 ASM_start_goto_vehicle                        104 105 109 20 21 22 23 24
 31 ASM_object_vec_normalisation                  101 104 105 109 21 22 23 25
 31 ASM_vec_cross_prod                            101 104 105 109 21 22 23 25
 30 ASM_rotate_object_axis                        104 105 109 21 22 23 25 26
 29 ASM_move_vehicle                              104 105 109 21 22 23 25 26
 26 good_node                                     104 105 109 21 22 23 25 26
 26 ASM_get_next_tnode                            104 105 109 21 22 23 25 26
 24 ASM_person_pickup                             104 105 20 21 22 29 3 35
 24 ASM_person_init_pickup                        104 105 20 21 22 29 3 35
 24 do_i_have_weapon                              104 105 20 21 22 29 3 35
 24 ASM_person_get_item                           104 105 20 21 22 29 3 35
 24 ASM_move_flying_vehicle                       109 20 22 24 25 26 28 29
 22 do_next_navigate_index                        109 20 24 25 26 28 35 36
 20 func_5fe50                                    109 20 24 25 26 28 35 36
 20 flying_alt                                    109 20 24 25 26 28 35 36
 20 ASM_process_my_takeoff                        109 20 24 25 26 28 35 36
 20 ASM_process_my_flying_vehicle                 109 20 24 25 26 28 35 36
 20 ariadne_initialise_route                      109 20 24 25 26 28 35 36
 19 ASM_apply_full_light                          101 109 20 21 22 29 37 38
 19 ASM_person_kill_person                        101 104 105 109 21 22 23 29
 19 person_poke                                   104 105 109 21 22 23 25 26
 19 ASM_person_hit_by_car                         104 105 109 21 22 23 25 26
 19 ASM_find_nearest_object2                      20 21 3 35 36 5 50 57
 18 get_some_phwoar                               101 109 21 22 29 37 38 49
 18 give_some_phwoar                              101 109 21 22 29 37 38 49
 18 ASM_fillin_phwoar_type_info                   101 109 21 22 29 37 38 49
 18 ASM_process_bang                              101 109 21 22 29 37 38 49
 18 func_ca6a3                                    101 109 21 22 29 37 38 49
 18 triangle_route_do_given                       109 20 24 26 28 35 36 38
 18 triangle_route_do_bak                         109 20 24 26 28 35 36 38
 18 ma_triangle_route                             109 20 24 26 28 35 36 38
 18 path_init8_wide                               109 20 24 26 28 35 36 38
 17 ASM_remove_thing                              101 109 23 26 29 38 41 46
 17 ASM_sea_texture                               101 109 21 22 29 37 38 49
 17 give_some_shrapnel                            101 109 21 22 29 37 38 49
 17 func_ca37b                                    101 109 21 22 29 37 38 49
 17 func_ca643                                    101 109 21 22 29 37 38 49
 17 func_ca88e                                    101 109 21 22 29 37 38 49
 17 ASM_person_run_away                           104 105 109 21 22 23 37 38
 17 ASM_process_next_vnav_turn                    104 105 21 22 23 25 26 3
 17 VNAV_find_dropoff_point                       104 105 21 22 23 25 26 3
 17 VNAV_there                                    104 105 21 22 23 25 26 3
 17 VNAV_can_i_get_there                          104 105 21 22 23 25 26 3
 17 VNAV_goto                                     104 105 21 22 23 25 26 3
 16 kill_flags                                    105 109 23 29 35 36 49 5
 16 ASM_aquire_target                             105 109 23 29 35 36 49 5
 15 search_for_danger                             104 105 109 21 22 37 38 41
 15 check_for_a_moving_vehicle_here               104 105 21 22 23 3 34 35
 15 ASM_process_unstopping                        104 105 21 22 23 3 34 35
 15 veh_heap_add                                  104 105 21 22 23 25 26 34
 15 VNAV_do                                       104 105 21 22 23 25 26 34
 15 VNAV_turn                                     104 105 21 22 23 25 26 34
 15 VNAV_goto_and_unpark                          104 105 21 22 23 3 34 35
 14 ASM_laser_hit_at                              101 109 23 38 41 46 5 50
 13 ASM_quick_light_unkn_func_04                  101 109 20 21 22 38 49 5
 13 quick_light_unkn_func_05                      101 109 20 21 22 38 49 5
 13 quick_light_unkn_func_07                      101 109 20 21 22 38 49 5
 13 quick_light_unkn_func_08                      101 109 20 21 22 38 49 5
 13 quick_light_unkn_func_09                      101 109 20 21 22 38 49 5
 13 move_smapwho                                  101 109 20 21 38 49 5 55
 13 func_ca152                                    101 109 21 22 29 37 38 49
 13 func_ca166                                    101 109 21 22 29 37 38 49
 13 ASM_process_stopping                          105 21 22 23 25 26 3 35
 13 ASM_process_hovering_vehicle                  109 20 25 26 36 38 46 63
 12 quick_light_unkn_func_06                      101 109 20 21 22 38 49 5
 12 ASM_assign_next_target_from_group             101 109 23 29 37 38 41 50
 12 ASM_process_laser                             101 109 38 41 46 5 50 8
 12 ASM_set_static_on_fire                        109 21 49 5 55 56 57 74
 12 ASM_process_mine                              109 21 26 38 5 58 59 7
 11 ASM_process_drift_smoke                       101 109 21 38 49 5 55 56
 11 ASM_FIRE_process_flame                        101 109 21 22 29 37 49 56
 11 func_dacdf                                    101 109 21 22 29 37 49 56
 11 ASM_search_object_for_qface                   104 105 107 23 24 26 36 76
 11 apply_unkn_vehicle50_damage                   109 21 49 5 55 56 74 8
 11 func_ca299                                    109 21 49 5 55 56 74 8
 10 ASM_process_laser_elec                        101 109 23 41 46 50 83 88
 10 ASM_process_razor_wire                        101 109 23 41 46 50 83 88
 10 func_ca500                                    101 109 22 29 37 38 5 57
 10 VNAV_renavigate                               104 105 21 22 23 25 26 34
 10 check_tank_collision                          107 109 20 24 41 63 74 83
 10 ASM_process_tank_stationary                   107 109 20 24 41 63 74 83
 10 ASM_give_take_me_weapon                       21 26 36 38 5 7 76 8
  9 do_fvect_colide                               101 109 23 5 50 8 83 88
  9 del_thing_vectors_from_mapwho                 101 109 21 37 38 49 55 56
  9 do_people_on_exploding_building               101 109 21 37 38 49 55 56
  9 ASM_explode_thing_building                    101 109 21 37 38 49 55 56
  9 ASM_triangulation_unkn_func_002               101 109 21 37 38 49 55 56
  9 func_ca12a                                    101 109 21 37 38 49 55 56
  9 do_explode                                    109 21 38 49 5 55 56 57
  9 ASM_process_nuclear_bomb                      109 21 38 49 5 55 56 57
  9 func_ca31b                                    109 21 38 49 5 55 56 57
  9 func_ca35b                                    109 21 38 49 5 55 56 57
  8 func_5fcf4                                    101 109 21 37 49 55 56 57
  8 explode_thing_object                          101 109 22 29 37 38 5 57
  8 ASM_process_my_crashing_vehicle               101 109 22 29 37 38 5 57
  8 ASM_start_crashing                            101 109 22 29 37 38 5 57
  8 ASM_init_vehicle_explode                      101 109 22 29 37 38 5 57
  8 ASM_check_for_a_vehicle_here                  105 21 26 3 37 4 50 83
  8 ASM_process_shuttle_loader                    20 21 34 37 56 63 7 9
  7 func_ca100                                    101 109 22 29 37 38 5
  7 func_ca33b                                    101 109 22 29 37 38 5
  7 func_dad7c                                    101 109 22 29 37 96 97
  7 func_db265                                    101 109 21 37 49 56 57
  7 func_db444                                    101 109 21 37 49 56 57
  7 func_db60d                                    101 109 21 37 49 56 57
  7 func_db7e3                                    101 109 21 37 49 56 57
  7 ASM_search_things_for_uniqueid                104 105 25 38 83 9 96
  7 ASM_track_angle                               104 105 35 36 38 74 76
  7 ASM_init_c_iff                                109 5 58 59 7 83 88
  7 ASM_napalm_person                             109 21 49 5 55 56 96
  7 ASM_process_c_iff                             109 5 58 59 7 83 88
  7 ASM_process_unplacing                         21 22 23 25 26 34 35
  7 VNAV_goto_and_unplace                         21 22 23 25 26 34 35
  7 ASM_map_things_unkn_func_04                   29 35 36 49 76 96 97
  6 ASM_person_hit_razor_wire                     101 109 20 23 58 59
  6 ASM_person_self_destruct                      101 38 49 55 56 57
  6 available_for_guard_duty                      101 21 22 23 38 96
  6 find_best_guardian                            101 21 22 23 38 96
  6 ASM_find_and_alert_guardian                   101 21 22 23 38 96
  6 ASM_create_electric_strand                    101 109 23 83 88 96
  6 ASM_process_electric_strand                   101 109 23 83 88 96
  6 ASM_process_my_land_vehicle                   20 36 38 74 76 8
  6 ASM_start_landing                             20 36 38 74 76 8
  5 ASM_person_save_victim                        101 21 22 38 96
  5 exit_pod                                      101 26 29 50 9
  5 ASM_process_shuttle_pod                       101 26 29 50 9
  5 set_object_flag                               101 26 29 50 9
  5 ASM_create_a_pod                              101 26 29 50 9
  5 ASM_create_another_strand                     101 23 83 88 96
  5 ASM_elec_hit_building                         101 23 83 88 96
  5 ASM_process_mushroom                          109 49 55 56 57
  5 ASM_test_stuff                                109 49 55 56 57
  4 ASM_process_rocket                            101 109 96 97
  4 func_dad8c                                    101 109 96 97
  4 make_triangle_solid                           109 21 56 57
  4 sit_on_my_face                                21 38 56 57
  4 func_4ee00                                    36 7 76 8
  4 ASM_bang_new4                                 49 55 56 57
  3 ASM_on_mapwho                                 104 105 84
  3 calc_person_speed                             21 26 7
  3 ASM_person_drop_item                          21 26 7
  3 func_dc131                                    22 29 37
  3 func_dc2d7                                    22 29 37
  3 func_dc498                                    22 29 37
  3 create_unkn_building_subtp27                  23 26 83
  3 ASM_create_train_carriage                     23 26 83
  3 thing_unkn_change_speed                       23 26 83
  3 ASM_train_unkn_st18_func_2                    23 26 83
  3 ASM_train_unkn_st18_func_1                    23 26 83
  3 train_unkn_func_02                            23 26 83
  3 ASM_train_unkn_st20_func_1                    23 26 83
  3 ASM_train_unkn_st21_exit_func_1               23 26 83
  3 ASM_VNAV_process_bezier                       29 35 83
  3 ASM_all_group_arrived                         41 42 46
  2 yank_from_vehicle                             101 38
  2 ASM_process_mech_stationary                   101 109
  2 ASM_process_mech_unknown1                     101 109
  2 init_unkn1thing                               101 109
  2 ASM_mech_unkn_func_09                         101 109
  2 mech_unkn_func_08                             101 109
  2 matrix_transpose_cp                           101 109
  2 mech_unkn_func_19                             101 109
  2 mech_unkn_func_20                             101 109
  2 ASM_find_peep_in_area                         104 105
  2 can_the_tank_run_over                         109 74
  2 ASM_process_flame1                            109 57
  2 ASM_init_flamer                               20 63
  2 ASM_process_napalm_flame                      20 63
  2 ASM_set_dist_volume                           83 88
  2 ASM_process_tank                              83 92
  2 ASM_build_tank_path                           83 92
  2 ASM_create_stasis_pod                         83 88
  2 ASM_find_and_stasis                           83 88
  2 ASM_process_stasis_pod                        83 88
  2 func_cb31e                                    83 88
  2 func_cb3ac                                    83 88
  2 func_cb3d4                                    83 88
  2 func_cb465                                    83 88
  1 ASM_is_group_all_persuaded_by_me              109
  1 flatten_vehicle                               109
  1 ASM_mounted_los                               109
  1 ASM_process_splash                            109
  1 func_db9b9                                    109
  1 func_dbb7c                                    109
  1 ASM_person_init_drop_special                  21
  1 search_floor_for_item                         21
  1 find_next_ship_tnode                          22
  1 ASM_process_ship                              22
  1 ASM_try_and_kill_target                       46
  1 tank_los                                      83
  1 ASM_process_time_pod                          88
  1 ASM_all_group_arrived_square                  92
```
