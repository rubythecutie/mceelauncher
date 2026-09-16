#pragma once

#include <QString>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlSchemeHandler>
#include <utility>

class SchemeHandler : public QWebEngineUrlSchemeHandler {
private:
    QString endUrl;
public:
    explicit SchemeHandler(QString endUrl) : endUrl(std::move(endUrl)) {}
    void requestStarted(QWebEngineUrlRequestJob *request) override;
};

class InterceptPage : public QWebEnginePage {
private:
    QString endUrlStr;
public:
    explicit InterceptPage(QWebEngineProfile *profile, QString endUrlStr, QObject *parent = nullptr);
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
    bool checkAndHandle(const QUrl &url);
    static bool matchesEndUrl(const QUrl &url, const QString &endUrlStr);
    static bool isWrongPlace(const QUrl &url);
};
