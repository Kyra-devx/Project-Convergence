package com.convergence.voxel;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

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
        // Added last so these controls receive taps above the transparent
        // touch overlay while the remaining screen continues to drive look.
        root.addView(createActionButtons());

        setContentView(root);
    }

    private LinearLayout createActionButtons() {
        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.VERTICAL);
        actions.setGravity(Gravity.CENTER_HORIZONTAL);

        Button breakButton = new Button(this);
        breakButton.setText("BREAK");
        breakButton.setContentDescription("Break the targeted block");
        breakButton.setOnClickListener(
            view -> glSurfaceView.requestAction(VoxelRenderer.ACTION_BREAK));
        LinearLayout.LayoutParams breakParams =
            new LinearLayout.LayoutParams(dp(112), dp(48));
        breakParams.bottomMargin = dp(8);
        actions.addView(breakButton, breakParams);

        Button placeButton = new Button(this);
        placeButton.setText("PLACE");
        placeButton.setContentDescription("Place a block against the targeted face");
        placeButton.setOnClickListener(
            view -> glSurfaceView.requestAction(VoxelRenderer.ACTION_PLACE));
        actions.addView(
            placeButton,
            new LinearLayout.LayoutParams(dp(112), dp(48)));

        FrameLayout.LayoutParams position = new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM | Gravity.END);
        position.setMargins(0, 0, dp(16), dp(24));
        actions.setLayoutParams(position);
        return actions;
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
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
