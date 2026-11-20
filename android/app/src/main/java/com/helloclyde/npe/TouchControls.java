package com.helloclyde.npe;

/** Platform-independent touch geometry and press/release ownership. */
final class TouchControls {
    interface Sink { void button(int action, boolean down); }
    static final class Layout {
        final int unit, size, left, right, top, utilityTop, utilityUnit;
        Layout(int width, int height) {
            unit = Math.max(32, Math.min(52, Math.min((width - 32) / 6, (height - 80) / 3)));
            size = unit * 3; left = 12; right = width - size - 12;
            top = Math.max(60, height - size - 12); utilityTop = Math.max(8, top - 52);
            utilityUnit = Math.min(size / 2, Math.max(48, (width - 104) / 4));
        }
    }
    // Nintendo diamond: X top, Y left, A right, B bottom.
    static int column(int action) { return action == 4 ? 2 : action == 7 ? 0 : 1; }
    static int row(int action) { return action == 6 ? 0 : action == 5 ? 2 : 1; }
    static final class DirectionPad {
        private final Sink sink;
        int pointer = -1, direction = -1;
        DirectionPad(Sink sink) { this.sink = sink; }
        void down(int id, float x, float y, float width, float height) {
            if (pointer != -1) return;
            pointer = id; move(id, x, y, width, height);
        }
        void move(int id, float x, float y, float width, float height) {
            if (id != pointer) return;
            int next = -1;
            if (x >= 0 && y >= 0 && x < width && y < height) {
                float dx = x - width / 2, dy = y - height / 2;
                if (Math.abs(dx) > width / 6 || Math.abs(dy) > height / 6)
                    next = Math.abs(dx) > Math.abs(dy) ? (dx < 0 ? 3 : 1) : (dy < 0 ? 0 : 2);
            }
            if (next == direction) return;
            if (direction != -1) sink.button(direction, false);
            direction = next;
            if (direction != -1) sink.button(direction, true);
        }
        void up(int id) { if (pointer == id) cancel(); }
        void cancel() {
            if (direction != -1) sink.button(direction, false);
            pointer = direction = -1;
        }
        void repeat() { if (direction != -1) sink.button(direction, true); }
    }
}
