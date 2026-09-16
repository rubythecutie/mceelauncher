#include "active_directory_signin.h"
#include "../xal_webview_qt.h"
#include "../util.h"
#include <log.h>
#include <mcpelauncher/path_helper.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#ifndef NO_OPENSSL
#include <openssl/sha.h>
#endif
#include <random>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <map>

namespace {
const char* EDU_CLIENT_ID = "b36b1432-1a1c-4c82-9b76-24de1cab42f2";
const char* EDU_AUTHORIZE_URL = "https://login.microsoftonline.com/common/oauth2/v2.0/authorize";
const char* EDU_TOKEN_URL = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
const char* EDU_RESOURCE = "https://meeservices.minecraft.net";
const char* EDU_SCOPE = "https://meeservices.minecraft.net/.default openid profile offline_access";

std::string urlEncode(const std::string& s) {
    std::string out;
    char hex[] = "0123456789ABCDEF";
    for(unsigned char c : s) {
        if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string urlDecode(const std::string& s) {
    std::string out;
    for(size_t i = 0; i < s.size(); ++i) {
        if(s[i] == '%' && i + 2 < s.size()) {
            auto hv = [](char h) -> int {
                if(h >= '0' && h <= '9') return h - '0';
                if(h >= 'a' && h <= 'f') return h - 'a' + 10;
                if(h >= 'A' && h <= 'F') return h - 'A' + 10;
                return -1;
            };
            int v1 = hv(s[i+1]), v2 = hv(s[i+2]);
            if(v1 >= 0 && v2 >= 0) {
                out += (char)(v1 * 16 + v2);
                i += 2;
                continue;
            }
        } else if(s[i] == '+') {
            out += ' ';
            continue;
        }
        out += s[i];
    }
    return out;
}

std::string base64UrlEncode(const uint8_t* data, size_t len) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    size_t i = 0;
    while(i < len) {
        uint32_t a = i < len ? data[i++] : 0;
        uint32_t b = i < len ? data[i++] : 0;
        uint32_t c = i < len ? data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        out += b64[(triple >> 18) & 63];
        out += b64[(triple >> 12) & 63];
        out += b64[(triple >> 6) & 63];
        out += b64[triple & 63];
    }
    size_t mod = len % 3;
    if(mod == 1)
        out = out.substr(0, out.size() - 2);
    else if(mod == 2)
        out = out.substr(0, out.size() - 1);
    return out;
}

std::string base64UrlDecode(const std::string& s) {
    std::string b64 = s;
    for(char& c : b64) {
        if(c == '-') c = '+';
        else if(c == '_') c = '/';
    }
    while(b64.size() % 4)
        b64 += '=';
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,0,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::string out;
    for(size_t i = 0; i + 3 < b64.size(); i += 4) {
        int a = T[(unsigned char)b64[i]], b = T[(unsigned char)b64[i+1]];
        int c = T[(unsigned char)b64[i+2]], d = T[(unsigned char)b64[i+3]];
        if(a < 0 || b < 0) break;
        out += (char)((a << 2) | (b >> 4));
        if(c >= 0) out += (char)(((b & 15) << 4) | (c >> 2));
        if(d >= 0) out += (char)(((c & 3) << 6) | d);
    }
    return out;
}

std::string randomBase64Url(size_t bytes) {
    std::vector<uint8_t> buf(bytes);
    std::random_device rd;
    for(auto& b : buf)
        b = (uint8_t)rd();
    return base64UrlEncode(buf.data(), buf.size());
}

std::string sha256Base64Url(const std::string& s) {
#ifdef NO_OPENSSL
    return randomBase64Url(32);
#else
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256((const uint8_t*)s.data(), s.size(), hash);
    return base64UrlEncode(hash, sizeof(hash));
#endif
}

std::map<std::string, std::string> parseQuery(const std::string& query) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while(i < query.size()) {
        auto amp = query.find('&', i);
        std::string pair = query.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        auto eq = pair.find('=');
        if(eq != std::string::npos)
            out[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        else if(!pair.empty())
            out[urlDecode(pair)] = "";
        if(amp == std::string::npos)
            break;
        i = amp + 1;
    }
    return out;
}

size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    ((std::string*)userdata)->append(ptr, size * nmemb);
    return size * nmemb;
}

bool httpPostForm(const std::string& url, const std::string& body, std::string& respOut, std::string& errOut) {
    CURL* curl = curl_easy_init();
    if(!curl) {
        errOut = "curl_easy_init failed";
        return false;
    }
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respOut);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    if(rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if(rc != CURLE_OK) {
        errOut = std::string("HTTP request failed: ") + curl_easy_strerror(rc) + ". Check your internet connection.";
        return false;
    }
    if(httpCode < 200 || httpCode >= 300) {
        errOut = "HTTP " + std::to_string(httpCode) + ": " + respOut;
        return false;
    }
    return true;
}

std::string tokenCachePath() {
    return PathHelper::getPrimaryDataDirectory() + "edu_auth.json";
}

std::string usernameFromIdToken(const std::string& idToken) {
    try {
        auto dot1 = idToken.find('.');
        if(dot1 == std::string::npos) return "";
        auto dot2 = idToken.find('.', dot1 + 1);
        if(dot2 == std::string::npos) return "";
        std::string payload = base64UrlDecode(idToken.substr(dot1 + 1, dot2 - dot1 - 1));
        auto j = nlohmann::json::parse(payload);
        for(auto key : {"preferred_username", "email", "unique_name", "upn"}) {
            if(j.contains(key) && j[key].is_string())
                return j[key].get<std::string>();
        }
    } catch(...) {
    }
    return "";
}

std::string scopeForResource(const std::string& resource) {
    if(resource.empty())
        return EDU_SCOPE;
    std::string r = resource;
    if(r.back() == '/')
        return r + ".default openid profile offline_access";
    return r + "/.default openid profile offline_access";
}

}

