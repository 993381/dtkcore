#ifndef DGCONFIGURE_P_H
#define DGCONFIGURE_P_H

#include <QCoreApplication>
#include <functional>
#include <QMutex>

#include "dtkcore_global.h"

// TODO: 所有配置文件 gsettins 相关的东西以后都替换为：https://gerrit.uniontech.com/c/dtkcore/+/48960

class QGSettings;

DCORE_BEGIN_NAMESPACE

#ifndef GURADED_BY
#define GURADED_BY(...)
#endif

class DGConfigure {
public:
    static DGConfigure *instance(const QString &appName,
                                 const QString &schemaId = s_defaultSchemaId,
                                 const QString &schemaPath = s_defaultSchemaPath);

    bool setValue(const QString &key, const QString &value);
    QString getValue(const QString &key);
    QStringList allKeys();
    void resetValue(const QString &key);

    static DGConfigure *selectSchema(const QString &schemaId,
                                     const QString &schemaPath,
                                     const QString &appName = qApp->applicationName());

    static bool isValidSchemaId(const QString &schemaId);
    bool isValidKey(const QString &key);

    void setOnValueChangedCallback(std::function<void(const QString &key)> callback);

private:
    static QVector<DGConfigure *> s_schemaVec;

    struct {
        QByteArray m_schemaId;
        QByteArray m_schemaPath;
        QByteArray m_appName;
        QGSettings *m_pSettings;
        std::function<void(const QString &key)> m_onValueChanged;
    } m_schemaInfo;

    static QMutex s_mtx;

    static DGConfigure *createNewSchema(const QString &appName,
                                        const QString &schemaId,
                                        const QString &schemaPath) GURADED_BY(s_mtx);

private:
    static const QByteArray s_defaultSchemaId;
    static const QByteArray s_defaultSchemaPath;

private:
    explicit DGConfigure(const QString &appName, const QString &schemaId, const QString &schemaPath);
    ~DGConfigure() = default;
    DGConfigure(const DGConfigure &) = delete;
    DGConfigure &operator=(const DGConfigure &) = delete;
    DGConfigure(DGConfigure &&) = delete;
    DGConfigure &operator=(DGConfigure &&) = delete;
    static void destroy();
};

#define gConfigure DTK_CORE_NAMESPACE::DGConfigure::instance(qApp->applicationName())
#define gConfigureSelect(...)  DTK_CORE_NAMESPACE::DGConfigure::selectSchema(__VA_ARGS__)

DCORE_END_NAMESPACE

#endif // DGCONFIGURE_P_H
