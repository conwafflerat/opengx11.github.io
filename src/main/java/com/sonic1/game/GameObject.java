package com.sonic1.game;

/**
 * Base for all moving objects, mirroring the original Sprite Status Table:
 *   x/y      -> x_pos/y_pos  (16.16 fixed-point, high word = pixel)
 *   xVel/yVel-> x_vel/y_vel  (1/256 px per frame)
 *
 * {@link #applyVelocity()} reproduces the engine's ObjMove: position is advanced
 * by velocity shifted left 8 bits, so a velocity of 0x0600 (= 6.0 in 1/256 units)
 * adds exactly 6 pixels per frame.
 */
public abstract class GameObject {
    public int x;     // 16.16
    public int y;     // 16.16
    public int xVel;  // 1/256 px per frame
    public int yVel;  // 1/256 px per frame

    protected GameObject(int pixelX, int pixelY) {
        this.x = pixelX << 16;
        this.y = pixelY << 16;
    }

    public void applyVelocity() {
        x += xVel << 8;
        y += yVel << 8;
    }

    public int pixelX() { return x >> 16; }
    public int pixelY() { return y >> 16; }

    public void setPixelX(int px) { x = px << 16; }
    public void setPixelY(int py) { y = py << 16; }
}
