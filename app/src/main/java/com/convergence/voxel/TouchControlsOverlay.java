package com.convergence.voxel;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

/** Transparent touch layer that displays the floating movement joystick. */
final class TouchControlsOverlay extends View {
    private final VoxelGLSurfaceView surfaceView;
    private final Paint basePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ringPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint knobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint hintPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    TouchControlsOverlay(Context context, VoxelGLSurfaceView surfaceView) {
        super(context);
        this.surfaceView = surfaceView;

        setLayerType(View.LAYER_TYPE_SOFTWARE, null);
        setClickable(true);

        basePaint.setColor(Color.argb(54, 220, 235, 245));
        basePaint.setStyle(Paint.Style.FILL);

        ringPaint.setColor(Color.argb(150, 220, 235, 245));
        ringPaint.setStyle(Paint.Style.STROKE);
        ringPaint.setStrokeWidth(dp(2.0f));

        knobPaint.setColor(Color.argb(180, 75, 180, 155));
        knobPaint.setStyle(Paint.Style.FILL);

        hintPaint.setColor(Color.argb(125, 235, 242, 248));
        hintPaint.setTextSize(dp(11.0f));
        hintPaint.setTextAlign(Paint.Align.CENTER);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        final float width = getWidth();
        final float height = getHeight();

        hintPaint.setAlpha(115);
        canvas.drawText("MOVE", width * 0.22f, height - dp(30.0f), hintPaint);
        canvas.drawText("LOOK", width * 0.78f, height - dp(30.0f), hintPaint);

        if (!surfaceView.isJoystickActive()) {
            return;
        }

        final float centerX = surfaceView.getJoystickCenterX();
        final float centerY = surfaceView.getJoystickCenterY();
        final float knobX = surfaceView.getJoystickKnobX();
        final float knobY = surfaceView.getJoystickKnobY();
        final float radius = surfaceView.getJoystickRadiusPx();

        canvas.drawCircle(centerX, centerY, radius, basePaint);
        canvas.drawCircle(centerX, centerY, radius, ringPaint);
        canvas.drawCircle(knobX, knobY, radius * 0.34f, knobPaint);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        surfaceView.handleTouchInput(event);
        invalidate();
        return true;
    }

    void cancelInput() {
        surfaceView.resetTouchInput();
        invalidate();
    }

    private float dp(float value) {
        return value * getResources().getDisplayMetrics().density;
    }
}
