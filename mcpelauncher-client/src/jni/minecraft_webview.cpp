#include "minecraft_webview.h"
#include <log.h>

MinecraftWebview::MinecraftWebview(FakeJni::JInt id) : mId(id) {
    Log::info("MinecraftWebview", "Created MinecraftWebview id=%d", (int)id);
}

void MinecraftWebview::setUrl(std::shared_ptr<FakeJni::JString> url) {
    Log::info("MinecraftWebview", "setUrl id=%d url=%s", (int)mId, url ? url->asStdString().c_str() : "(null)");
}

void MinecraftWebview::setRect(FakeJni::JFloat x, FakeJni::JFloat y, FakeJni::JFloat w, FakeJni::JFloat h) {
    Log::info("MinecraftWebview", "setRect id=%d x=%f y=%f w=%f h=%f", (int)mId, (float)x, (float)y, (float)w, (float)h);
}

void MinecraftWebview::setShowView(FakeJni::JBoolean show) {
    Log::info("MinecraftWebview", "setShowView id=%d show=%d", (int)mId, (int)show);
}

void MinecraftWebview::setPropagatedAlpha(FakeJni::JFloat alpha) {
    Log::info("MinecraftWebview", "setPropagatedAlpha id=%d alpha=%f", (int)mId, (float)alpha);
}

void MinecraftWebview::sendToWebView(std::shared_ptr<FakeJni::JString> msg) {
    Log::info("MinecraftWebview", "sendToWebView id=%d msg=%.200s", (int)mId, msg ? msg->asStdString().c_str() : "(null)");
}

void MinecraftWebview::teardown() {
    Log::info("MinecraftWebview", "teardown id=%d", (int)mId);
}

void MinecraftWebview::sendToHost(std::shared_ptr<FakeJni::JString> a, std::shared_ptr<FakeJni::JString> b, std::shared_ptr<FakeJni::JString> c) {
    Log::info("MinecraftWebview", "sendToHost id=%d (JS -> host)", (int)mId);
    try {
        FakeJni::LocalFrame frame;
        auto method = getClass().getMethod("(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", "nativeSendToHost");
        if(method) {
            auto sa = frame.getJniEnv().createLocalReference(a ? a : std::make_shared<FakeJni::JString>(""));
            auto sb = frame.getJniEnv().createLocalReference(b ? b : std::make_shared<FakeJni::JString>(""));
            auto sc = frame.getJniEnv().createLocalReference(c ? c : std::make_shared<FakeJni::JString>(""));
            method->invoke(frame.getJniEnv(), this, mId, sa, sb, sc);
        } else {
            Log::warn("MinecraftWebview", "nativeSendToHost not found");
        }
    } catch(std::exception &e) {
        Log::error("MinecraftWebview", "nativeSendToHost failed: %s", e.what());
    }
}

void MinecraftWebview::onWebError(FakeJni::JInt code, std::shared_ptr<FakeJni::JString> msg) {
    Log::info("MinecraftWebview", "onWebError id=%d code=%d msg=%s", (int)mId, (int)code, msg ? msg->asStdString().c_str() : "(null)");
}
