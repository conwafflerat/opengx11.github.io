package com.sonic1.game;

import com.sonic1.engine.Input;
import com.sonic1.engine.Renderer;

import static com.sonic1.game.Constants.*;

/**
 * Sonic, ported 1:1 from the s1disasm movement code.
 *
 * Per-frame order matches the disassembly:
 *   Ground: slope factor -> roll check -> accel/decel/friction -> derive x/y
 *           velocity from ground speed -> jump check -> move -> ground collision.
 *   Air:    variable jump cut -> air control -> air drag -> move -> gravity ->
 *           air collision.
 *
 * Collision is the simplified solid-tile version (see Level); the constants and
 * the movement maths are the real ones.
 */
public final class Player extends GameObject {

    public int groundVel;          // inertia along the ground (1/256 px)
    public int angle;              // ground angle, 0 = flat (256 = full turn)
    public boolean onGround;
    public boolean rolling;
    public boolean jumping;
    public boolean facingLeft;

    public int xRadius = WIDTH_RADIUS;
    public int yRadius = HEIGHT_RADIUS;

    public int animTimer;          // for placeholder sprite animation

    // 256-entry sine table scaled to 256, matching the engine's CalcSine output.
    private static final int[] SIN = new int[256];
    static {
        for (int i = 0; i < 256; i++) SIN[i] = (int) Math.round(256.0 * Math.sin(i * Math.PI * 2.0 / 256.0));
    }
    private static int sin(int a) { return SIN[a & 255]; }
    private static int cos(int a) { return SIN[(a + 64) & 255]; }

    public Player(int pixelX, int pixelY) {
        super(pixelX, pixelY);
        onGround = true;
    }

    public void update(Input in, Level lvl) {
        animTimer++;
        if (onGround) groundMode(in, lvl);
        else          airMode(in, lvl);
    }

    // ---- Ground -------------------------------------------------------------
    private void groundMode(Input in, Level lvl) {
        // Slope factor: inertia -= slope * sin(angle). On flat ground sin(0)=0.
        int slope = rolling ? (sameSign(groundVel, sin(angle)) ? SLOPE_ROLL_DN : SLOPE_ROLL_UP) : SLOPE_RUN;
        groundVel -= (slope * sin(angle)) >> 8;

        boolean left = in.down(Input.LEFT);
        boolean right = in.down(Input.RIGHT);

        // Start rolling: down held, moving fast enough, no horizontal input.
        if (!rolling && in.down(Input.DOWN) && Math.abs(groundVel) >= ROLL_MIN_SPEED && !(left ^ right)) {
            startRoll();
        }

        if (rolling) rollInput(left, right);
        else         runInput(left, right);

        if (rolling && Math.abs(groundVel) < UNROLL_SPEED) stopRoll();

        // Derive directional velocity from ground speed and angle.
        xVel = (groundVel * cos(angle)) >> 8;
        yVel = (groundVel * sin(angle)) >> 8;

        if (in.jumpPressed()) {
            jump();
            applyVelocity();
            airCollide(lvl);
            return;
        }

        applyVelocity();
        groundCollide(lvl);
    }

    private void runInput(boolean left, boolean right) {
        if (left && !right) {
            facingLeft = true;
            if (groundVel > 0) {                       // braking
                groundVel -= DECEL;
                if (groundVel < 0) groundVel = -0x80;  // quick turn-around kick
            } else if (groundVel > -TOP_SPEED) {
                groundVel -= ACCEL;
                if (groundVel < -TOP_SPEED) groundVel = -TOP_SPEED;
            }
        } else if (right && !left) {
            facingLeft = false;
            if (groundVel < 0) {                       // braking
                groundVel += DECEL;
                if (groundVel > 0) groundVel = 0x80;
            } else if (groundVel < TOP_SPEED) {
                groundVel += ACCEL;
                if (groundVel > TOP_SPEED) groundVel = TOP_SPEED;
            }
        } else {                                       // friction
            applyFriction(FRICTION);
        }
    }

