package com.convergence.voxel;

import android.app.Activity;
import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.ViewGroup;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** Android entry point for the Project Convergence rendering prototype. */
public final class MainActivity extends Activity {
    static {
        System.loadLibrary("convergence");
    }

    private GLSurfaceView glSurfaceView;

    private static native void nativeOnSurfaceCreated(AssetManager assetManager);
    private static native void nativeOnSurfaceChanged(int width, int height);
    private static native void nativeOnDrawFrame();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        glSurfaceView = new GLSurfaceView(this);
        glSurfaceView.setEGLContextClientVersion(3);
        glSurfaceView.setRenderer(new ConvergenceRenderer());
        glSurfaceView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        addContentView(
            glSurfaceView,
            new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        );
    }

    @Override
    protected void onPause() {
        if (glSurfaceView != null) {
            glSurfaceView.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.onResume();
        }
    }

    private final class ConvergenceRenderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(GL10 unused, EGLConfig config) {
            nativeOnSurfaceCreated(getAssets());
        }

        @Override
        public void onSurfaceChanged(GL10 unused, int width, int height) {
            nativeOnSurfaceChanged(width, height);
        }

        @Override
        public void onDrawFrame(GL10 unused) {
            nativeOnDrawFrame();
        }
    }
}
