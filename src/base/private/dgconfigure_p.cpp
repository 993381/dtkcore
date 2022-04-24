#include "dgconfigure_p.h"
#include <QDebug>
#include <QGSettings/QGSettings>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

/*!
  \class DGConfigure
  \brief 使用 QGSettings 配置 schema
  该配置文件使用一个默认的配置，对应的 schema 文件在 dtkcommon 里面：https://gerrit.uniontech.com/c/dtkcommon/+/57369
  如果想使用扩展的配置文件，则使用宏 gConfigureSelect。
  DGConfigure 用法：
    sudo vim /usr/share/glib-2.0/schemas/com.deepin.dtk.gschema.xml
    sudo glib-compile-schemas /usr/share/glib-2.0/schemas
    特别注意的一点，gsettings 的配置文件的key值不能带大写字母，如果是这种的 key：<key type="s" name="log-format">，
    gConfigure->setValue 和 gConfigure->getValue 传入的字符应该做转换，使用 logFormat 才能获取到正确的值。
    可以用 dconf-editor 方便的管理这些键值对，如果不小心删除了某个键值对，只需要找到对应的 schema 文件，修改一下
    <summary> 或者 <description> 里面的内容，再次执行 sudo glib-compile-schemas /usr/share/glib-2.0/schemas 即可，
    如果还是有问题，那就需要用 gConfigure->resetValue 配合以上步骤重新设置即可。
  日志格式配置：
    这是一个典型的日志格式：
        "%{time}{yyyy-MM-dd, HH:mm:ss.zzz} [%{type:-7}] [%{file:-20} %{function:-35} %{line}] %{message}\n"
    后面的换行符需要自己添加，"% { } time type file function line message :"以及花括号中的内容是解析时的关键字，各项可选，
    按照需求配置，其它的按原样输出。花括号中的数字是控制 Log 中对应项项宽度的数值。从环境变量中设置 log format 请使用
    DTK_MESSAGE_PATTERN。

   接口的使用：
    包含文件： DLog、private/dgconfigure_p.h
      app.setApplicationName("mytest");                         // 该字段不可以省略
      Dtk::Core::DLogManager::registerConsoleAppender();        // 控制台输出格式化日志
      Dtk::Core::DLogManager::registerFileAppender();           // 追加日志到文件
    方式一，使用默认的 sechma 文件：
      gConfigure->getValue("logFormat");                        // 直接使用该宏从默认的配置文件获取配置
    方式二，使用新的 sechma 配置文件：
      if (auto config = gConfigure->selectSchema("com.test.mytest", "/com/test/mytest/"))
      {
           qDebug() << "log fmt: " << config->getValue("logFormat");
           // 获取到当前 schema 的所有 key：
           qDebug() << "all keys from mytest: " << config->allKeys();
           config->setValue("logFormat", "xxx");
           config->resetValue("logFormat");
      }
    以上获取到的字段默认内容是 "format"。sechma文件中的内容每次修改完后都用 glib-compile-schemas 编译生效。
*/

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(dcoreConfigure, "dtk.dconfigure")
#else
Q_LOGGING_CATEGORY(dcoreConfigure, "dtk.dconfigure", QtInfoMsg)
#endif

// gsettings path: /usr/share/glib-2.0/schemas
const QByteArray DGConfigure::s_defaultSchemaId = "com.deepin.dtk";
const QByteArray DGConfigure::s_defaultSchemaPath = "/com/deepin/dtk/";

QVector<DGConfigure *> DGConfigure::s_schemaVec;

QMutex DGConfigure::s_mtx;

/*
  \fn DGConfigure::instance(const QString &appName, const QString &schemaId, const QString &schemaPath)
  \brief 使用默认的配置文件创建一个默认的实例。默认的 gsettings 是 relocation 的模式。
  直接使用 gConfigure 这个宏即可方便的调用，对默认的配置项进行各种操作。
  一个重要的前提是： DAppliction 的 applicationName 必须有被预先设置。
 */
DGConfigure *DGConfigure::instance(const QString &appName,
                                                 const QString &schemaId,
                                                 const QString &schemaPath)
{
    static DGConfigure *gs =  new DGConfigure(appName, schemaId, schemaPath);
    return gs;
}

bool DGConfigure::setValue(const QString &key, const QString &value)
{
    return isValidKey(key) && m_schemaInfo.m_pSettings->trySet(key, value);
}

QString DGConfigure::getValue(const QString &key)
{
    return isValidKey(key) ? m_schemaInfo.m_pSettings->get(key).toString() : QString();
}

QStringList DGConfigure::allKeys()
{
    if (!isValidSchemaId(m_schemaInfo.m_schemaId))
        return QStringList();
    return m_schemaInfo.m_pSettings->keys();
}

