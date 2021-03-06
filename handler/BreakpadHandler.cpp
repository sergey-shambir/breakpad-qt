/*
    Copyright (c) 2009, Aleksey Palazhchenko
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "BreakpadHandler.h"

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>

#if defined(Q_OS_MAC)
#include "client/mac/handler/exception_handler.h"
#elif defined(Q_OS_LINUX)
#include "client/linux/handler/exception_handler.h"
#elif defined(Q_OS_WIN32)
#include "client/windows/handler/exception_handler.h"
#endif

namespace BreakpadQt
{

class GlobalHandlerPrivate
{
public:
    GlobalHandlerPrivate();
    ~GlobalHandlerPrivate();

public:
    static char reporter_[1024];
    static char reporterArguments_[8*1024];
    static google_breakpad::ExceptionHandler* handler_;
    static ReportCrashesToSystem reportCrashesToSystem_;
};

char GlobalHandlerPrivate::reporter_[1024] = {0};
char GlobalHandlerPrivate::reporterArguments_[8*1024] = {0};
google_breakpad::ExceptionHandler* GlobalHandlerPrivate::handler_ = 0;
ReportCrashesToSystem GlobalHandlerPrivate::reportCrashesToSystem_ = ReportUnhandled;


bool launcher(const char* program, const char* const arguments[])
{
    // TODO launcher
//	if(!GlobalHandlerPrivate::reporter_.isEmpty()) {
//		QProcess::startDetached(GlobalHandlerPrivate::reporter_);	// very likely we will die there
//	}

    Q_UNUSED(program);
    Q_UNUSED(arguments);
    return false;
}


#if defined(Q_OS_WIN32)
bool DumpCallback(const wchar_t* dump_dir,
                  const wchar_t* minidump_id,
                  void* context,
                  EXCEPTION_POINTERS* exinfo,
                  MDRawAssertionInfo* assertion,
                  bool succeeded)
#elif defined(Q_OS_MAC)
bool DumpCallback(const char *dump_dir,
                  const char *minidump_id,
                  void *context, bool succeeded)
#else
bool DumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                  void* context,
                  bool succeeded)
#endif
{
#ifdef Q_OS_LINUX
    Q_UNUSED(descriptor);
#endif
    Q_UNUSED(context);
#if defined(Q_OS_WIN32)
    Q_UNUSED(assertion);
    Q_UNUSED(exinfo);
#endif
    /*
        NO STACK USE, NO HEAP USE THERE !!!
        Creating QString's, using qDebug, etc. - everything is crash-unfriendly.
    */
    // But I'm brave man.

#if defined(Q_OS_WIN32)
    QString path = QString::fromWCharArray(dump_dir) + QLatin1String("/") + QString::fromWCharArray(minidump_id);
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));
#elif defined(Q_OS_MAC)
    QString path = QString::fromUtf8(dump_dir) + QLatin1String("/") + QString::fromUtf8(minidump_id);
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", qPrintable(path));
#else
    qDebug("%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump", descriptor.path());
#endif

    launcher(GlobalHandlerPrivate::reporter_, 0);
    return (GlobalHandlerPrivate::reportCrashesToSystem_ == ReportUnhandled) ? true : false;
}

GlobalHandlerPrivate::GlobalHandlerPrivate()
{
    handler_ = NULL;
}

GlobalHandlerPrivate::~GlobalHandlerPrivate()
{
    delete handler_;
    handler_ = NULL;
}


GlobalHandler* GlobalHandler::instance()
{
    static GlobalHandler globalHandler;
    return &globalHandler;
}

GlobalHandler::GlobalHandler()
{
    d = new GlobalHandlerPrivate();
}

GlobalHandler::~GlobalHandler()
{
    delete d;
    d = 0;
}

void GlobalHandler::setDumpPath(const QString& path)
{
    QString absPath = path;
    if(!QDir::isAbsolutePath(absPath)) {
        absPath = QDir::cleanPath(qApp->applicationDirPath() + QLatin1String("/") + path);
    }
    Q_ASSERT(QDir::isAbsolutePath(absPath));

    QDir().mkpath(absPath);
    if (!QDir().exists(absPath)) {
        qDebug("Failed to set dump path which not exists: %s", qPrintable(absPath));
        return;
    }

#if defined(Q_OS_WIN32)
    d->handler_ = new google_breakpad::ExceptionHandler(absPath.toStdWString(), /*FilterCallback*/ 0,
                                                        DumpCallback, /*context*/ 0,
                                                        google_breakpad::ExceptionHandler::HANDLER_ALL);
#elif defined(Q_OS_MAC)
    d->handler_ = new google_breakpad::ExceptionHandler(absPath.toStdString(), /*FilterCallback*/ 0,
                                                        DumpCallback, /*context*/ 0, true, NULL);
#else
    d->handler_ = new google_breakpad::ExceptionHandler(google_breakpad::MinidumpDescriptor(absPath.toStdString()),
                                                        /*FilterCallback*/ 0, DumpCallback, /*context*/ 0, true, -1);
#endif
}

void GlobalHandler::setReporter(const QString& reporter)
{
    QString rep = reporter;

    if(!QDir::isAbsolutePath(rep)) {
#if defined(Q_OS_MAC)
            // TODO(AlekSi) What to do if we are not inside bundle?
            rep = QDir::cleanPath(qApp->applicationDirPath() + QLatin1String("/../Resources/") + rep);
#elif defined(Q_OS_LINUX) || defined(Q_OS_WIN32)
            // MAYBE(AlekSi) Better place for Linux? libexec? or what?
            rep = QDir::cleanPath(qApp->applicationDirPath() + QLatin1String("/") + rep);
#else
#error unsupported platform, sorry
#endif

        qDebug("BreakpadQt: setReporter: %s -> %s", qPrintable(reporter), qPrintable(rep));
    }
    Q_ASSERT(QDir::isAbsolutePath(rep));

    // add .exe for Windows if needed
#	if defined(Q_OS_WIN32)
        if(!QDir().exists(rep)) {
            rep += QLatin1String(".exe");
        }
#	endif
    Q_ASSERT(QDir().exists(rep));

    qstrcpy(d->reporter_, QFile::encodeName(rep));
}

void GlobalHandler::setReportCrashesToSystem(ReportCrashesToSystem report)
{
    d->reportCrashesToSystem_ = report;
}

bool GlobalHandler::writeMinidump()
{
    if (!d->handler_) {
        qWarning("BreakpadQt: writeMinidump() is not possible, since dump path not set.");
        return false;
    }
    bool res = d->handler_->WriteMinidump();
    if (res) {
        qDebug("BreakpadQt: writeMinidump() successed.");
    } else {
        qWarning("BreakpadQt: writeMinidump() failed.");
    }
    return res;
}

}	// namespace
