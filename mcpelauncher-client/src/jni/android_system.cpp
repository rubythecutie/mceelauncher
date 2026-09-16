#include "android_system.h"
#include <log.h>
#include <sys/utsname.h>
#include <chrono>

std::shared_ptr<FakeJni::JString> AndroidBuild::MODEL = std::make_shared<FakeJni::JString>("Linux");
std::shared_ptr<FakeJni::JString> AndroidBuild::MANUFACTURER = std::make_shared<FakeJni::JString>("unknown");
std::shared_ptr<FakeJni::JString> AndroidBuild::BRAND = std::make_shared<FakeJni::JString>("unknown");
std::shared_ptr<FakeJni::JString> AndroidBuild::DEVICE = std::make_shared<FakeJni::JString>("linux");
std::shared_ptr<FakeJni::JString> AndroidBuild::DISPLAY = std::make_shared<FakeJni::JString>("SQ3A.220705.004");
std::shared_ptr<FakeJni::JString> AndroidBuild::ID = std::make_shared<FakeJni::JString>("SQ3A.220705.004");
std::shared_ptr<FakeJni::JString> AndroidBuild::PRODUCT = std::make_shared<FakeJni::JString>("linux");
std::shared_ptr<FakeJni::JString> AndroidBuild::HARDWARE = std::make_shared<FakeJni::JString>("linux");
std::shared_ptr<FakeJni::JString> AndroidBuild::USER = std::make_shared<FakeJni::JString>("linux");
std::shared_ptr<FakeJni::JString> AndroidBuild::TYPE = std::make_shared<FakeJni::JString>("user");

static std::string getUnameRelease() {
    struct utsname u;
    if(uname(&u) == 0 && u.release[0] != '\0')
        return u.release;
    return "12";
}

std::shared_ptr<FakeJni::JString> System::getProperty(std::shared_ptr<FakeJni::JString> key) {
    if(!key)
        return nullptr;
    std::string k = key->asStdString();
    if(k == "os.version") {
        return std::make_shared<FakeJni::JString>("12");
    } else if(k == "os.name") {
        return std::make_shared<FakeJni::JString>("Linux");
    } else if(k == "os.arch") {
#if defined(__x86_64__)
        return std::make_shared<FakeJni::JString>("amd64");
#elif defined(__i386__)
        return std::make_shared<FakeJni::JString>("x86");
#elif defined(__aarch64__)
        return std::make_shared<FakeJni::JString>("aarch64");
#else
        return std::make_shared<FakeJni::JString>("unknown");
#endif
    } else if(k == "java.version") {
        return std::make_shared<FakeJni::JString>("17");
    } else if(k == "line.separator") {
        return std::make_shared<FakeJni::JString>("\n");
    } else if(k == "file.separator") {
        return std::make_shared<FakeJni::JString>("/");
    }
    Log::info("System", "getProperty(%s) -> null (stub)", k.c_str());
    return nullptr;
}

std::shared_ptr<FakeJni::JString> System::getProperty2(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def) {
    auto v = getProperty(key);
    return v ? v : def;
}

FakeJni::JLong System::currentTimeMillis() {
    auto now = std::chrono::system_clock::now();
    return (FakeJni::JLong)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

FakeJni::JLong System::nanoTime() {
    auto now = std::chrono::steady_clock::now();
    return (FakeJni::JLong)std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}