ActiveDirectorySignIn::ActiveDirectorySignIn() {
    Log::info("ActiveDirectorySignIn", "Created ActiveDirectorySignIn instance");
    try {
        std::ifstream f(tokenCachePath());
        if(f) {
            auto j = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
            if(j.contains("user_hint") && j["user_hint"].is_string())
                mUserHint = j["user_hint"].get<std::string>();
        }
    } catch(...) {
    }
}

std::shared_ptr<ActiveDirectorySignIn> ActiveDirectorySignIn::createActiveDirectorySignIn() {
    Log::info("ActiveDirectorySignIn", "createActiveDirectorySignIn called");
    return std::make_shared<ActiveDirectorySignIn>();
}

void ActiveDirectorySignIn::authenticate(std::shared_ptr<FakeJni::JString> userHint, FakeJni::JInt promptBehavior) {
    std::string hint = userHint ? userHint->asStdString() : "";
    Log::info("ActiveDirectorySignIn", "authenticate called: userHint=%s promptBehavior=%d (webview flow)", hint.c_str(), (int)promptBehavior);
    mUserHint = hint;
    mCancelled = false;
    mHasError = false;
    mErrorString.clear();
    mErrorStatus = 0;
    mErrorSubStatus = 0;
    mAccessToken.clear();
    mExpiresOn = -1;

    {
        std::string refreshToken, cachedHint;
        try {
            std::ifstream f(tokenCachePath());
            if(f) {
                auto j = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
                refreshToken = j.value("refresh_token", "");
                cachedHint = j.value("user_hint", "");
            }
        } catch(...) {
        }
        if(!refreshToken.empty() && (hint.empty() || cachedHint.empty() || hint == cachedHint)) {
            std::string body = "client_id=" + urlEncode(EDU_CLIENT_ID)
                + "&refresh_token=" + urlEncode(refreshToken)
                + "&grant_type=refresh_token"
                + "&scope=" + urlEncode(EDU_SCOPE);
            std::string resp, err;
            if(httpPostForm(EDU_TOKEN_URL, body, resp, err)) {
                try {
                    auto j = nlohmann::json::parse(resp);
                    if(!j.contains("error") && j.contains("access_token")) {
                        mAccessToken = j.value("access_token", "");
                        long expiresIn = 3600;
                        if(j.contains("expires_in")) {
                            if(j["expires_in"].is_number())
                                expiresIn = j["expires_in"].get<long>();
                            else if(j["expires_in"].is_string())
                                expiresIn = std::stol(j["expires_in"].get<std::string>());
                        }
                        mExpiresOn = (FakeJni::JLong)time(nullptr) + expiresIn;
                        std::string newRefresh = j.value("refresh_token", "");
                        if(!newRefresh.empty())
                            refreshToken = newRefresh;
                        std::string idToken = j.value("id_token", "");
                        std::string username = usernameFromIdToken(idToken);
                        if(!username.empty())
                            mUserHint = username;
                        else if(!cachedHint.empty())
                            mUserHint = cachedHint;
                        try {
                            nlohmann::json cache;
                            cache["refresh_token"] = refreshToken;
                            cache["user_hint"] = mUserHint;
                            cache["scope"] = EDU_SCOPE;
                            std::string path = tokenCachePath();
                            std::ofstream f(path, std::ios::trunc);
                            if(f) {
                                f << cache.dump();
                                f.close();
                                chmod(path.c_str(), 0600);
                            }
                        } catch(...) {
                        }
                        if(!mAccessToken.empty()) {
                            Log::info("ActiveDirectorySignIn", "authenticate: silent refresh succeeded, user=%s (no window)", mUserHint.c_str());
                            notifyDataChanged();
                            return;
                        }
                    }
                } catch(...) {
                }
            }
            Log::info("ActiveDirectorySignIn", "authenticate: silent refresh failed, falling back to webview");
        }
    }

    const std::string redirectUri = "https://login.microsoftonline.com/common/oauth2/nativeclient";
    std::string verifier = randomBase64Url(32);
    std::string challenge = sha256Base64Url(verifier);
    std::string state = randomBase64Url(16);
    std::string scope = EDU_SCOPE;

    std::string authUrl = std::string(EDU_AUTHORIZE_URL)
        + "?client_id=" + urlEncode(EDU_CLIENT_ID)
        + "&response_type=code"
        + "&redirect_uri=" + urlEncode(redirectUri)
        + "&scope=" + urlEncode(scope)
        + "&state=" + urlEncode(state)
        + "&code_challenge=" + urlEncode(challenge)
        + "&code_challenge_method=S256"
        + "&response_mode=query";
    if(promptBehavior == 0)
        authUrl += "&prompt=select_account";
    else
        authUrl += "&prompt=login";
    if(!hint.empty())
        authUrl += "&login_hint=" + urlEncode(hint);

    Log::info("ActiveDirectorySignIn", "Opening login window (webview): %s", authUrl.c_str());
    std::string finalUrl;
    try {
        XalWebViewQt webview;
        finalUrl = webview.show(authUrl, redirectUri);
        trim(finalUrl);
        auto nl = finalUrl.find_last_of('\n');
        if(nl != std::string::npos) {
            std::string last = finalUrl.substr(nl + 1);
            trim(last);
            if(!last.empty())
                finalUrl = last;
        }
    } catch(std::exception& e) {
        Log::warn("ActiveDirectorySignIn", "Webview failed: %s", e.what());
    }
    Log::info("ActiveDirectorySignIn", "Webview returned url=%.200s", finalUrl.c_str());
    if(finalUrl.empty() || finalUrl.rfind(redirectUri, 0) != 0) {
        auto pos = finalUrl.find(redirectUri);
        if(pos != std::string::npos) {
            finalUrl = finalUrl.substr(pos, finalUrl.find_first_of(" \t\r\n", pos) - pos);
        } else {
            Log::info("ActiveDirectorySignIn", "Login cancelled (webview closed without redirect)");
            mCancelled = true;
            notifyDataChanged();
            return;
        }
    }
    std::string q;
    {
        auto qp = finalUrl.find('?');
        auto hp = finalUrl.find('#');
        if(qp != std::string::npos)
            q = finalUrl.substr(qp + 1, (hp == std::string::npos ? std::string::npos : hp - qp - 1));
        if(hp != std::string::npos) {
            std::string frag = finalUrl.substr(hp + 1);
            if(!q.empty()) q += "&" + frag;
            else q = frag;
        }
    }
    auto params = parseQuery(q);
    if(params.count("error")) {
        Log::warn("ActiveDirectorySignIn", "OAuth error: %s", params["error"].c_str());
        mCancelled = true;
        notifyDataChanged();
        return;
    }
    std::string code = params.count("code") ? params["code"] : "";
    std::string gotState = params.count("state") ? params["state"] : "";
    if(code.empty()) {
        Log::error("ActiveDirectorySignIn", "No auth code in redirect URL");
        mHasError = true;
        mErrorString = "Login did not return an authorization code. Check your internet connection.";
        notifyDataChanged();
        return;
    }
    if(!gotState.empty() && gotState != state) {
        Log::warn("ActiveDirectorySignIn", "OAuth state mismatch");
    }
    std::string body = "client_id=" + urlEncode(EDU_CLIENT_ID)
        + "&code=" + urlEncode(code)
        + "&redirect_uri=" + urlEncode(redirectUri)
        + "&grant_type=authorization_code"
        + "&code_verifier=" + urlEncode(verifier)
        + "&scope=" + urlEncode(scope);
    std::string resp, err;
    if(!httpPostForm(EDU_TOKEN_URL, body, resp, err)) {
        Log::error("ActiveDirectorySignIn", "Token exchange failed: %s", err.c_str());
        mHasError = true;
        mErrorString = err;
        notifyDataChanged();
        return;
    }
    try {
        auto j = nlohmann::json::parse(resp);
        if(j.contains("error")) {
            mHasError = true;
            mErrorString = j.value("error", "token_error") + ": " + j.value("error_description", "");
            notifyDataChanged();
            return;
        }
        mAccessToken = j.value("access_token", "");
        long expiresIn = 3600;
        if(j.contains("expires_in")) {
            if(j["expires_in"].is_number())
                expiresIn = j["expires_in"].get<long>();
            else if(j["expires_in"].is_string())
                expiresIn = std::stol(j["expires_in"].get<std::string>());
        }
        mExpiresOn = (FakeJni::JLong)time(nullptr) + expiresIn;
        std::string refreshToken = j.value("refresh_token", "");
        std::string idToken = j.value("id_token", "");
        std::string username = usernameFromIdToken(idToken);
        if(!username.empty())
            mUserHint = username;
        else if(!hint.empty())
            mUserHint = hint;
        try {
            nlohmann::json cache;
            cache["refresh_token"] = refreshToken;
            cache["user_hint"] = mUserHint;
            cache["scope"] = scope;
            std::string path = tokenCachePath();
            std::string dir = path.substr(0, path.find_last_of('/'));
            mkdir(dir.c_str(), 0700);
            std::ofstream f(path, std::ios::trunc);
            if(f) {
                f << cache.dump();
                f.close();
                chmod(path.c_str(), 0600);
            }
        } catch(...) {
        }
        if(mAccessToken.empty()) {
            mHasError = true;
            mErrorString = "Token response contained no access_token. Check your internet connection.";
        } else {
            Log::info("ActiveDirectorySignIn", "Login succeeded (webview), user=%s expiresOn=%lld", mUserHint.c_str(), (long long)mExpiresOn);
        }
    } catch(std::exception& e) {
        mHasError = true;
        mErrorString = std::string("Failed to parse token response: ") + e.what() + ". Check your internet connection.";
    }
    notifyDataChanged();
}

