package com.convergence.voxel;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;

/**
 * OpenGL surface and multi-touch input controller.
 * Left half: one floating joystick pointer. Right half: one look pointer.
 */
final class VoxelGLSurfaceView extends GLSurfaceView {
    private static final float JOYSTICK_RADIUS_DP = 72.0f;
    private static final float JOYSTICK_DEAD_ZONE = 0.08f;

    private final VoxelRenderer renderer;
    private final float joystickRadiusPx;

    private int lookPointerId = MotionEvent.INVALID_POINTER_ID;
    private int joystickPointerId = MotionEvent.INVALID_POINTER_ID;
    private float lastLookX;
    private float lastLookY;
    private float joystickCenterX;
    private float joystickCenterY;
    private float joystickKnobX;
    private float joystickKnobY;
    private float joystickForward;
    private float joystickStrafe;

    VoxelGLSurfaceView(Context context) {
        super(context);
        renderer = new VoxelRenderer(context.getApplicationContext().getAssets());
        joystickRadiusPx =
            JOYSTICK_RADIUS_DP * getResources().getDisplayMetrics().density;

        setEGLContextClientVersion(3);
        setPreserveEGLContextOnPause(true);
        setRenderer(renderer);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
        setFocusable(true);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return handleTouchInput(event);
    }

    boolean handleTouchInput(MotionEvent event) {
        final int action = event.getActionMasked();
        final int actionIndex = event.getActionIndex();

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                beginPointer(event, actionIndex);
                return true;
            case MotionEvent.ACTION_MOVE:
                updatePointers(event);
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                endPointer(event.getPointerId(actionIndex));
                return true;
            case MotionEvent.ACTION_CANCEL:
                resetTouchInput();
                return true;
            default:
                return true;
        }
    }

    private void beginPointer(MotionEvent event, int pointerIndex) {
        final float x = event.getX(pointerIndex);
        final float y = event.getY(pointerIndex);
        final int pointerId = event.getPointerId(pointerIndex);
        final float splitX = getWidth() * 0.5f;

        if (x < splitX && joystickPointerId == MotionEvent.INVALID_POINTER_ID) {
            joystickPointerId = pointerId;
            joystickCenterX = x;
            joystickCenterY = y;
            joystickKnobX = x;
            joystickKnobY = y;
            joystickForward = 0.0f;
            joystickStrafe = 0.0f;
            renderer.submitJoystick(0.0f, 0.0f);
        } else if (x >= splitX
                && lookPointerId == MotionEvent.INVALID_POINTER_ID) {
            lookPointerId = pointerId;
            lastLookX = x;
            lastLookY = y;
        }
    }

    private void updatePointers(MotionEvent event) {
        for (int index = 0; index < event.getPointerCount(); index++) {
            final int pointerId = event.getPointerId(index);
            final float x = event.getX(index);
            final float y = event.getY(index);

            if (pointerId == lookPointerId) {
                renderer.submitLook(x - lastLookX, y - lastLookY);
                lastLookX = x;
                lastLookY = y;
            } else if (pointerId == joystickPointerId) {
                updateJoystick(x, y);
            }
        }
    }

    private void updateJoystick(float x, float y) {
        float offsetX = (x - joystickCenterX) / joystickRadiusPx;
        float offsetY = (y - joystickCenterY) / joystickRadiusPx;
        final float length = (float) Math.hypot(offsetX, offsetY);

        if (length > 1.0f) {
            offsetX /= length;
            offsetY /= length;
        }

        joystickKnobX = joystickCenterX + offsetX * joystickRadiusPx;
        joystickKnobY = joystickCenterY + offsetY * joystickRadiusPx;

        if (length < JOYSTICK_DEAD_ZONE) {
            joystickForward = 0.0f;
            joystickStrafe = 0.0f;
        } else {
            // The offsets are already clamped to the unit circle.
            joystickForward = -offsetY;
            joystickStrafe = offsetX;
        }

        renderer.submitJoystick(joystickForward, joystickStrafe);
    }

    private void endPointer(int pointerId) {
        if (pointerId == lookPointerId) {
            lookPointerId = MotionEvent.INVALID_POINTER_ID;
        }

        if (pointerId == joystickPointerId) {
            joystickPointerId = MotionEvent.INVALID_POINTER_ID;
            joystickForward = 0.0f;
            joystickStrafe = 0.0f;
            joystickKnobX = joystickCenterX;
            joystickKnobY = joystickCenterY;
            renderer.submitJoystick(0.0f, 0.0f);
        }
    }

    void resetTouchInput() {
        lookPointerId = MotionEvent.INVALID_POINTER_ID;
        joystickPointerId = MotionEvent.INVALID_POINTER_ID;
        joystickForward = 0.0f;
        joystickStrafe = 0.0f;
        joystickKnobX = joystickCenterX;
        joystickKnobY = joystickCenterY;
        renderer.submitJoystick(0.0f, 0.0f);
    }

    void resetFrameClock() {
        renderer.resetFrameClock();
    }

    boolean isJoystickActive() {
        return joystickPointerId != MotionEvent.INVALID_POINTER_ID;
    }

    float getJoystickCenterX() {
        return joystickCenterX;
    }

    float getJoystickCenterY() {
        return joystickCenterY;
    }

    float getJoystickKnobX() {
        return joystickKnobX;
    }

    float getJoystickKnobY() {
        return joystickKnobY;
    }

    float getJoystickRadiusPx() {
        return joystickRadiusPx;
    }
}
