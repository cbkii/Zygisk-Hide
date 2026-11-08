import java.io.ByteArrayOutputStream

plugins {
    id("com.android.library") version "8.8.1" apply false
}

val commitHash: String by extra {
    val result = providers.exec {
        commandLine("git", "rev-parse", "--verify", "--short", "HEAD")
    }
    result.standardOutput.asText.get().trim()
}

val moduleId by extra("zygisk-hide")
val moduleName by extra("Zygisk Hide")
val verName by extra("v3.2.0-alpha3")
val verCode by extra(322)