    private void rollInput(boolean left, boolean right) {
        applyFriction(ROLL_FRICTION);
        if (left && !right && groundVel > 0) {
            groundVel -= ROLL_DECEL;
            if (groundVel < 0) groundVel = -0x80;
            facingLeft = true;
        } else if (right && !left && groundVel < 0) {
            groundVel += ROLL_DECEL;
            if (groundVel > 0) groundVel = 0x80;
            facingLeft = false;
        }
        if (groundVel > ROLL_TOP_SPEED) groundVel = ROLL_TOP_SPEED;
        if (groundVel < -ROLL_TOP_SPEED) groundVel = -ROLL_TOP_SPEED;
    }

    private void applyFriction(int f) {
        if (groundVel > 0) { groundVel -= f; if (groundVel < 0) groundVel = 0; }
        else if (groundVel < 0) { groundVel += f; if (groundVel > 0) groundVel = 0; }
    }

    private void jump() {
        xVel -= (JUMP_FORCE * sin(angle)) >> 8;   // flat: unchanged
        yVel -= (JUMP_FORCE * cos(angle)) >> 8;   // flat: yVel -= JUMP_FORCE
        onGround = false;
        jumping = true;
        rolling = true;                            // ball form in the air
        setRadii(WIDTH_RADIUS_ROLL, HEIGHT_RADIUS_ROLL);
    }

    // ---- Air ----------------------------------------------------------------
    private void airMode(Input in, Level lvl) {
        // Variable jump height: releasing jump while rising caps upward speed.
        if (jumping && yVel < -JUMP_CUT && !in.jumpHeld()) yVel = -JUMP_CUT;

        boolean left = in.down(Input.LEFT);
        boolean right = in.down(Input.RIGHT);
        if (left && !right) {
            xVel -= AIR_ACCEL;
            if (xVel < -TOP_SPEED) xVel = -TOP_SPEED;
            facingLeft = true;
        } else if (right && !left) {
            xVel += AIR_ACCEL;
            if (xVel > TOP_SPEED) xVel = TOP_SPEED;
            facingLeft = false;
        }

        // Air drag near the apex of the jump.
        if (yVel < 0 && yVel >= -AIR_DRAG_THRESHOLD) {
            xVel -= xVel >> 5;
        }

        applyVelocity();

        // Gravity is added after the move, per the disassembly.
        yVel += GRAVITY;
        if (yVel > MAX_FALL_SPEED) yVel = MAX_FALL_SPEED;

        airCollide(lvl);
    }

    // ---- Collision ----------------------------------------------------------
    private void groundCollide(Level lvl) {
        int px = resolveWalls(lvl, pixelX(), pixelY());
        px = clampX(px, lvl);
        setPixelX(px);

        int py = pixelY();
        int feet = py + yRadius;
        if (lvl.solidAt(px - xRadius, feet) || lvl.solidAt(px + xRadius, feet)) {
            int top = Math.floorDiv(feet, Level.TILE) * Level.TILE;
            setPixelY(top - yRadius);
            yVel = 0;
            onGround = true;
        } else {
            onGround = false;          // ran off a ledge; xVel already set
        }
    }

    private void airCollide(Level lvl) {
        int px = resolveWalls(lvl, pixelX(), pixelY());
        px = clampX(px, lvl);
        setPixelX(px);

        int py = pixelY();
        if (yVel >= 0) {               // falling: look for floor
            int feet = py + yRadius;
            if (lvl.solidAt(px - xRadius, feet) || lvl.solidAt(px + xRadius, feet)) {
                int top = Math.floorDiv(feet, Level.TILE) * Level.TILE;
                setPixelY(top - yRadius);
                land();
            }
        } else {                       // rising: look for ceiling
            int head = py - yRadius;
            if (lvl.solidAt(px - xRadius, head) || lvl.solidAt(px + xRadius, head)) {
                int bottom = Math.floorDiv(head, Level.TILE) * Level.TILE + Level.TILE;
                setPixelY(bottom + yRadius);
                yVel = 0;
            }
        }
    }

