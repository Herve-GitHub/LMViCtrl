#pragma once

#include <QDialog>
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
                               QWidget *parent = nullptr);

    void setAction(const EventAction &action);
    EventAction action() const;

private:
    void rebuildTargetTable();
    void updateEditorState();
    QString currentActionType() const;
    QString currentActionLabel() const;

    QString m_eventName;
    QList<ScreenData> m_screens;
    QList<WidgetInstance> m_widgets;
    EventAction m_initialAction;

    QComboBox *m_typeCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_targetTable = nullptr;
    QStackedWidget *m_detailStack = nullptr;
    QPlainTextEdit *m_codeEdit = nullptr;
    QLineEdit *m_propertyEdit = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QLineEdit *m_methodEdit = nullptr;
    QLineEdit *m_freemasterTargetEdit = nullptr;
    QLineEdit *m_freemasterValueEdit = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
};
