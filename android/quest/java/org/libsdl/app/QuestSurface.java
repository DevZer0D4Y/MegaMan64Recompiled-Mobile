package org.libsdl.app;

import android.content.Context;
import android.view.Surface;
import android.view.SurfaceHolder;

/** SDL renders into the OpenXR Surface rather than the Android window. */
public final class QuestSurface extends SDLSurface {
    private Surface xrSurface;
    private boolean announced;
    public QuestSurface(Context context) { super(context); }
    @Override public Surface getNativeSurface() { return xrSurface; }
    @Override public void surfaceCreated(SurfaceHolder holder) { }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { }
    @Override public void surfaceDestroyed(SurfaceHolder holder) { }
    public boolean hasXRSurface() { return xrSurface != null; }
    public void setXRSurface(Surface surface) {
        xrSurface = surface;
        if (surface == null) {
            mIsSurfaceReady = false;
            SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED;
            SDLActivity.handleNativeState();
            return;
        }
        mWidth = 1600; mHeight = 900;
        SDLActivity.nativeSetScreenResolution(1600, 900, 1600, 900, 72);
        if (!announced) { SDLActivity.onNativeSurfaceCreated(); announced = true; }
        SDLActivity.onNativeResize();
        SDLActivity.onNativeSurfaceChanged();
        // XR visibility is authoritative; Android window focus can remain false in immersive mode.
        SDLActivity.mHasFocus = true;
        mIsSurfaceReady = true;
        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED;
        SDLActivity.handleNativeState();
    }
}