void DGConfigure::resetValue(const QString &key)
{
    if (isValidKey(key))
        return;
    m_schemaInfo.m_pSettings->reset(key);
}

/*
  \fn DGConfigure::createNewSchema(const QString &appName, const QString &schemaId, const QString &schemaPath)
  brief: 内部使用， selectSchema 的时候会尝试创建一个新的，如果已经存在该 schema 的配置则直接返回已存在的。
  如果传入参数对应的 schema 不存在则新建并加入到实例列表，该函数线程安全。
  若使用 gsettings 在 relocation 的模式下，如果手动删除了对应的 schema 文件，则需要重启应用程序才会被重新创建。
*/
DGConfigure *DGConfigure::createNewSchema(const QString &appName, const QString &schemaId, const QString &schemaPath)
{
    if (schemaPath.isEmpty()) {
        qFatal("Create schema faild. schema path invalid.");
    }
    if (!DGConfigure::isValidSchemaId(schemaId)) {
        qCWarning(dcoreConfigure, "Create schema faild. schema id does not exist, please configure gschema.xml and compile.");
        return nullptr;
    }

    QMutexLocker locker(&s_mtx);

    for (DGConfigure *schema : s_schemaVec) {
        if (schema->m_schemaInfo.m_schemaId == schemaId.toUtf8() &&
                schema->m_schemaInfo.m_schemaPath == schemaPath.toUtf8() &&
                schema->m_schemaInfo.m_appName == appName.toUtf8()) {
            qCWarning(dcoreConfigure) << QString("Create new schema faile, already have a schame [%1] [%2] in instance list.").arg(schemaId).arg(schemaPath);
            return schema;
        }
    }

    DGConfigure *gs =  new DGConfigure(appName, schemaId, schemaPath);
    // default is instance
    s_schemaVec.push_back(gs);
    return gs;
}

/*
  \fn DGConfigure::selectSchema(const QString &schemaId, const QString &schemaPath, const QString &appName)
  \brief 调用该函数的前提是保证你已经在 /usr/share/glib-2.0/schemas 下放置了正确的 .gschema.xml 配置文件
  并且已经执行了编译命令： sudo glib-compile-schemas /usr/share/glib-2.0/schemas
  对于 relocation 的模式，可以指定多个自定义的路径，即使路径不是真实存在的。
  对于非 relocation 的模式，则 id 和 path 必须正确，id 错误则会报错，path 错误则会导致崩溃。
  通常，只有 id，没有指定 path 的 .gschema.xml 文件是 relocation 的模式。
 */
DGConfigure *DGConfigure::selectSchema(const QString &schemaId,
                                                     const QString &schemaPath,
                                                     const QString &appName)
{
    DGConfigure *schema = createNewSchema(appName, schemaId, schemaPath);
    if (!schema) {
        qCWarning(dcoreConfigure, "Select schema failed, return nullptr");
    }
    return schema;
}

bool DGConfigure::isValidSchemaId(const QString &schemaId)
{
    if (!QGSettings::isSchemaInstalled(schemaId.toUtf8())) {
        qCWarning(dcoreConfigure) << QString("Error, Schema id %1 does not exist.").arg(schemaId);
        return false;
    }
    return true;
}

bool DGConfigure::isValidKey(const QString &key)
{
    return DGConfigure::isValidSchemaId(m_schemaInfo.m_schemaId)
            && m_schemaInfo.m_pSettings->keys().indexOf(key) != -1;
}

void DGConfigure::setOnValueChangedCallback(std::function<void(const QString &key)> callback)
{
    m_schemaInfo.m_onValueChanged = std::move(callback);
}

DGConfigure::DGConfigure(const QString &appName, const QString &schemaId, const QString &schemaPath)
{
    QByteArray path = schemaPath.toUtf8();
    path.append(appName + "/");
    QByteArray id = schemaId.toUtf8();

    m_schemaInfo.m_appName = appName.toUtf8();
    m_schemaInfo.m_schemaId = schemaId.toUtf8();
    m_schemaInfo.m_schemaPath = schemaPath.toUtf8();

    m_schemaInfo.m_pSettings = new QGSettings(id, path);

    QObject::connect(m_schemaInfo.m_pSettings, &QGSettings::changed, [this](const QString &key) {
        if (m_schemaInfo.m_onValueChanged) {
            m_schemaInfo.m_onValueChanged(key);
        }
    });

    std::atexit(destroy);
}

void DGConfigure::destroy()
{
    for (DGConfigure *settings: s_schemaVec) {
        if (settings->m_schemaInfo.m_pSettings) {
            delete settings->m_schemaInfo.m_pSettings;
            settings->m_schemaInfo.m_pSettings = nullptr;
        }
        if (settings) {
            delete settings;
            settings = nullptr;
        }
    }
}

DCORE_END_NAMESPACE
