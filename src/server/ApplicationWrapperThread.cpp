#include "ApplicationWrapperThread.h"

#include <QProcessEnvironment>

ApplicationWrapperThread::ApplicationWrapperThread(QString applicationName, QObject * parent) :
    QThread(parent),
    applicationName(applicationName)
{
}

void ApplicationWrapperThread::run()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    //env.insert("LD_PRELOAD", "/usr/lib64/libkappstream.so");
    env.insert("LD_PRELOAD", "/usr/lib64/libkappstream.so /usr/lib64/libkappstreamhook.so");
    QProcess p;
    p.setProcessEnvironment(env);
    p.setProcessChannelMode(QProcess::ForwardedChannels);
    p.start(applicationName);
    p.waitForFinished(-1);
}
