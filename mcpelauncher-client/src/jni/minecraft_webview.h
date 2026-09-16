#pragma once
#include <fake-jni/fake-jni.h>

class MinecraftWebview : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("com/mojang/minecraftpe/Webview/MinecraftWebview")

    MinecraftWebview(FakeJni::JInt id);

    void setUrl(std::shared_ptr<FakeJni::JString> url);
    void setRect(FakeJni::JFloat x, FakeJni::JFloat y, FakeJni::JFloat w, FakeJni::JFloat h);
    void setShowView(FakeJni::JBoolean show);
    void setPropagatedAlpha(FakeJni::JFloat alpha);
    void sendToWebView(std::shared_ptr<FakeJni::JString> msg);
    void teardown();

    void sendToHost(std::shared_ptr<FakeJni::JString> a, std::shared_ptr<FakeJni::JString> b, std::shared_ptr<FakeJni::JString> c);
    void onWebError(FakeJni::JInt code, std::shared_ptr<FakeJni::JString> msg);

private:
    FakeJni::JInt mId = 0;
};
