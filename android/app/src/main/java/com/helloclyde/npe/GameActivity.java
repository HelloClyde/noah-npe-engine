package com.helloclyde.npe;

import android.app.AlertDialog;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import org.libsdl.app.SDLActivity;
import java.util.ArrayList;

public class GameActivity extends SDLActivity {
    private final Handler repeats = new Handler(Looper.getMainLooper());
    private final ArrayList<Button> buttons = new ArrayList<>();
    private static native void nativeButton(int action, boolean down);
    @Override protected String[] getLibraries() { return new String[]{"SDL2", "main"}; }
    @Override protected String[] getArguments() { return new String[]{getIntent().getStringExtra("game")}; }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null || mSurface == null) return; // SDL shows its own load error.
        int height = (int)(112 * getResources().getDisplayMetrics().density);
        RelativeLayout.LayoutParams surface = new RelativeLayout.LayoutParams(-1, -1);
        surface.bottomMargin = height; mSurface.setLayoutParams(surface);
        LinearLayout dock = new LinearLayout(this); dock.setOrientation(LinearLayout.VERTICAL);
        dock.setBackgroundColor(0xff202020);
        RelativeLayout.LayoutParams position = new RelativeLayout.LayoutParams(-1, height);
        position.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM); mLayout.addView(dock, position);
        LinearLayout row1 = row(dock), row2 = row(dock);
        button(row1,"↑",0,true); button(row1,"A 确认",4,false);
        button(row1,"X 对话",6,false); button(row1,"P 上页",8,false);
        button(row1,"N 下页",9,false);
        Button leave = new Button(this); leave.setText("换游戏"); leave.setFocusable(false);
        row1.addView(leave,new LinearLayout.LayoutParams(0,-1,1));
        leave.setOnClickListener(v -> new AlertDialog.Builder(this).setTitle("返回游戏列表？")
            .setMessage("请先用游戏菜单存档，未保存的进度不会自动保存。")
            .setPositiveButton("返回列表", (d,w) -> finish()).setNegativeButton("继续游戏",null).show());
        button(row2,"←",3,true); button(row2,"↓",2,true); button(row2,"→",1,true);
        button(row2,"B 返回",5,false); button(row2,"Y 卸装",7,false);
        mSurface.requestFocus();
    }
    private LinearLayout row(LinearLayout parent) {
        LinearLayout row = new LinearLayout(this); row.setGravity(Gravity.CENTER);
        parent.addView(row,new LinearLayout.LayoutParams(-1,0,1)); return row;
    }
    private void button(LinearLayout row, String label, int action, boolean repeat) {
        Button key = new Button(this); key.setText(label); key.setTextSize(12); key.setFocusable(false);
        row.addView(key,new LinearLayout.LayoutParams(0,-1,1)); buttons.add(key);
        Runnable next = new Runnable() { public void run() {
            if (!key.isPressed()) return;
            nativeButton(action,true); repeats.postDelayed(this,100);
        }};
        key.setOnTouchListener((v,event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    key.setPressed(true); nativeButton(action,true);
                    if (repeat) repeats.postDelayed(next,300); return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    key.setPressed(false); repeats.removeCallbacks(next); nativeButton(action,false); return true;
                default: return true;
            }
        });
    }
    @Override protected void onPause() {
        repeats.removeCallbacksAndMessages(null);
        for (Button button : buttons) button.setPressed(false);
        super.onPause();
    }
}
