#pragma once
#include <fake-jni/fake-jni.h>

class System : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("java/lang/System")

    static std::shared_ptr<FakeJni::JString> getProperty(std::shared_ptr<FakeJni::JString> key);
    static std::shared_ptr<FakeJni::JString> getProperty2(std::shared_ptr<FakeJni::JString> key, std::shared_ptr<FakeJni::JString> def);
    static FakeJni::JLong currentTimeMillis();
    static FakeJni::JLong nanoTime();
};

class AndroidBuild : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("android/os/Build")

    static std::shared_ptr<FakeJni::JString> MODEL;
    static std::shared_ptr<FakeJni::JString> MANUFACTURER;
    static std::shared_ptr<FakeJni::JString> BRAND;
    static std::shared_ptr<FakeJni::JString> DEVICE;
    static std::shared_ptr<FakeJni::JString> DISPLAY;
    static std::shared_ptr<FakeJni::JString> ID;
    static std::shared_ptr<FakeJni::JString> PRODUCT;
    static std::shared_ptr<FakeJni::JString> HARDWARE;
    static std::shared_ptr<FakeJni::JString> USER;
    static std::shared_ptr<FakeJni::JString> TYPE;
};