    private void land() {
        onGround = true;
        jumping = false;
        yVel = 0;
        angle = 0;
        if (rolling) { rolling = false; setRadii(WIDTH_RADIUS, HEIGHT_RADIUS); }
        groundVel = xVel;              // flat: inertia from horizontal speed
    }

    private int resolveWalls(Level lvl, int px, int py) {
        if (lvl.solidAt(px + xRadius, py)) {
            int tileLeft = Math.floorDiv(px + xRadius, Level.TILE) * Level.TILE;
            px = tileLeft - xRadius - 1;
            if (xVel > 0) xVel = 0;
            if (groundVel > 0) groundVel = 0;
        }
        if (lvl.solidAt(px - xRadius, py)) {
            int tileRight = (Math.floorDiv(px - xRadius, Level.TILE) + 1) * Level.TILE - 1;
            px = tileRight + xRadius + 1;
            if (xVel < 0) xVel = 0;
            if (groundVel < 0) groundVel = 0;
        }
        return px;
    }

    private int clampX(int px, Level lvl) {
        int min = xRadius;
        int max = lvl.pixelWidth() - xRadius - 1;
        if (px < min) { px = min; if (groundVel < 0) groundVel = 0; if (xVel < 0) xVel = 0; }
        if (px > max) { px = max; if (groundVel > 0) groundVel = 0; if (xVel > 0) xVel = 0; }
        return px;
    }

    // ---- Size changes keep the feet anchored --------------------------------
    private void setRadii(int newX, int newY) {
        y += (yRadius - newY) << 16;   // shift centre so feet stay put
        xRadius = newX;
        yRadius = newY;
    }

    private void startRoll() { rolling = true; setRadii(WIDTH_RADIUS_ROLL, HEIGHT_RADIUS_ROLL); }
    private void stopRoll()  { rolling = false; setRadii(WIDTH_RADIUS, HEIGHT_RADIUS); }

    private static boolean sameSign(int a, int b) { return (a ^ b) >= 0; }

    public boolean isBall() { return rolling || jumping; }

    // ---- Placeholder rendering (real art comes from ROM extraction) ---------
    public void render(Renderer r, int camX, int camY) {
        int sx = pixelX() - camX;
        int sy = pixelY() - camY;
        final int BLUE = 0x2030D0, BLUE_HI = 0x4A6BF5, PEACH = 0xF0C0A0, RED = 0xD81810, WHITE = 0xFFFFFF, BLACK = 0x000000;

        if (isBall()) {
            int rad = yRadius;
            r.fillCircle(sx, sy, rad, BLUE);
            r.fillCircle(sx - rad / 3, sy - rad / 3, rad / 3, BLUE_HI);
            // spin dashes
            int spin = (animTimer / 2) % 4;
            for (int i = 0; i < 4; i++) {
                double a = Math.PI / 2 * i + spin * 0.6;
                int dx = (int) (Math.cos(a) * rad * 0.6);
                int dy = (int) (Math.sin(a) * rad * 0.6);
                r.fillCircle(sx + dx, sy + dy, 1, BLUE_HI);
            }
            return;
        }

        int face = facingLeft ? -1 : 1;
        // spikes (behind, opposite to facing)
        r.fillCircle(sx - face * 7, sy - 4, 4, BLUE);
        r.fillCircle(sx - face * 8, sy + 2, 4, BLUE);
        // head / body
        r.fillCircle(sx, sy - 2, 11, BLUE);
        // face
        r.fillCircle(sx + face * 5, sy - 1, 7, PEACH);
        // eye
        r.fillRect(sx + face * 2, sy - 6, 5, 6, WHITE);
        r.fillRect(sx + face * 4, sy - 4, 2, 3, BLACK);
        // shoes
        int run = (animTimer / 3) % 2;
        r.fillCircle(sx - 3 + (run == 0 ? -1 : 1), sy + 15, 4, RED);
        r.fillCircle(sx + 3 + (run == 0 ? 1 : -1), sy + 15, 4, RED);
        r.fillRect(sx - 7, sy + 16, 14, 2, WHITE);
    }
}