void ActiveDirectorySignIn::silentSignin(std::shared_ptr<FakeJni::JString> userHint, std::shared_ptr<FakeJni::JString> target) {
    std::string hint = userHint ? userHint->asStdString() : "";
    std::string t = target ? target->asStdString() : "";
    Log::info("ActiveDirectorySignIn", "silentSignin called: userHint=%s target=%s", hint.c_str(), t.c_str());
    if(!hint.empty())
        mUserHint = hint;
    mCancelled = false;
    mHasError = false;
    mErrorString.clear();

    std::string refreshToken, scope = EDU_SCOPE;
    try {
        std::ifstream f(tokenCachePath());
        if(f) {
            auto j = nlohmann::json::parse(std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()));
            refreshToken = j.value("refresh_token", "");
            if(mUserHint.empty() && j.contains("user_hint"))
                mUserHint = j.value("user_hint", "");
        }
    } catch(...) {
    }
    if(refreshToken.empty()) {
        Log::info("ActiveDirectorySignIn", "silentSignin: no cached refresh token");
        mHasError = true;
        mErrorString = "No cached credentials. Check your internet connection and sign in again.";
        mErrorStatus = -1;
        mErrorSubStatus = -1;
        notifyDataChanged();
        return;
    }
    if(!t.empty())
        scope = scopeForResource(t);
    std::string body = "client_id=" + urlEncode(EDU_CLIENT_ID)
        + "&refresh_token=" + urlEncode(refreshToken)
        + "&grant_type=refresh_token"
        + "&scope=" + urlEncode(scope);
    std::string resp, err;
    if(!httpPostForm(EDU_TOKEN_URL, body, resp, err)) {
        Log::warn("ActiveDirectorySignIn", "silent token refresh failed: %s", err.c_str());
        mHasError = true;
        mErrorString = err;
        notifyDataChanged();
        return;
    }
    try {
        auto j = nlohmann::json::parse(resp);
        if(j.contains("error")) {
            mHasError = true;
            mErrorString = j.value("error", "token_error") + ": " + j.value("error_description", "");
            notifyDataChanged();
            return;
        }
        mAccessToken = j.value("access_token", "");
        long expiresIn = 3600;
        if(j.contains("expires_in")) {
            if(j["expires_in"].is_number())
                expiresIn = j["expires_in"].get<long>();
            else if(j["expires_in"].is_string())
                expiresIn = std::stol(j["expires_in"].get<std::string>());
        }
        mExpiresOn = (FakeJni::JLong)time(nullptr) + expiresIn;
        std::string newRefresh = j.value("refresh_token", "");
        if(!newRefresh.empty())
            refreshToken = newRefresh;
        std::string idToken = j.value("id_token", "");
        std::string username = usernameFromIdToken(idToken);
        if(!username.empty())
            mUserHint = username;
        try {
            nlohmann::json cache;
            cache["refresh_token"] = refreshToken;
            cache["user_hint"] = mUserHint;
            cache["scope"] = EDU_SCOPE;
            std::string path = tokenCachePath();
            std::ofstream f(path, std::ios::trunc);
            if(f) {
                f << cache.dump();
                f.close();
                chmod(path.c_str(), 0600);
            }
        } catch(...) {
        }
        if(mAccessToken.empty()) {
            mHasError = true;
            mErrorString = "Silent token response contained no access_token. Check your internet connection.";
        } else {
            Log::info("ActiveDirectorySignIn", "silentSignin succeeded, user=%s target=%s", mUserHint.c_str(), t.c_str());
        }
    } catch(std::exception& e) {
        mHasError = true;
        mErrorString = std::string("Failed to parse silent token response: ") + e.what();
    }
    notifyDataChanged();
}

