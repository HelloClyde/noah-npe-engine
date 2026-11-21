package com.helloclyde.npe;

import android.app.AlertDialog;
import android.content.res.Configuration;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import org.libsdl.app.SDLActivity;
import java.util.ArrayList;

public class GameActivity extends SDLActivity {
    private final Handler repeats = new Handler(Looper.getMainLooper());
    private final ArrayList<Press> presses = new ArrayList<>();
    private RelativeLayout controls;
    private Pad pad;
    private int lastWidth, lastHeight, lastSize, transparency = 35, buttonSize = 100;
    private static native void nativeButton(int action, boolean down);
    @Override protected String[] getLibraries() { return new String[]{"SDL2", "main"}; }
    @Override protected String[] getArguments() { return new String[]{getIntent().getStringExtra("game")}; }
    private int dp(float value) { return Math.round(value * getResources().getDisplayMetrics().density); }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (mLayout == null || mSurface == null) return;
        transparency = Math.max(0, Math.min(90, getPreferences(0).getInt("touch_transparency", 35)));
        buttonSize = TouchControls.clampSize(getPreferences(0).getInt("touch_size", 100));
        mLayout.setMotionEventSplittingEnabled(true);
        mLayout.setOnApplyWindowInsetsListener((view, insets) -> {
            int left, top, right, bottom;
            if (Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets safe = insets.getInsets(WindowInsets.Type.systemBars()
                    | WindowInsets.Type.displayCutout() | WindowInsets.Type.mandatorySystemGestures());
                left = safe.left; top = safe.top; right = safe.right; bottom = safe.bottom;
            } else {
                left = insets.getSystemWindowInsetLeft(); top = insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight(); bottom = insets.getSystemWindowInsetBottom();
                if (Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
                    left = Math.max(left, insets.getDisplayCutout().getSafeInsetLeft());
                    right = Math.max(right, insets.getDisplayCutout().getSafeInsetRight());
                    top = Math.max(top, insets.getDisplayCutout().getSafeInsetTop());
                    bottom = Math.max(bottom, insets.getDisplayCutout().getSafeInsetBottom());
                }
            }
            if (view.getPaddingLeft()!=left || view.getPaddingTop()!=top || view.getPaddingRight()!=right || view.getPaddingBottom()!=bottom) {
                view.setPadding(left, top, right, bottom); view.post(this::layoutControls);
            }
            return insets;
        });
        mLayout.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob) -> layoutControls());
        mLayout.post(this::layoutControls);
        mLayout.requestApplyInsets(); mSurface.requestFocus();
    }
    @Override public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        lastWidth = lastHeight = 0;
        if (mLayout != null) mLayout.post(this::layoutControls);
    }
    private void place(View child, int width, int height, int x, int y) {
        RelativeLayout.LayoutParams params = new RelativeLayout.LayoutParams(dp(width), dp(height));
        params.leftMargin = dp(x); params.topMargin = dp(y); controls.addView(child, params);
    }
    private void layoutControls() {
        if (mLayout == null || mSurface == null) return;
        int pixelsW = mLayout.getWidth()-mLayout.getPaddingLeft()-mLayout.getPaddingRight();
        int pixelsH = mLayout.getHeight()-mLayout.getPaddingTop()-mLayout.getPaddingBottom();
        if (pixelsW <= 0 || pixelsH <= 0 || (lastWidth==pixelsW && lastHeight==pixelsH && lastSize==buttonSize)) return;
        releaseControls(); presses.clear();
        if (controls != null) mLayout.removeView(controls);
        lastWidth = pixelsW; lastHeight = pixelsH; lastSize = buttonSize;
        float density = getResources().getDisplayMetrics().density;
        int width = (int)(pixelsW/density), height = (int)(pixelsH/density);
        TouchControls.Layout layout = new TouchControls.Layout(width, height, buttonSize);
        // Full game surface: controls no longer reserve or shrink a bottom strip.
        mSurface.setLayoutParams(new RelativeLayout.LayoutParams(-1,-1));
        controls = new RelativeLayout(this); controls.setMotionEventSplittingEnabled(true);
        mLayout.addView(controls, new RelativeLayout.LayoutParams(-1,-1));
        pad = new Pad(); place(pad,layout.size,layout.size,layout.left,layout.top);
        String[] labels = {"A\n确认", "B\n返回", "X\n对话", "Y\n卸装"};
        int[] colors = {0xff267768,0xff8c414a,0xff355d8c,0xff89702b};
        for (int action=4;action<=7;++action) {
            Button key = key(labels[action-4],colors[action-4],true);
            bind(key,action);
            place(key,layout.unit,layout.unit,
                layout.right+TouchControls.column(action)*layout.unit,
                layout.top+TouchControls.row(action)*layout.unit);
        }
        utility("选项左",10,layout.left,layout.utilityTop,layout.utilityUnit,layout.utilityHeight);
        utility("选项右",11,layout.left+layout.utilityUnit,layout.utilityTop,layout.utilityUnit,layout.utilityHeight);
        utility("说明上页",8,width-12-layout.utilityUnit*2,layout.utilityTop,layout.utilityUnit,layout.utilityHeight);
        utility("说明下页",9,width-12-layout.utilityUnit,layout.utilityTop,layout.utilityUnit,layout.utilityHeight);
        controls.setAlpha((100-transparency)/100f);
        // This entry is outside the faded controls, so settings remain reachable.
        Button more = key("更多",0xff343b48,false);
        more.setOnClickListener(v -> showMore());
        RelativeLayout.LayoutParams menu = new RelativeLayout.LayoutParams(dp(72),dp(48));
        menu.addRule(RelativeLayout.ALIGN_PARENT_TOP); menu.addRule(RelativeLayout.CENTER_HORIZONTAL);
        menu.topMargin = dp(4);
        View old = mLayout.findViewWithTag("touch-more"); if (old != null) mLayout.removeView(old);
        more.setTag("touch-more"); mLayout.addView(more,menu);
        mSurface.requestFocus();
    }
    private Button key(String text,int color,boolean round) {
        Button key = new Button(this); key.setText(text); key.setTextSize(round?13:12);
        key.setTextColor(Color.WHITE); key.setAllCaps(false); key.setFocusable(false);
        key.setMinWidth(0); key.setMinimumWidth(0); key.setMinHeight(0); key.setMinimumHeight(0);
        key.setPadding(dp(2),0,dp(2),0); key.setGravity(Gravity.CENTER);
        GradientDrawable shape = new GradientDrawable(); shape.setColor(color);
        if (round) shape.setShape(GradientDrawable.OVAL); else shape.setCornerRadius(dp(10));
        shape.setStroke(dp(1),0xffa2adbb); key.setBackground(new RippleDrawable(ColorStateList.valueOf(0x66ffffff),shape,null)); return key;
    }
    private void utility(String label,int action,int x,int y,int width,int height) {
        Button key = key(label,0xff343b48,false); bind(key,action); place(key,Math.max(1,width-4),height,x+2,y);
    }
    private void bind(Button key,int action) { Press press = new Press(key,action); presses.add(press); key.setOnTouchListener(press); }
    private final class Press implements View.OnTouchListener {
        final Button key; final int action; int pointer=-1; boolean down;
        Press(Button key,int action) { this.key=key;this.action=action; }
        void release() { if(down)nativeButton(action,false); down=false;pointer=-1;key.setPressed(false); }
        @Override public boolean onTouch(View v,MotionEvent e) {
            int type=e.getActionMasked();
            if(type==MotionEvent.ACTION_DOWN) {
                pointer=e.getPointerId(0);down=true;key.setPressed(true);nativeButton(action,true);
            } else if(type==MotionEvent.ACTION_UP || type==MotionEvent.ACTION_CANCEL
                || (type==MotionEvent.ACTION_POINTER_UP && e.getPointerId(e.getActionIndex())==pointer)) {
                release();
            } else if(type==MotionEvent.ACTION_MOVE && down) {
                int index=e.findPointerIndex(pointer);
                if(index<0 || e.getX(index)<0 || e.getY(index)<0 || e.getX(index)>=v.getWidth() || e.getY(index)>=v.getHeight())release();
            }
            return true;
        }
    }
    private final class Pad extends View {
        final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        final TouchControls.DirectionPad input=new TouchControls.DirectionPad(GameActivity::nativeButton);
        final Runnable repeat=new Runnable() { public void run() {
            if(input.direction<0)return;input.repeat();repeats.postDelayed(this,100);
        }};
        Pad() { super(GameActivity.this);setContentDescription("方向键：按住移动，滑动换向");setFocusable(false); }
        void release() { repeats.removeCallbacks(repeat);input.cancel();invalidate(); }
        @Override public boolean onTouchEvent(MotionEvent e) {
            int before=input.direction,type=e.getActionMasked();
            if(type==MotionEvent.ACTION_DOWN)input.down(e.getPointerId(0),e.getX(),e.getY(),getWidth(),getHeight());
            else if(type==MotionEvent.ACTION_MOVE) {
                int i=e.findPointerIndex(input.pointer);if(i>=0)input.move(input.pointer,e.getX(i),e.getY(i),getWidth(),getHeight());else input.cancel();
            } else if(type==MotionEvent.ACTION_UP || type==MotionEvent.ACTION_POINTER_UP)input.up(e.getPointerId(e.getActionIndex()));
            else if(type==MotionEvent.ACTION_CANCEL)input.cancel();
            if(before!=input.direction) { repeats.removeCallbacks(repeat);if(input.direction>=0)repeats.postDelayed(repeat,300); }
            invalidate();return true;
        }
        @Override protected void onDraw(Canvas canvas) {
            float unit=getWidth()/3f,center=getWidth()/2f;
            paint.setColor(0xff303944);canvas.drawRoundRect(unit,0,unit*2,getHeight(),unit/5,unit/5,paint);
            canvas.drawRoundRect(0,unit,getWidth(),unit*2,unit/5,unit/5,paint);
            float[] xs={center,unit*2.5f,center,unit*.5f},ys={unit*.5f,center,unit*2.5f,center};
            String[] labels={"↑","→","↓","←"};paint.setTextSize(unit*.52f);paint.setTextAlign(Paint.Align.CENTER);
            Paint.FontMetrics fm=paint.getFontMetrics();
            for(int i=0;i<4;++i) {
                if(input.direction==i){paint.setColor(0xff4faaa0);canvas.drawCircle(xs[i],ys[i],unit*.46f,paint);}
                paint.setColor(Color.WHITE);canvas.drawText(labels[i],xs[i],ys[i]-(fm.ascent+fm.descent)/2,paint);
            }
            paint.setColor(0xff657181);canvas.drawCircle(center,center,unit*.1f,paint);
        }
    }
    private void showMore() {
        releaseControls();
        new AlertDialog.Builder(this).setTitle("触屏设置")
            .setItems(new String[]{"按键透明度", "按键大小", "按键说明", "返回游戏列表"},(d,which)-> {
                if(which==0)showTouchSlider(false);
                else if(which==1)showTouchSlider(true);
                else if(which==2)new AlertDialog.Builder(this).setTitle("任天堂布局")
                    .setMessage("左侧十字键：按住移动，滑动换向。\nA：确认／对话继续；B：返回／游戏菜单；X：发起对话；Y：卸装。\n选项左／右：购买框切换是／否。说明上页／下页：P／N。购买物品说明也可用十字键左右翻页。\n答题：十字键选答案，A 确认，B 结束答题。遇到输入题，X 打开键盘，Y 删除。")
                    .setPositiveButton("知道了",null).show();
                else new AlertDialog.Builder(this).setTitle("返回游戏列表？")
                    .setMessage("请先用游戏菜单存档，未保存的进度不会自动保存。")
                    .setPositiveButton("返回列表",(a,b)->finish()).setNegativeButton("继续游戏",null).show();
            }).show();
    }
    private void previewTouchSetting(boolean size, int value) {
        if (size) {
            buttonSize = value;
            layoutControls();
        } else {
            transparency = value;
            controls.setAlpha((100-value)/100f);
        }
    }

    private void showTouchSlider(boolean size) {
        int previous = size ? buttonSize : transparency;
        int minimum = size ? TouchControls.MIN_SIZE : 0;
        int maximum = size ? TouchControls.MAX_SIZE : 90;
        String title = size ? "按键大小" : "按键透明度";
        String preference = size ? "touch_size" : "touch_transparency";
        LinearLayout box=new LinearLayout(this);box.setOrientation(LinearLayout.VERTICAL);box.setPadding(dp(20),dp(12),dp(20),0);
        TextView value=new TextView(this);value.setTextColor(Color.WHITE);box.addView(value);
        SeekBar slider=new SeekBar(this);slider.setMax(maximum-minimum);slider.setProgress(previous-minimum);box.addView(slider,new LinearLayout.LayoutParams(-1,dp(56)));
        value.setText(previous+"%");
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar bar,int progress,boolean user) {
                int selected = minimum + progress;
                previewTouchSetting(size, selected);
                value.setText(selected+"%");
            }
            public void onStartTrackingTouch(SeekBar bar) {} public void onStopTrackingTouch(SeekBar bar) {}
        });
        new AlertDialog.Builder(this).setTitle(title).setView(box)
            .setPositiveButton("保存",(d,w)->getPreferences(0).edit().putInt(preference,size?buttonSize:transparency).apply())
            .setNegativeButton("取消",(d,w)->previewTouchSetting(size,previous))
            .setOnCancelListener(d->previewTouchSetting(size,previous)).show();
    }
    private void releaseControls() {
        repeats.removeCallbacksAndMessages(null);if(pad!=null)pad.release();for(Press press:presses)press.release();
    }
    @Override public void onWindowFocusChanged(boolean focus) { super.onWindowFocusChanged(focus);if(!focus)releaseControls(); }
    @Override protected void onPause() { releaseControls();super.onPause(); }
}
