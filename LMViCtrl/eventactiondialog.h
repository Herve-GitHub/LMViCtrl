#pragma once

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
    explicit EventActionDialog(const QString &eventName,
                               const QList<ScreenData> &screens,
                               const QList<WidgetInstance> &widgets,
                               const QList<WidgetMeta> &widgetMetas,
                               QWidget *parent = nullptr);

    void setAction(const EventAction &action);
    EventAction action() const;

private:
    void rebuildTargetTable();
    void rebuildPropertyCombo(const QString &preferredProperty = QString(), bool updateValue = true);
    void updateEditorState();
    QString currentActionType() const;
    QString currentActionLabel() const;
    QString currentPropertyName() const;
    WidgetInstance currentTargetWidget() const;
    QString valueTextForProperty(const WidgetInstance &widget, const PropertyMeta &property) const;

    QString m_eventName;
    QList<ScreenData> m_screens;
    QList<WidgetInstance> m_widgets;
    QHash<QString, WidgetMeta> m_widgetMetas;
    EventAction m_initialAction;

    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_targetTable = nullptr;
    QStackedWidget *m_detailStack = nullptr;
    QPlainTextEdit *m_codeEdit = nullptr;
    QComboBox *m_propertyCombo = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QLineEdit *m_methodEdit = nullptr;
    QLineEdit *m_freemasterTargetEdit = nullptr;
    QLineEdit *m_freemasterValueEdit = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
};
