#pragma once
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QList>
#include <QSize>
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
    QString group;      // 属性分组（用于属性面板按组折叠）
    QString unit;       // 显示单位（如 "px"）
    QString event;      // type=="code" 时有效
    QString language;   // type=="code" 时使用的语言（lua/js/...）
    QString snippet;    // 默认代码片段
    QList<PropertyOption> options; // type=="enum" 时有效
    double min = 0;
    double max = 99999;
    bool multiline = false;
    int  lines = 1;
    bool bindable = false;     // 是否可与外部变量绑定
    bool advanced = false;     // 是否归入"高级"折叠区
    bool readOnly = false;
    bool hidden   = false;
    bool required = false;
};

// 事件定义
struct EventDef {
    QString name;
    QString label;
    QString description;
};

// 数据绑定声明
struct BindingDef {
    QString name;       // 对应 property 或 event 名
    QString target;     // 目标语义槽（text/enabled/visible/trigger ...）
    QString direction;  // "in" / "out" / "inout"
};

// 控件 API 契约（Lua 模块实际实现了哪些方法）
struct WidgetApi {
    bool hasDraw             = false;
    bool hasOn               = false;
    bool hasGetProperty      = false;
    bool hasSetProperty      = false;
    bool hasGetProperties    = false;
    bool hasApplyProperties  = false;
    bool hasToState          = false;
    bool hasDestroy          = false;
};

// 自定义渲染提示（render_mode = "custom" 时供 Qt 端通用绘制使用）
// key: 提示项名称（shape/fill_from/text_from/...），value: 字符串
using DrawHints = QVariantMap;

// Widget 元数据
struct WidgetMeta {
    // 标识
    QString id;
    QString name;
    QString description;
    QString category;
    QString icon;            // 工具箱图标（相对路径或绝对路径）
    QString previewImage;    // 拖拽/占位预览图
    QString author;
    QStringList tags;
    QString schemaVersion;
    QString version;
    QString luaFilePath;

    // 渲染策略
    QString type;            // 真正的 LVGL 对象类型，如 "lv_button" / "custom"
    QString renderMode;      // "builtin" / "custom" / "image"

    // 尺寸约束
    QSize defaultSize { 120, 60 };
    QSize minSize     { 1, 1 };
    QSize maxSize     { 4096, 4096 };
    bool  resizable   = true;
    bool  rotatable   = false;

    // 属性 / 事件
    QList<PropertyMeta>  properties;
    QStringList          events;          // 兼容字段：仅事件名
    QList<EventDef>      eventDefs;       // 完整事件定义
    QList<PropertyMeta>  eventProperties; // 事件处理代码（编辑器中编辑）

    // 数据绑定
    QList<BindingDef>    bindings;
    bool hasDataBinding = false;

    // 自定义绘制提示
    DrawHints drawHints;

    // API 契约
    WidgetApi api;
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
Q_DECLARE_METATYPE(EventDef)
Q_DECLARE_METATYPE(BindingDef)
Q_DECLARE_METATYPE(WidgetApi)
Q_DECLARE_METATYPE(WidgetMeta)
Q_DECLARE_METATYPE(WidgetInstance)
Q_DECLARE_METATYPE(ScreenData)
Q_DECLARE_METATYPE(ProjectResources)
Q_DECLARE_METATYPE(ProjectTarget)
Q_DECLARE_METATYPE(ProjectData)
