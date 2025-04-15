plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

val autoVersion = (((System.currentTimeMillis() / 1000) - 1451606400) / 10).toInt()

android {
    namespace = "com.sudachi_emu.sudachi"

    buildToolsVersion = "35.0.0"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    buildFeatures {
        buildConfig = true
        compose = true
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        applicationId = "com.sudachi_emu.sudachi"

        minSdk = 33
        targetSdk = 35
        versionCode = if (System.getenv("AUTO_VERSIONED") == "true") {
            autoVersion
        } else {
            1
        }
        versionName = getGitVersion()

        buildConfigField("String", "GIT_HASH", "\"${getGitHash()}\"")
        buildConfigField("String", "BRANCH", "\"${getBranch()}\"")

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"


        externalNativeBuild {
            cmake {
                arguments(
                    "-DENABLE_QT=0",
                    "-DENABLE_SDL3=0",
                    "-DENABLE_WEB_SERVICE=0",
                    "-DBUNDLE_SPEEX=ON",
                    "-DANDROID_ARM_NEON=true",
                    "-DSUDACHI_USE_BUNDLED_VCPKG=ON",
                    "-DSUDACHI_USE_BUNDLED_FFMPEG=ON",
                    "-DSUDACHI_ENABLE_LTO=ON",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"
                )

                abiFilters("arm64-v8a", "x86_64")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    packaging {
        jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)
}

fun runGitCommand(command: List<String>): String {
    return try {
        ProcessBuilder(command)
            .directory(project.rootDir)
            .redirectOutput(ProcessBuilder.Redirect.PIPE)
            .redirectError(ProcessBuilder.Redirect.PIPE)
            .start().inputStream.bufferedReader().use { it.readText() }
            .trim()
    } catch (e: Exception) {
        logger.error("Cannot find git")
        ""
    }
}

fun getGitVersion(): String {
    val gitVersion = runGitCommand(
        listOf(
            "git",
            "describe",
            "--always",
            "--long"
        )
    ).replace(Regex("(-0)?-[^-]+$"), "")
    val versionName = if (System.getenv("GITHUB_ACTIONS") != null) {
        System.getenv("GIT_TAG_NAME") ?: gitVersion
    } else {
        gitVersion
    }
    return versionName.ifEmpty { "0.0" }
}

fun getGitHash(): String =
    runGitCommand(listOf("git", "rev-parse", "--short", "HEAD")).ifEmpty { "dummy-hash" }

fun getBranch(): String =
    runGitCommand(listOf("git", "rev-parse", "--abbrev-ref", "HEAD")).ifEmpty { "dummy-hash" }