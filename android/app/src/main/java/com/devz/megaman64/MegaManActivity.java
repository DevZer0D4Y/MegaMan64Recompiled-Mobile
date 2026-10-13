package com.devz.megaman64;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

public class MegaManActivity extends SDLActivity {
    private static final int DOCUMENTS = 64;
    private boolean documentRequest;
    private android.widget.FrameLayout touchControls;
    private int touchMask;
    private int controlsMode;
    private boolean navigationPad;
    private boolean editingControls, editingMenus;
    private float dragX, dragY, dragOriginX, dragOriginY;
    private int layoutLeft,layoutTop,layoutWidth,layoutHeight;
    private android.content.SharedPreferences layoutPreferences(){return getSharedPreferences("touchLayouts",MODE_PRIVATE);}
    private String layoutProfile(){return (editingControls?editingMenus:(controlsMode!=1||navigationPad))?"menu.":"game.";}
    private void editControls() {
        resetTouchControls();
        if(!editingControls) {
            new android.app.AlertDialog.Builder(this).setTitle("Edit touch controls")
                .setItems(new String[]{"Gameplay layout","Menu layout"},(dialog,which)->{
                    editingMenus=which==1;editingControls=true;
                    if(controlsMode==1)nativeMenu();
                    updateControlLayout();
                    Toast.makeText(this,"Drag controls to move them. Tap DONE for size, style and saving.",Toast.LENGTH_LONG).show();
                }).setNegativeButton("Cancel",null).show();
        } else {
            String profile=layoutProfile();android.content.SharedPreferences preferences=layoutPreferences();
            float scale=preferences.getFloat(profile+"scale",1),alpha=preferences.getFloat(profile+"alpha",1);
            new android.app.AlertDialog.Builder(this).setTitle(editingMenus?"Menu controls":"Gameplay controls")
                .setItems(new String[]{"Save & done","Larger controls","Smaller controls","More visible","More transparent","Change color style","Reset this layout"},(dialog,which)->{
                    android.content.SharedPreferences.Editor editor=preferences.edit();
                    switch(which){
                        case 0:editingControls=false;editor.commit();break;
                        case 1:editor.putFloat(profile+"scale",Math.min(1.6f,scale+.1f)).apply();break;
                        case 2:editor.putFloat(profile+"scale",Math.max(.7f,scale-.1f)).apply();break;
                        case 3:editor.putFloat(profile+"alpha",Math.min(1,alpha+.1f)).apply();break;
                        case 4:editor.putFloat(profile+"alpha",Math.max(.35f,alpha-.1f)).apply();break;
                        case 5:editor.putInt(profile+"theme",(preferences.getInt(profile+"theme",0)+1)%3).apply();break;
                        case 6:for(String key:preferences.getAll().keySet())if(key.startsWith(profile))editor.remove(key);editor.apply();break;
                    }
                    updateControlLayout();
                }).setNegativeButton("Keep editing",null).show();
        }
    }
    private boolean dragControl(android.view.View control,android.view.MotionEvent event) {
        switch(event.getActionMasked()){
            case android.view.MotionEvent.ACTION_DOWN:dragX=event.getRawX();dragY=event.getRawY();dragOriginX=control.getX();dragOriginY=control.getY();break;
            case android.view.MotionEvent.ACTION_MOVE:
                float x=Math.max(layoutLeft,Math.min(layoutLeft+layoutWidth-control.getWidth(),dragOriginX+event.getRawX()-dragX));
                float y=Math.max(layoutTop,Math.min(layoutTop+layoutHeight-control.getHeight(),dragOriginY+event.getRawY()-dragY));
                control.setX(x);control.setY(y);break;
            case android.view.MotionEvent.ACTION_UP:
            case android.view.MotionEvent.ACTION_CANCEL:
                if(layoutWidth>0&&layoutHeight>0){String key=layoutProfile()+control.getId();layoutPreferences().edit()
                    .putFloat(key+"x",(control.getX()+control.getWidth()/2f-layoutLeft)/layoutWidth)
                    .putFloat(key+"y",(control.getY()+control.getHeight()/2f-layoutTop)/layoutHeight).apply();}break;
        }
        return true;
    }
    private static native void nativeMenuKey(int action, boolean pressed);
    private static native void nativeFlushSaves();
    private final int[] navMasks = {1<<18,1<<19,1<<17,1<<16,0x8000,0x4000,0x1000};
    private final android.os.Handler controlHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private Runnable repeatingNavigation;
    private int heldNavigation = -1;
    private final class PanelButton extends android.widget.Button {
        final android.graphics.Paint paint = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
        final android.graphics.Path panel = new android.graphics.Path();
        PanelButton(String label) { super(MegaManActivity.this); setText(label); setContentDescription(label);
            setBackgroundColor(android.graphics.Color.TRANSPARENT); setPadding(0,0,0,0); setAllCaps(false); setElevation(0); }
        @Override protected void onDraw(android.graphics.Canvas canvas) {
            float w=getWidth(),h=getHeight(),cut=Math.min(w,h)*.18f;
            int theme=layoutPreferences().getInt(layoutProfile()+"theme",0);
            int top=theme==1?0xce175961:(theme==2?0xce444b57:0xce234966);
            int edge=theme==1?0xffffc45c:(theme==2?0xffa5d693:0xff6bd9ef);
            panel.reset();panel.moveTo(cut,1);panel.lineTo(w-cut,1);panel.lineTo(w-1,cut);panel.lineTo(w-1,h-cut);
            panel.lineTo(w-cut,h-1);panel.lineTo(cut,h-1);panel.lineTo(1,h-cut);panel.lineTo(1,cut);panel.close();
            paint.setStyle(android.graphics.Paint.Style.FILL);
            paint.setShader(new android.graphics.LinearGradient(0,0,0,h,isPressed()?0xee397db3:top,0xb809182f,android.graphics.Shader.TileMode.CLAMP));
            canvas.drawPath(panel,paint);paint.setShader(null);paint.setStyle(android.graphics.Paint.Style.STROKE);
            paint.setStrokeWidth(2*getResources().getDisplayMetrics().density);
            paint.setColor(getText().toString().equals("A")||getText().toString().equals("OK")?0xffffc45c:edge);canvas.drawPath(panel,paint);
            paint.setStyle(android.graphics.Paint.Style.FILL);paint.setColor(0xffe2f6ff);
            paint.setTypeface(android.graphics.Typeface.create("sans-serif-condensed",android.graphics.Typeface.BOLD));
            paint.setTextAlign(android.graphics.Paint.Align.CENTER);paint.setTextSize(Math.min(h*.34f,18*getResources().getDisplayMetrics().scaledDensity));
            canvas.drawText(getText().toString(),w/2,h/2-(paint.ascent()+paint.descent())/2,paint);
        }
    }
    private static native void nativeTouch(int mask);
    private static native void nativeMenu();
    private static native void nativeGamePad();
    private static native void nativeStick(float x, float y);
    private final class AnalogStick extends android.view.View {
        private float stickX, stickY;
        private int activePointer=-1;
        private final android.graphics.Paint paint = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG);
        AnalogStick() { super(MegaManActivity.this); setContentDescription("Movement stick"); }
        void reset() { activePointer=-1;stickX=stickY=0; nativeStick(0,0); invalidate(); }
        @Override protected void onDraw(android.graphics.Canvas canvas) {
            float cx=getWidth()/2f,cy=getHeight()/2f,radius=Math.min(cx,cy)*.92f;
            int theme=layoutPreferences().getInt(layoutProfile()+"theme",0);
            paint.setColor(theme==1?0xaa175961:(theme==2?0xaa444b57:0xaa16344e));canvas.drawCircle(cx,cy,radius,paint);
            paint.setColor(theme==1?0xb8ffc45c:(theme==2?0xb8a5d693:0xb86bd9ef)); canvas.drawCircle(cx+stickX*radius*.7f,cy+stickY*radius*.7f,radius*.38f,paint);
        }
        @Override public boolean onTouchEvent(android.view.MotionEvent event) {
            if(editingControls)return dragControl(this,event);
            int action=event.getActionMasked();
            if(action==android.view.MotionEvent.ACTION_UP || action==android.view.MotionEvent.ACTION_CANCEL ||
                (action==android.view.MotionEvent.ACTION_POINTER_UP && event.getPointerId(event.getActionIndex())==activePointer)) { reset(); return true; }
            if(action==android.view.MotionEvent.ACTION_DOWN)activePointer=event.getPointerId(event.getActionIndex());
            if(action!=android.view.MotionEvent.ACTION_DOWN && action!=android.view.MotionEvent.ACTION_MOVE)return true;
            int pointer=event.findPointerIndex(activePointer);if(pointer<0)return true;
            float radius=Math.min(getWidth(),getHeight())*.32f;
            if(radius<=0) return true;
            float x=(event.getX(pointer)-getWidth()/2f)/radius,y=(event.getY(pointer)-getHeight()/2f)/radius;
            float length=(float)Math.hypot(x,y);
            if(length>1) { x/=length; y/=length; }
            stickX=x; stickY=y; invalidate();
            float magnitude=Math.min(length,1),output=magnitude<=.12f?0:(magnitude-.12f)/.88f;
            if(magnitude>0) { x=x/magnitude*output; y=y/magnitude*output; }
            nativeStick(x,-y); return true;
        }
    }
    private void releaseNavigation() {
        if (repeatingNavigation != null) controlHandler.removeCallbacks(repeatingNavigation);
        repeatingNavigation=null;
        if (heldNavigation >= 0) {
            nativeMenuKey(heldNavigation,false);
            touchMask &= ~navMasks[heldNavigation];nativeTouch(touchMask);
        }
        heldNavigation=-1;
    }
    private void navigation(int action,boolean down) {
        if (!down) { if(heldNavigation==action) releaseNavigation(); return; }
        releaseNavigation();heldNavigation=action;
        if(controlsMode==1) { touchMask|=navMasks[action];nativeTouch(touchMask);return; }
        nativeMenuKey(action,true);
        if(action<4) {
            repeatingNavigation=new Runnable(){public void run(){if(heldNavigation==action && controlsMode!=1){nativeMenuKey(action,true);controlHandler.postDelayed(this,95);}}};
            controlHandler.postDelayed(repeatingNavigation,380);
        }
    }
    private void updateControlLayout() {
        if(touchControls==null)return;
        boolean menu=editingControls?editingMenus:(controlsMode!=1||navigationPad);
        for(int i=0;i<touchControls.getChildCount();i++) {
            android.view.View control=touchControls.getChildAt(i);
            boolean visible=i==23 || (i==15 ? (editingControls ? !menu : (controlsMode==1||controlsMode==3)) : (i>=16&&i<=22 ? menu : !menu));
            if(control instanceof AnalogStick)visible=!menu;
            control.setVisibility(visible?android.view.View.VISIBLE:android.view.View.GONE);
            if(i==15){
                ((PanelButton)control).setText(menu?"GAME PAD":"MENU PAD");
                control.setContentDescription(menu?"Switch to gameplay pad":"Switch to menu pad");
                control.setElevation(9*getResources().getDisplayMetrics().density);
            }
            if(i==23){((PanelButton)control).setText(editingControls?"DONE":"EDIT");control.setElevation(10*getResources().getDisplayMetrics().density);}
            control.setAlpha(i==23?1:layoutPreferences().getFloat(layoutProfile()+"alpha",1));
            control.invalidate();
            if(i==22)((PanelButton)control).setText(controlsMode==1?"START":"APPLY");
        }
        touchControls.setVisibility(controlsMode==0?android.view.View.GONE:android.view.View.VISIBLE);
        touchControls.requestLayout();
        touchControls.post(this::layoutControls);
    }
    private void layoutControls() {
        if(touchControls==null||touchControls.getWidth()<=0||touchControls.getHeight()<=0)return;
        android.view.View v=touchControls;int l=0,t=0,r=touchControls.getWidth(),b=touchControls.getHeight();

                    int left=0,top=0,right=0,bottom=0;
                    android.view.WindowInsets insets=v.getRootWindowInsets();
                    if(insets!=null){left=insets.getSystemWindowInsetLeft();right=insets.getSystemWindowInsetRight();top=insets.getSystemWindowInsetTop();bottom=insets.getSystemWindowInsetBottom();
                        if(android.os.Build.VERSION.SDK_INT>=28&&insets.getDisplayCutout()!=null){android.view.DisplayCutout cut=insets.getDisplayCutout();left=Math.max(left,cut.getSafeInsetLeft());right=Math.max(right,cut.getSafeInsetRight());top=Math.max(top,cut.getSafeInsetTop());bottom=Math.max(bottom,cut.getSafeInsetBottom());}}
                    int w=r-l-left-right,h=b-t-top-bottom;
                    layoutLeft=left;layoutTop=top;layoutWidth=w;layoutHeight=h;
                    float scale=layoutPreferences().getFloat(layoutProfile()+"scale",1);
                    int size=(int)(Math.min(54*getResources().getDisplayMetrics().density,h*.15f)*scale);
                    for(int i=0;i<touchControls.getChildCount();i++){
                        android.view.View control=touchControls.getChildAt(i);int width=size,height=size;float x,y;
                        if(control instanceof AnalogStick){width=height=(int)(Math.min(155*getResources().getDisplayMetrics().density,h*.48f)*scale);x=.14f;y=.70f;}
                        else{float[] pos=(float[])control.getTag();x=pos[0];y=pos[1];if(i==5||i==14||i==15||i>=20)width=(int)(size*1.5f);}
                        String key=layoutProfile()+control.getId();
                        if(i!=23){x=layoutPreferences().getFloat(key+"x",x);y=layoutPreferences().getFloat(key+"y",y);}
                        android.widget.FrameLayout.LayoutParams lp=new android.widget.FrameLayout.LayoutParams(width,height);
                        lp.leftMargin=left+Math.max(0,Math.min(w-width,(int)(w*x-width/2)));lp.topMargin=top+Math.max(0,Math.min(h-height,(int)(h*y-height/2)));control.setLayoutParams(lp);
                    }
    }
    public void setControlsMode(int mode) {
        runOnUiThread(() -> {
            controlsMode=mode;navigationPad=false;
            if (touchControls == null) {
                touchControls = new android.widget.FrameLayout(this);
                touchControls.setMotionEventSplittingEnabled(true);
                String[] names={"A","B","Z","L","R","START","C↑","C↓","C←","C→","D↑","D↓","D←","D→","MENU","PAD","↑","↓","←","→","OK","BACK","APPLY","EDIT"};
                int[] masks={0x8000,0x4000,0x2000,0x20,0x10,0x1000,8,4,2,1,0x800,0x400,0x200,0x100,0,0};
                float[][] positions={{.92f,.68f},{.83f,.82f},{.08f,.12f},{.22f,.12f},{.90f,.12f},{.44f,.10f},{.73f,.39f},{.73f,.69f},{.65f,.54f},{.81f,.54f},{.39f,.55f},{.39f,.85f},{.32f,.70f},{.46f,.70f},{.56f,.10f},{.68f,.10f},{.13f,.65f},{.13f,.90f},{.065f,.775f},{.195f,.775f},{.88f,.85f},{.75f,.85f},{.59f,.85f},{.32f,.10f}};
                for(int i=0;i<names.length;i++) {
                    final int index=i;PanelButton button=new PanelButton(names[i]);button.setTag(positions[i]);button.setId(i+1);
                    button.setOnTouchListener((v,event)->{
                        if(editingControls&&index!=23)return dragControl(button,event);
                        int type=event.getActionMasked();boolean down=type==android.view.MotionEvent.ACTION_DOWN;
                        boolean up=type==android.view.MotionEvent.ACTION_UP||type==android.view.MotionEvent.ACTION_CANCEL;
                        if(!down&&!up)return true;
                        button.setPressed(down);button.invalidate();
                        if(index==23){if(down)editControls();}
                        else if(index>=16)navigation(index-16,down);
                        else if(index==15&&down){
                            resetTouchControls();
                            if(controlsMode==3){navigationPad=false;nativeGamePad();}
                            else navigationPad=!navigationPad;
                            updateControlLayout();
                        }
                        else if(index==14&&down){resetTouchControls();nativeMenu();}
                        else if(index<14){if(down)touchMask|=masks[index];else touchMask&=~masks[index];nativeTouch(touchMask);}
                        return true;
                    });
                    touchControls.addView(button,new android.widget.FrameLayout.LayoutParams(64,64));
                }
                AnalogStick analog=new AnalogStick();analog.setId(25);touchControls.addView(analog,new android.widget.FrameLayout.LayoutParams(150,150));
                touchControls.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->layoutControls());
                addContentView(touchControls,new android.view.ViewGroup.LayoutParams(-1,-1));
            }
            resetTouchControls();updateControlLayout();
        });
    }
    private static native void nativeDocumentsSelected(String[] paths);

    @Override protected String[] getLibraries() { return new String[] {"SDL2", "main"}; }

    @Override protected void onCreate(Bundle state) {
        // std::ifstream and mod files need ordinary filesystem paths, not APK URIs.
        try {
            copyAssetTree("assets", new File(getFilesDir(), "assets"));
            copyAsset("recompcontrollerdb.txt", new File(getFilesDir(), "recompcontrollerdb.txt"));
        } catch (IOException error) {
            throw new IllegalStateException("Unable to unpack game assets", error);
        }
        super.onCreate(state);
    }

    private void copyAssetTree(String source, File destination) throws IOException {
        String[] entries = getAssets().list(source);
        if (entries == null) { throw new IOException("Cannot list " + source); }
        if (entries.length == 0) { copyAsset(source, destination); return; }
        if (!destination.isDirectory() && !destination.mkdirs()) {
            throw new IOException("Cannot create " + destination);
        }
        for (String entry : entries) { copyAssetTree(source + "/" + entry, new File(destination, entry)); }
    }

    private void copyAsset(String source, File destination) throws IOException {
        try (InputStream input = getAssets().open(source);
             FileOutputStream output = new FileOutputStream(destination)) {
            transfer(input, output, Long.MAX_VALUE);
        }
    }

    private static void transfer(InputStream input, FileOutputStream output, long limit) throws IOException {
        byte[] buffer = new byte[65536];
        long total = 0;
        int count;
        while ((count = input.read(buffer)) != -1) {
            total += count;
            if (total > limit) { throw new IOException("Selected file is too large"); }
            output.write(buffer, 0, count);
        }
    }

    // Called from native code. Android owns the picker and grants access to each URI.
    public void openDocuments(boolean multiple) {
        runOnUiThread(() -> {
            if (documentRequest) { return; }
            documentRequest = true;
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, multiple);
            try { startActivityForResult(intent, DOCUMENTS); }
            catch (RuntimeException error) {
                documentRequest = false;
                nativeDocumentsSelected(new String[0]);
                Toast.makeText(this, "No file picker available", Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        if (request != DOCUMENTS) { super.onActivityResult(request, result, data); return; }
        // The provider may be remote. Copy away from the Android UI thread.
        new Thread(() -> {
            ArrayList<String> paths = new ArrayList<>();
            ArrayList<File> imported = new ArrayList<>();
            try {
                if (result == Activity.RESULT_OK && data != null) {
                    if (data.getClipData() != null) {
                        for (int i = 0; i < data.getClipData().getItemCount(); ++i) {
                            importDocument(data.getClipData().getItemAt(i).getUri(), paths, imported);
                        }
                    } else if (data.getData() != null) { importDocument(data.getData(), paths, imported); }
                }
            } catch (IOException | RuntimeException error) {
                Log.e("MegaMan64", "Document import failed", error);
                paths.clear();
                for (File file : imported) { file.delete(); }
                runOnUiThread(() -> Toast.makeText(this, "Unable to import selected file", Toast.LENGTH_LONG).show());
            } finally {
                runOnUiThread(() -> {
                    documentRequest = false;
                    nativeDocumentsSelected(paths.toArray(new String[0]));
                });
            }
        }, "ROM import").start();
    }

    private void importDocument(Uri uri, ArrayList<String> paths, ArrayList<File> imported) throws IOException {
        // Keep the extension because the mod installer selects formats by extension.
        String name = "document";
        try (android.database.Cursor cursor = getContentResolver().query(uri,
                new String[] {android.provider.OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) { name = cursor.getString(0); }
        }
        // Keep the original basename: RT64 texture mods also derive IDs from it.
        name = name == null ? "document.bin" : new File(name.replace('\\', '/')).getName();
        if (name.isEmpty() || name.equals(".") || name.equals("..")) { name = "document.bin"; }
        File directory = new File(getCacheDir(), "imports");
        if (!directory.isDirectory() && !directory.mkdirs()) { throw new IOException("Cannot create import directory"); }
        File uniqueDirectory = new File(directory, java.util.UUID.randomUUID().toString());
        if (!uniqueDirectory.mkdir()) { throw new IOException("Cannot create import directory"); }
        File output = new File(uniqueDirectory, name);
        imported.add(output);
        try (InputStream input = getContentResolver().openInputStream(uri);
             FileOutputStream stream = new FileOutputStream(output)) {
            if (input == null) { throw new IOException("Cannot open selected document"); }
            transfer(input, stream, 512L * 1024 * 1024);
        } catch (IOException error) { output.delete(); throw error; }
        paths.add(output.getAbsolutePath());
    }

    private void resetTouchControls() {
        releaseNavigation();
        touchMask = 0;
        if (touchControls == null) return;
        nativeTouch(0);
        nativeStick(0,0);
        for (int i=0; i<touchControls.getChildCount(); i++) {
            android.view.View control = touchControls.getChildAt(i);
            control.setPressed(false);control.invalidate();
            if (control instanceof AnalogStick) ((AnalogStick)control).reset();
        }
    }

    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        // The notification shade can take focus without pausing the Activity.
        if (!hasFocus) {
            resetTouchControls();
        } else {
            getWindow().getDecorView().post(() -> {
                getWindow().getDecorView().requestLayout();
                if (touchControls != null) touchControls.requestLayout();
            });
        }
    }

    @Override protected void onPause() {
        resetTouchControls();
        if (!SDLActivity.mBrokenLibraries) nativeFlushSaves();
        super.onPause();
    }

    @Override protected void onDestroy() {
        if (documentRequest) { nativeDocumentsSelected(new String[0]); }
        super.onDestroy();
    }
}
