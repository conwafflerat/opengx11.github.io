package com.sonic1.engine;

/**
 * Genesis controller state with per-frame edge detection.
 *
 * The window thread sets the raw held flags via {@link #set}; the game thread
 * calls {@link #poll} once per frame to compute "pressed this frame" edges,
 * mirroring how the original engine reads (V_int) controller state once a frame.
 */
public final class Input {
    public static final int UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, A = 4, B = 5, C = 6, START = 7;
    private static final int COUNT = 8;

    private final boolean[] held = new boolean[COUNT];
    private final boolean[] heldSnapshot = new boolean[COUNT];
    private final boolean[] prev = new boolean[COUNT];

    public synchronized void set(int button, boolean down) {
        if (button >= 0 && button < COUNT) held[button] = down;
    }

    /** Snapshot the raw held state for this frame and compute edges. */
    public void poll() {
        System.arraycopy(heldSnapshot, 0, prev, 0, COUNT);
        synchronized (this) {
            System.arraycopy(held, 0, heldSnapshot, 0, COUNT);
        }
    }

    public boolean down(int button)    { return heldSnapshot[button]; }
    public boolean pressed(int button) { return heldSnapshot[button] && !prev[button]; }

    /** Any jump button (A/B/C) held. */
    public boolean jumpHeld()    { return down(A) || down(B) || down(C); }
    /** Any jump button pressed this frame. */
    public boolean jumpPressed() { return pressed(A) || pressed(B) || pressed(C); }

    /** For scripted self-test input: force a held state without the AWT thread. */
    public void scriptHold(int button, boolean down) { set(button, down); }
}
