#pragma once
// ?? ?? ���� 3 �м��ϣ����� ?? ??
// 👇 只需要这 4 个，绝对不会报错
// 👇 👇 把这 3 行加上！！！ 👇 👇
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEventLoop>

#include <QDialog>
#include <QHash>
#include "widgemeta.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;
class QTableWidget;

class EventActionDialog : public QDialog
{
public:
    explicit EventActionDialog(const QString& eventName,
        const QList<ScreenData>& screens,
        const QList<WidgetInstance>& widgets,
        const QList<WidgetMeta>& widgetMetas,
        QWidget* parent = nullptr);

    void setAction(const EventAction& action);
    EventAction action() const;

private:
    void rebuildTargetTable();
    void rebuildPropertyCombo(const QString& preferredProperty = QString(), bool updateValue = true);
    void updateEditorState();

    QString currentActionType() const;
    QString currentActionLabel() const;
    QString currentPropertyName() const;
    WidgetInstance currentTargetWidget() const;
    QString valueTextForProperty(const WidgetInstance& widget, const PropertyMeta& property) const;

    QString m_eventName;
    QList<ScreenData> m_screens;
    QList<WidgetInstance> m_widgets;
    QHash<QString, WidgetMeta> m_widgetMetas;
    EventAction m_initialAction;

    QComboBox* m_typeCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QTableWidget* m_targetTable = nullptr;
    QStackedWidget* m_detailStack = nullptr;
    QPlainTextEdit* m_codeEdit = nullptr;
    QComboBox* m_propertyCombo = nullptr;
    QLineEdit* m_valueEdit = nullptr;
    QLineEdit* m_methodEdit = nullptr;
    QLineEdit* m_freemasterTargetEdit = nullptr;
    QLineEdit* m_freemasterValueEdit = nullptr;
    QCheckBox* m_enabledCheck = nullptr;
    QComboBox* m_dataPointCombo = nullptr; // <-- �����
    QComboBox* m_valuePointCombo = nullptr; // <-- �����
    QComboBox* m_dateWritePointCombo = nullptr; // <-- �����

    QLineEdit* m_valueStartEdit = nullptr;
    QLineEdit* m_valueEndEdit = nullptr;
    QComboBox* m_valuePropCombo = nullptr;
    QLineEdit* m_valueFinalEdit = nullptr;


    QLineEdit* m_dateWriteValueEdit = nullptr;   // datewrite
};
