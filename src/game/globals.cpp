#include "globals.hpp"
#include "object.hpp"

uint8_t  f_gameMode    = 0;
uint8_t  f_gameType    = 0;
uint16_t f_frameCount  = 0;
uint8_t  f_lives       = 3;
uint8_t  f_rings       = 0;
uint32_t f_score       = 0;
uint16_t f_time_min    = 0;
uint16_t f_time_sec    = 0;
uint16_t f_time_frame  = 0;
uint8_t  f_invincible  = 0;
uint8_t  f_speedShoes  = 0;
uint8_t  f_water       = 0;
uint8_t  f_zoneAct     = 0;

int16_t  v_screenX        = 0;
int16_t  v_screenY        = 0;
int16_t  v_screenX2       = 0;
int16_t  v_planeAHScroll  = 0;
int16_t  v_planeBHScroll  = 0;
int16_t  v_planeAVScroll  = 0;
int16_t  v_planeBVScroll  = 0;

uint8_t  v_vdpRegs[24]    = {};

uint16_t v_levelWidth     = 0;
uint16_t v_levelHeight    = 0;
int16_t  v_levelBound_L   = 0;
int16_t  v_levelBound_R   = 0;
int16_t  v_levelBound_T   = 0;
int16_t  v_levelBound_B   = 0;

std::array<Object, OBJ_SLOTS> v_objects;

uint16_t v_ringCount = 0;
