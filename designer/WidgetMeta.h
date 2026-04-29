#pragma once
#include <QString>
#include <QVariant>
#include <QList>
#include <QStringList>
#include <QMetaType>

// 属性选项（支持纯字符串和 {value, label} 两种格式）
struct PropertyOption {
    QString value;
    QString label;
};

// 属性元信息
struct PropertyMeta {
    QString name;
    QString type;       // string/number/boolean/color/enum/code/resource
    QVariant defaultValue;
    QString label;
    QString description;
    QString event;      // type=="code" 时有效
    QList<PropertyOption> options; // type=="enum" 时有效
    double min = 0;
    double max = 99999;
    bool multiline = false;
    int lines = 1;
};

// Widget 元数据
struct WidgetMeta {
    QString id;
    QString name;
    QString description;
    QString schemaVersion;
    QString version;
    QString luaFilePath;
    QList<PropertyMeta> properties;
    QStringList events;
    bool hasDataBinding = false;
};

// Widget 实例
struct WidgetInstance {
    QString instanceId;   // UUID
    QString widgetId;
    QString name;         // 工程内唯一的实例名（用户可改）
    int zOrder = 0;// 层级顺序，数值越大越靠上
    int x = 0;// 位置   
    int y = 0;// 位置
    int width = 100;// 大小
    int height = 100;// 大小
    bool locked = false;// 是否锁定位置和大小
    bool visible = true;// 是否可见
    QVariantMap properties;// 属性值，key 是属性名，value 是属性值
};

// Screen 页面
struct ScreenData {
    QString id;// UUID
    QString name;// 页面名称
    int order = 0;// 页面顺序
    QString bgColor = "#000000";// 默认背景色
    QList<WidgetInstance> widgets;// 页面上的组件实例列表
};

// 项目资源配置
struct ProjectResources {
    QString imagePath = "./image/";// 图片资源路径
    QString linuxPath = "/root/image/";// Linux 图片资源路径
};

// 项目目标配置
struct ProjectTarget {
    int width = 1024;// 分辨率
    int height = 768;// 分辨率
    int colorDepth = 16;// 颜色深度，单位 bit
    QString platform = "linux";// linux/windows/arm/...
};

// 整个项目
struct ProjectData {
    QString id;// UUID
    QString schemaVersion = "1.0";//数据结构版本
    QString name = "NewProject";//工程名称
    QString description;//工程描述
    QString author;//工程作者
    QString createdAt;//工程创建时间
    QString updatedAt;//工程更新时间
    ProjectTarget target;//目标配置
    ProjectResources resources;//资源配置   
    QList<ScreenData> screens;//页面列表
};

Q_DECLARE_METATYPE(PropertyOption)
Q_DECLARE_METATYPE(PropertyMeta)
Q_DECLARE_METATYPE(WidgetMeta)
Q_DECLARE_METATYPE(WidgetInstance)
Q_DECLARE_METATYPE(ScreenData)
Q_DECLARE_METATYPE(ProjectResources)
Q_DECLARE_METATYPE(ProjectTarget)
Q_DECLARE_METATYPE(ProjectData)