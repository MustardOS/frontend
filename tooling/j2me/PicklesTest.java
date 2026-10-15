import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Graphics;
import javax.microedition.media.Manager;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

public class PicklesTest extends MIDlet {
    private TestCanvas canvas;

    protected void startApp() {
        if (canvas == null) {
            canvas = new TestCanvas(this, loadLaunchCount());
            new Thread(canvas).start();
        }
        Display.getDisplay(this).setCurrent(canvas);
    }

    protected void pauseApp() {
    }

    protected void destroyApp(boolean unconditional) {
        if (canvas != null) canvas.running = false;
    }

    private int loadLaunchCount() {
        int count = 0;
        try {
            RecordStore store = RecordStore.openRecordStore("picklestest", true);
            if (store.getNumRecords() == 0) {
                count = 1;
                byte[] data = Integer.toString(count).getBytes();
                store.addRecord(data, 0, data.length);
            } else {
                byte[] data = store.getRecord(1);
                count = Integer.parseInt(new String(data)) + 1;
                byte[] next = Integer.toString(count).getBytes();
                store.setRecord(1, next, 0, next.length);
            }
            store.closeRecordStore();
            System.out.println("PICKLESTEST rms launch count " + count);
        } catch (Exception e) {
            System.out.println("PICKLESTEST rms failed " + e);
            count = -1;
        }
        return count;
    }

    static class TestCanvas extends Canvas implements Runnable {
        volatile boolean running = true;
        private final PicklesTest midlet;
        private final int launches;
        private long started = System.currentTimeMillis();
        private int frames = 0;
        private int fps = 0;
        private long fpsMark = System.currentTimeMillis();
        private int fpsFrames = 0;
        private int lastKey = 0;
        private String lastAction = "none";
        private int pressCount = 0;
        private int heldKeys = 0;
        private int pointerX = -1;
        private int pointerY = -1;
        private boolean pointerDown = false;
        private String sound = "not played";

        TestCanvas(PicklesTest midlet, int launches) {
            this.midlet = midlet;
            this.launches = launches;
            setFullScreenMode(true);
        }

        public void run() {
            while (running) {
                long frameStart = System.currentTimeMillis();
                repaint();
                serviceRepaints();
                frames++;
                fpsFrames++;
                long now = System.currentTimeMillis();
                if (now - fpsMark >= 1000) {
                    fps = fpsFrames;
                    fpsFrames = 0;
                    fpsMark = now;
                    System.out.println("PICKLESTEST fps " + fps + " frames " + frames + " key " + lastKey + " " + lastAction);
                }
                long spare = 16 - (System.currentTimeMillis() - frameStart);
                if (spare > 0) {
                    try {
                        Thread.sleep(spare);
                    } catch (InterruptedException e) {
                        return;
                    }
                }
            }
        }

        protected void paint(Graphics g) {
            int w = getWidth();
            int h = getHeight();
            g.setColor(0x101820);
            g.fillRect(0, 0, w, h);

            int[] bars = {0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x00ffff, 0xff00ff, 0xffffff};
            int barWidth = w / bars.length;
            for (int i = 0; i < bars.length; i++) {
                g.setColor(bars[i]);
                g.fillRect(i * barWidth, 0, barWidth, 12);
            }

            int travel = w - 20;
            int x = travel > 0 ? (frames * 2) % (travel * 2) : 0;
            if (x > travel) x = travel * 2 - x;
            g.setColor(0xffa000);
            g.fillRect(x, h - 24, 20, 20);

            g.setColor(0xffffff);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL));
            int line = Font.getDefaultFont().getHeight() + 1;
            int y = 16;
            g.drawString("Pickles J2ME test " + w + "x" + h, 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("fps " + fps + "  frames " + frames, 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("uptime " + (System.currentTimeMillis() - started) / 1000 + "s  launches " + launches, 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("key " + lastKey + " " + lastAction + " #" + pressCount, 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("held " + heldKeys, 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("pointer " + pointerX + "," + pointerY + (pointerDown ? " down" : " up"), 2, y, Graphics.TOP | Graphics.LEFT);
            y += line;
            g.drawString("sound " + sound, 2, y, Graphics.TOP | Graphics.LEFT);

            if (pointerX >= 0) {
                g.setColor(pointerDown ? 0xff4040 : 0x40ff40);
                g.drawLine(pointerX - 4, pointerY, pointerX + 4, pointerY);
                g.drawLine(pointerX, pointerY - 4, pointerX, pointerY + 4);
            }
        }

        private String actionName(int keyCode) {
            int action;
            try {
                action = getGameAction(keyCode);
            } catch (Exception e) {
                return "invalid";
            }
            switch (action) {
                case UP: return "UP";
                case DOWN: return "DOWN";
                case LEFT: return "LEFT";
                case RIGHT: return "RIGHT";
                case FIRE: return "FIRE";
                case GAME_A: return "GAME_A";
                case GAME_B: return "GAME_B";
                case GAME_C: return "GAME_C";
                case GAME_D: return "GAME_D";
                default: return keyCode >= KEY_NUM0 && keyCode <= KEY_NUM9 ? "NUM" + (keyCode - KEY_NUM0)
                    : keyCode == KEY_STAR ? "STAR" : keyCode == KEY_POUND ? "POUND" : "other";
            }
        }

        public void keyPressed(int keyCode) {
            lastKey = keyCode;
            lastAction = actionName(keyCode);
            pressCount++;
            heldKeys++;
            System.out.println("PICKLESTEST press " + keyCode + " " + lastAction);
            if ("FIRE".equals(lastAction)) {
                try {
                    Manager.playTone(60 + pressCount % 24, 150, 80);
                    sound = "tone " + (60 + pressCount % 24);
                } catch (Exception e) {
                    sound = "failed " + e;
                }
                System.out.println("PICKLESTEST " + sound);
            }
        }

        public void keyReleased(int keyCode) {
            if (heldKeys > 0) heldKeys--;
            System.out.println("PICKLESTEST release " + keyCode);
        }

        public void pointerPressed(int x, int y) {
            pointerX = x;
            pointerY = y;
            pointerDown = true;
            System.out.println("PICKLESTEST pointer down " + x + "," + y);
        }

        public void pointerDragged(int x, int y) {
            pointerX = x;
            pointerY = y;
        }

        public void pointerReleased(int x, int y) {
            pointerX = x;
            pointerY = y;
            pointerDown = false;
            System.out.println("PICKLESTEST pointer up " + x + "," + y);
        }
    }
}
