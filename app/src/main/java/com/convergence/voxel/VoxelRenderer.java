package com.convergence.voxel;

import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.os.SystemClock;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** Bridges Android's GL thread to the native voxel renderer and camera. */
final class VoxelRenderer implements GLSurfaceView.Renderer {
    static {
        System.loadLibrary("convergence");
    }

    private final AssetManager assetManager;
    private volatile float aspect = 1.0f;

    VoxelRenderer(AssetManager assetManager) {
        this.assetManager = assetManager;
    }

    private native void nativeInit(AssetManager assets);
    private native void nativeResize(int width, int height);
    private native void nativeRender(long nowMs, float aspect);
    private native void nativeHandleLook(float deltaX, float deltaY);
    private native void nativeHandleJoystick(float forward, float strafe);
    private native void nativeResetFrameClock();

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        nativeInit(assetManager);
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        aspect = (float) width / (float) Math.max(height, 1);
        nativeResize(width, height);
    }

    @Override
    public void onDrawFrame(GL10 gl) {
        nativeRender(SystemClock.uptimeMillis(), aspect);
    }

    void submitLook(float deltaX, float deltaY) {
        nativeHandleLook(deltaX, deltaY);
    }

    void submitJoystick(float forward, float strafe) {
        nativeHandleJoystick(forward, strafe);
    }

    void resetFrameClock() {
        nativeResetFrameClock();
    }
}
