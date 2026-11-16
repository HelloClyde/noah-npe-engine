package com.helloclyde.npe;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;

public class LauncherActivity extends Activity {
    private boolean importing;
    @Override public void onCreate(Bundle state) { super.onCreate(state); showLibrary(); }
    @Override protected void onResume() { super.onResume(); if (!importing) showLibrary(); }

    private void showLibrary() {
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(20 * getResources().getDisplayMetrics().density);
        list.setPadding(pad, pad, pad, pad);
        ScrollView scroll = new ScrollView(this); scroll.addView(list); setContentView(scroll);
        TextView title = new TextView(this); title.setText("诺亚舟-NPE-引擎\n作者 HelloClyde"); title.setTextSize(22); list.addView(title);
        Button open = new Button(this); open.setText("选择并导入 NPE 游戏"); list.addView(open);
        open.setOnClickListener(view -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE); intent.setType("*/*");
            startActivityForResult(intent, 1);
        });
        TextView help = new TextView(this);
        help.setText("游戏文件需自行提供。导入后可离线游玩。\n用游戏菜单手动存档；卸载应用会删除已导入游戏和存档。\n屏幕 A 确认/翻页，B 返回/菜单，X 对话，Y 卸装；答题时 A/B/X/Y 对应 A/B/C/D。支持实体手柄。\n"); list.addView(help);
        File[] games = new File(getFilesDir(), "games").listFiles((dir, name) -> name.endsWith(".npe"));
        if (games == null) return;
        Arrays.sort(games, (a,b) -> Long.compare(b.lastModified(), a.lastModified()));
        for (File game : games) {
            String label = game.getName();
            try { label = new String(readAll(new FileInputStream(game.getPath()+".name"), 4096), StandardCharsets.UTF_8); }
            catch (IOException ignored) { }
            Button play = new Button(this); play.setText(label); list.addView(play);
            play.setOnClickListener(view -> launch(game));
        }
    }
    private void launch(File game) {
        Intent intent = new Intent(this, GameActivity.class);
        intent.putExtra("game", game.getAbsolutePath()); startActivity(intent);
    }
    private static byte[] readAll(InputStream source, int limit) throws IOException {
        try (InputStream in = source; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[8192]; int n;
            while ((n = in.read(buffer)) != -1) {
                if (out.size() + n > limit) throw new IOException("文件过大");
                out.write(buffer, 0, n);
            }
            return out.toByteArray();
        }
    }
    private void copyFonts() throws IOException {
        File assets = new File(getFilesDir(), "assets");
        if (!assets.isDirectory() && !assets.mkdirs()) throw new IOException("无法创建字体目录");
        for (String name : new String[]{"gbk12-native.bin", "gbk16-native.bin"}) {
            try (InputStream in = getAssets().open("assets/" + name);
                 FileOutputStream out = new FileOutputStream(new File(assets, name))) {
                byte[] buffer = new byte[16384]; int n;
                while ((n = in.read(buffer)) != -1) out.write(buffer, 0, n);
            }
        }
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != 1 || result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData(); importing = true;
        TextView progress = new TextView(this); progress.setText("正在导入游戏…"); setContentView(progress);
        new Thread(() -> {
            File pending = null;
            try {
                copyFonts();
                File games = new File(getFilesDir(), "games");
                if (!games.isDirectory() && !games.mkdirs()) throw new IOException("无法创建游戏目录");
                String label = "NPE 游戏";
                try (Cursor cursor = getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst()) label = cursor.getString(0);
                }
                pending = File.createTempFile("import-", ".tmp", games);
                MessageDigest digest = MessageDigest.getInstance("SHA-256");
                byte[] header = new byte[12]; int total = 0;
                try (InputStream in = getContentResolver().openInputStream(uri); FileOutputStream out = new FileOutputStream(pending)) {
                    if (in == null) throw new IOException("无法打开游戏文件");
                    byte[] buffer = new byte[16384]; int n;
                    while ((n = in.read(buffer)) != -1) {
                        if (total + n > 128 * 1024 * 1024) throw new IOException("NPE 文件不能超过 128MB");
                        if (total < 12) System.arraycopy(buffer, 0, header, total, Math.min(n, 12-total));
                        total += n; digest.update(buffer, 0, n); out.write(buffer, 0, n);
                    }
                }
                if (total < 12 || !new String(header, StandardCharsets.US_ASCII).equals("NP560 RPG2.1"))
                    throw new IOException("请选择 NP560 RPG2.1 格式的 NPE 游戏");
                StringBuilder hash = new StringBuilder(); for (byte value : digest.digest()) hash.append(String.format("%02x", value & 255));
                File game = new File(games, hash + ".npe");
                if (game.exists()) { if (!pending.delete()) throw new IOException("无法清理导入临时文件"); }
                else if (!pending.renameTo(game)) throw new IOException("无法保存导入游戏");
                pending = null;
                try (FileOutputStream out = new FileOutputStream(game.getPath()+".name")) {
                    out.write(label.getBytes(StandardCharsets.UTF_8));
                }
                runOnUiThread(() -> { importing = false; showLibrary(); launch(game); });
            } catch (Exception error) {
                if (pending != null) pending.delete();
                runOnUiThread(() -> { importing = false; showLibrary(); new AlertDialog.Builder(this)
                    .setTitle("导入失败").setMessage(error.getMessage()).setPositiveButton("确定", null).show(); });
            }
        }, "NPE import").start();
    }
}