void ActiveDirectorySignIn::signOut() {
    Log::info("ActiveDirectorySignIn", "signOut called");
    mAccessToken.clear();
    mExpiresOn = -1;
    unlink(tokenCachePath().c_str());
    try {
        FakeJni::LocalFrame frame;
        auto method = getClass().getMethod("()V", "nativeOnSignOutCallback");
        if(method)
            method->invoke(frame.getJniEnv(), this);
    } catch(std::exception &e) {
        Log::error("ActiveDirectorySignIn", "nativeOnSignOutCallback failed: %s", e.what());
    }
}

void ActiveDirectorySignIn::clearCookies() {
    Log::info("ActiveDirectorySignIn", "clearCookies called");
}

std::shared_ptr<FakeJni::JString> ActiveDirectorySignIn::getAccessToken() {
    Log::info("ActiveDirectorySignIn", "getAccessToken called (%zu chars)", mAccessToken.size());
    return std::make_shared<FakeJni::JString>(mAccessToken);
}

FakeJni::JLong ActiveDirectorySignIn::getExpiresOn() {
    Log::info("ActiveDirectorySignIn", "getExpiresOn: %lld", (long long)mExpiresOn);
    return mExpiresOn;
}

std::shared_ptr<FakeJni::JString> ActiveDirectorySignIn::getUserHint() {
    return std::make_shared<FakeJni::JString>(mUserHint);
}

