#include <GLES3/gl3.h>
#include <jni.h>

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnSurfaceCreated(
    JNIEnv* /* env */,
    jclass /* clazz */
) {
    glClearColor(0.035F, 0.055F, 0.085F, 1.0F);
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnSurfaceChanged(
    JNIEnv* /* env */,
    jclass /* clazz */,
    jint width,
    jint height
) {
    if (width > 0 && height > 0) {
        glViewport(0, 0, width, height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_convergence_voxel_MainActivity_nativeOnDrawFrame(
    JNIEnv* /* env */,
    jclass /* clazz */
) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
