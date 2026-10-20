# Keep the JNI bridge: its methods are called from C++ by name.
-keep class np.sunai.audio.NativeAudio { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