FakeJni::JBoolean ActiveDirectorySignIn::getCancelled() {
    return mCancelled;
}

FakeJni::JBoolean ActiveDirectorySignIn::hasError() {
    Log::info("ActiveDirectorySignIn", "hasError=%d err=%.200s", (int)mHasError, mErrorString.c_str());
    return mHasError;
}

std::shared_ptr<FakeJni::JString> ActiveDirectorySignIn::getErrorString() {
    return std::make_shared<FakeJni::JString>(mErrorString);
}

FakeJni::JInt ActiveDirectorySignIn::getErrorStatus() {
    return mErrorStatus;
}

FakeJni::JInt ActiveDirectorySignIn::getErrorSubStatus() {
    return mErrorSubStatus;
}

std::shared_ptr<FakeJni::JString> ActiveDirectorySignIn::getSignOutError() {
    return std::make_shared<FakeJni::JString>(mSignOutError);
}

void ActiveDirectorySignIn::onActivityResult(FakeJni::JInt requestCode, FakeJni::JInt resultCode, std::shared_ptr<FakeJni::JObject> data) {
    Log::info("ActiveDirectorySignIn", "onActivityResult: req=%d res=%d (stub, ignored)", (int)requestCode, (int)resultCode);
}

void ActiveDirectorySignIn::onDestroy() {
}

void ActiveDirectorySignIn::onResume() {
}

void ActiveDirectorySignIn::onStop() {
}

void ActiveDirectorySignIn::notifyDataChanged() {
    try {
        FakeJni::LocalFrame frame;
        auto method = getClass().getMethod("()V", "nativeOnDataChanged");
        if(method) {
            Log::info("ActiveDirectorySignIn", "Invoking nativeOnDataChanged (cancelled=%d error=%d)", (int)mCancelled, (int)mHasError);
            method->invoke(frame.getJniEnv(), this);
        } else {
            Log::warn("ActiveDirectorySignIn", "nativeOnDataChanged method not found (game native not registered?)");
        }
    } catch(std::exception &e) {
        Log::error("ActiveDirectorySignIn", "Failed to invoke nativeOnDataChanged: %s", e.what());
    }
}
