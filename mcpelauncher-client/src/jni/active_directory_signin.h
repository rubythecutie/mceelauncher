#pragma once
#include <fake-jni/fake-jni.h>
#include "main_activity.h"

class ActiveDirectorySignIn : public FakeJni::JObject {
public:
    DEFINE_CLASS_NAME("com/mojang/minecraftpe/ActiveDirectorySignIn")

    ActiveDirectorySignIn();

    static std::shared_ptr<ActiveDirectorySignIn> createActiveDirectorySignIn();

    void authenticate(std::shared_ptr<FakeJni::JString> userHint, FakeJni::JInt promptBehavior);
    void silentSignin(std::shared_ptr<FakeJni::JString> userHint, std::shared_ptr<FakeJni::JString> target);
    void signOut();
    void clearCookies();

    std::shared_ptr<FakeJni::JString> getAccessToken();
    FakeJni::JLong getExpiresOn();
    std::shared_ptr<FakeJni::JString> getUserHint();
    FakeJni::JBoolean getCancelled();
    FakeJni::JBoolean hasError();
    std::shared_ptr<FakeJni::JString> getErrorString();
    FakeJni::JInt getErrorStatus();
    FakeJni::JInt getErrorSubStatus();
    std::shared_ptr<FakeJni::JString> getSignOutError();

    void onActivityResult(FakeJni::JInt requestCode, FakeJni::JInt resultCode, std::shared_ptr<FakeJni::JObject> data);
    void onDestroy();
    void onResume();
    void onStop();

private:
    std::string mAccessToken;
    FakeJni::JLong mExpiresOn = -1;
    std::string mUserHint;
    bool mCancelled = false;
    bool mHasError = false;
    std::string mErrorString;
    FakeJni::JInt mErrorStatus = -1;
    FakeJni::JInt mErrorSubStatus = -1;
    std::string mSignOutError;

    void notifyDataChanged();
};
