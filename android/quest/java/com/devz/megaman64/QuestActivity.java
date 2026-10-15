package com.devz.megaman64;

import android.app.AlertDialog;
import android.content.Context;
import android.os.Bundle;
import android.view.Surface;
import org.libsdl.app.SDLSurface;
import org.libsdl.app.QuestSurface;

public final class QuestActivity extends MegaManActivity {
    private QuestSurface questSurface;
    private boolean destroying;
    private static native void nativeStart(QuestActivity activity);
    private static native void nativeStop();
    private static native void nativeMode(int mode);
    private static native void nativeSurfaceEnabled(boolean enabled);
    @Override public void loadLibraries() {
        super.loadLibraries();
        System.loadLibrary("openxr_loader");
        System.loadLibrary("quest_vr");
    }
    @Override public SDLSurface createSDLSurface(Context context) {
        questSurface = new QuestSurface(context);
        return questSurface;
    }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        nativeStart(this);
    }
    @Override public void setControlsMode(int mode) { nativeMode(mode); }
    // Called on the OpenXR thread; all SDL lifecycle changes stay on the UI thread.
    public void onQuestSurface(Surface surface) {
        runOnUiThread(() -> {
            if (destroying) return;
            if (surface != null && mIsResumedCalled) nativeSurfaceEnabled(true);
            questSurface.setXRSurface(surface);
        });
    }
    public void onQuestError(String message) {
        runOnUiThread(() -> {
            if (!destroying) new AlertDialog.Builder(this).setTitle("Quest VR startup")
                .setMessage(message).setPositiveButton("Close", (dialog, which) -> finish()).show();
        });
    }
    @Override public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus || (questSurface != null && questSurface.hasXRSurface()));
    }
    @Override protected void onResume() {
        super.onResume();
        nativeSurfaceEnabled(true);
    }
    @Override protected void onPause() {
        nativeSurfaceEnabled(false);
        super.onPause();
    }
    @Override protected void onDestroy() {
        destroying = true;
        nativeSurfaceEnabled(false);
        // Shut down the game producer before releasing the OpenXR swapchain.
        super.onDestroy();
        nativeStop();
    }
}
