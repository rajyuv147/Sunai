plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "np.sunai"
    compileSdk = 35

    defaultConfig {
        applicationId = "np.sunai"
        minSdk = 27            // Android 8.1: first version with the fast (MMAP) audio path
        targetSdk = 35
        versionCode = 5
        versionName = "0.2.0"

        ndk {
            // Only the processors real phones use. Keeps the APK small.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-fno-exceptions", "-fno-rtti")
                arguments += listOf("-DANDROID_STL=c++_static", "-DANDROID_ARM_NEON=TRUE")
            }
        }
    }

    // Release signing: uses a real keystore when the CI secrets are set,
    // otherwise falls back to the debug key so a test APK can still be shared.
    val keystorePath = System.getenv("SUNAI_KEYSTORE")
    signingConfigs {
        if (keystorePath != null) {
            create("release") {
                storeFile = file(keystorePath)
                storePassword = System.getenv("SUNAI_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("SUNAI_KEY_ALIAS")
                keyPassword = System.getenv("SUNAI_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = if (keystorePath != null) signingConfigs.getByName("release")
                            else signingConfigs.getByName("debug")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        buildConfig = false
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        jniLibs { useLegacyPackaging = false }
    }
}

// No dependencies at all: no AndroidX, no analytics, no network libraries.
// Oboe and RNNoise are compiled from source in src/main/cpp/third_party.
