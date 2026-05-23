package com.sonic1.game;

/**
 * Tile collision map. The real game uses 16x16 chunks with per-column height
 * arrays (so it can describe slopes and loops); this foundation uses solid /
 * empty 16x16 tiles, which is enough for flat ground, walls, platforms and pits.
 * Slope height arrays are the next data layer to port from the disassembly.
 */
public final class Level {
    public static final int TILE = 16;

    public final int cols;
    public final int rows;
    private final boolean[] solid;

    public int spawnX;
    public int spawnY;

    public Level(int cols, int rows) {
        this.cols = cols;
        this.rows = rows;
        this.solid = new boolean[cols * rows];
    }

    public void set(int c, int r, boolean v) {
        if (c < 0 || r < 0 || c >= cols || r >= rows) return;
        solid[r * cols + c] = v;
    }

    public boolean get(int c, int r) {
        if (c < 0 || r < 0 || c >= cols || r >= rows) return false;
        return solid[r * cols + c];
    }

    /** Solidity at a pixel. Side walls are solid; below the map is open (a pit). */
    public boolean solidAt(int px, int py) {
        if (py < 0) return false;            // open sky
        int c = Math.floorDiv(px, TILE);
        int r = Math.floorDiv(py, TILE);
        if (c < 0 || c >= cols) return true;  // level side bounds
        if (r >= rows) return false;          // fall out the bottom
        return solid[r * cols + c];
    }

    public int pixelWidth()  { return cols * TILE; }
    public int pixelHeight() { return rows * TILE; }

    public void fillRows(int rowStart, int rowEnd, int colStart, int colEnd) {
        for (int r = rowStart; r <= rowEnd; r++)
            for (int c = colStart; c <= colEnd; c++)
                set(c, r, true);
    }

    /**
     * A Green Hill-flavoured test stage: long ground with a step up, a floating
     * platform, a wall to brake against, and a pit to jump.
     */
    public static Level testZone() {
        int cols = 256, rows = 28;
        Level lv = new Level(cols, rows);

        // Main ground: bottom 4 rows.
        lv.fillRows(24, 27, 0, cols - 1);

        // Pit: remove the floor between columns 60 and 68.
        for (int r = 24; r <= 27; r++)
            for (int c = 60; c <= 68; c++)
                lv.set(c, r, false);

        // Step up after the pit.
        lv.fillRows(22, 23, 80, 110);

        // Higher ledge.
        lv.fillRows(19, 23, 111, 130);

        // A wall to run into / brake against.
        lv.fillRows(14, 23, 150, 152);

        // Floating platform to jump onto.
        lv.fillRows(18, 18, 165, 178);

        // Staircase back down.
        lv.fillRows(21, 23, 190, 200);
        lv.fillRows(22, 23, 201, 210);
        lv.fillRows(23, 23, 211, 220);

        lv.spawnX = 64;
        lv.spawnY = 24 * TILE - Constants.HEIGHT_RADIUS; // feet on the main ground
        return lv;
    }
}
