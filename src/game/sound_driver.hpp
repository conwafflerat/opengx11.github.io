#pragma once
#include <cstdint>
#include "../hardware/sound.hpp"

// ============================================================
// Native FM sound driver — replaces the Z80 driver from s2disasm.
//
// The original Z80 driver (SMPS) reads instrument/sequence data
// from Z80 RAM (uploaded by the 68K at level load time) and
// programs the YM2612 each frame.
//
// Here we replicate that driver's output in C++:
//   - SFX IDs match s2disasm's SFX_* equates exactly
//   - BGM IDs match s2disasm's BGM_* equates exactly
// ============================================================

// BGM track IDs (matching s2disasm's bgm_* equates)
enum class BGM : uint8_t {
    None         = 0x00,
    EmeraldHill  = 0x01,
    ChemPlant    = 0x02,
    AquaticRuin  = 0x03,
    CasinoNight  = 0x04,
    HillTop      = 0x05,
    MysticCave   = 0x06,
    OilOcean     = 0x07,
    Metropolis   = 0x08,
    SkyChase     = 0x09,
    WingFortress = 0x0A,
    DeathEgg     = 0x0B,
    TitleScreen  = 0x0C,
    SpecialStage = 0x0D,
    Credits      = 0x0E,
    BossTheme    = 0x0F,
    GameOver     = 0x10,
    Invincible   = 0x11,
    SpeedShoes   = 0x12,
};

// SFX IDs
enum class SFX : uint8_t {
    Ring        = 0x01,
    Jump        = 0x02,
    Hurt        = 0x03,
    MonitorBreak= 0x04,
    Checkpoint  = 0x05,
    LifeGained  = 0x06,
    Drown       = 0x07,
    SpinDash    = 0x08,
    SpinRelease = 0x09,
};

// Initialise the driver — call once after Sound::init()
void sound_driver_init(Sound &snd);

// Play a BGM track. Stops the current track and restarts from the beginning.
void sound_driver_play_bgm(BGM bgm);

// Play a sound effect (up to 6 SFX channels simultaneously, matching SMPS).
void sound_driver_play_sfx(SFX sfx);

// Stop everything.
void sound_driver_stop();

// Called once per frame (before Sound::submit_frame). Advances all active
// note timers and writes the next batch of YM2612/PSG register updates.
void sound_driver_tick(Sound &snd);
