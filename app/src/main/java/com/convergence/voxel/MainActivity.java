package com.convergence.voxel;

import android.app.Activity;
import android.os.Bundle;
import android.widget.FrameLayout;
import android.view.ViewGroup;

/** Android entry point for the Project Convergence first-person prototype. */
public final class MainActivity extends Activity {
    private VoxelGLSurfaceView glSurfaceView;
    private TouchControlsOverlay controlsOverlay;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        glSurfaceView = new VoxelGLSurfaceView(this);
        controlsOverlay = new TouchControlsOverlay(this, glSurfaceView);

        FrameLayout root = new FrameLayout(this);
        root.addView(
            glSurfaceView,
            new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        );
        root.addView(
            controlsOverlay,
            new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        );

        setContentView(root);
    }

    @Override
    protected void onPause() {
        if (controlsOverlay != null) {
            controlsOverlay.cancelInput();
        }
        if (glSurfaceView != null) {
            glSurfaceView.resetFrameClock();
            glSurfaceView.onPause();
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) {
            glSurfaceView.resetFrameClock();
            glSurfaceView.onResume();
        }
    }
}
