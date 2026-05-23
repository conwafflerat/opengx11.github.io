package com.sonic1.game;

/**
 * Player physics constants, transcribed 1:1 from the Sonic 1 disassembly
 * (sonicretro/s1disasm).
 *
 * Units follow the original engine:
 *   - Positions are 16.16 fixed-point: high 16 bits = pixel, low 16 bits = fraction.
 *   - Velocities are in 1/256 px per frame (a "subpixel"). To advance a position
 *     by a velocity the engine does: pos += vel << 8  (see GameObject.applyVelocity).
 *
 * Hex values are the literals found in the disassembly so they can be diffed
 * against the source directly.
 */
public final class Constants {
    private Constants() {}

    // --- Framebuffer / timing ------------------------------------------------
    public static final int SCREEN_W = 320;
    public static final int SCREEN_H = 224;
    public static final double FPS = 60.0;

    // --- Ground movement -----------------------------------------------------
    public static final int ACCEL      = 0x000C; // Sonic_acceleration
    public static final int DECEL      = 0x0080; // Sonic_deceleration (braking)
    public static final int FRICTION   = 0x000C; // friction == accel on ground
    public static final int TOP_SPEED  = 0x0600; // Sonic_max_speed (6 px/frame)

    // --- Air movement --------------------------------------------------------
    public static final int AIR_ACCEL  = 0x0018; // 2x ground accel
    public static final int GRAVITY    = 0x0038; // added to y_vel each air frame
    public static final int JUMP_FORCE = 0x0680; // initial jump velocity (Sonic 1)
    public static final int JUMP_CUT   = 0x0400; // variable jump release cap
    public static final int AIR_DRAG_THRESHOLD = 0x0400; // -$400 <= y_vel < 0

    // --- Rolling -------------------------------------------------------------
    public static final int ROLL_FRICTION = 0x0006; // half of ground friction
    public static final int ROLL_DECEL    = 0x0020;
    public static final int ROLL_MIN_SPEED = 0x0080; // min |speed| to start rolling
    public static final int UNROLL_SPEED   = 0x0028; // drop below -> stop rolling
    public static final int ROLL_TOP_SPEED = 0x1000; // rolling speed cap

    // --- Slopes (used once real collision angles exist) ----------------------
    public static final int SLOPE_RUN      = 0x0020;
    public static final int SLOPE_ROLL_UP  = 0x0050;
    public static final int SLOPE_ROLL_DN  = 0x0020;

    // --- Collision radii (pixels, measured from object centre) ---------------
    public static final int WIDTH_RADIUS         = 9;  // standing x radius
    public static final int HEIGHT_RADIUS        = 19; // standing y radius
    public static final int WIDTH_RADIUS_ROLL    = 7;  // rolling x radius
    public static final int HEIGHT_RADIUS_ROLL   = 14; // rolling y radius

    // --- Misc ----------------------------------------------------------------
    public static final int MAX_FALL_SPEED = 0x1000; // y_vel clamp while airborne
}
