#pragma once

#include <QDialog>
#include "widgemeta.h"

class QLineEdit;
class QTableWidget;

class EventTriggerDialog : public QDialog
{
public:
    explicit EventTriggerDialog(const WidgetMeta &meta,
                                const QStringList &existingEvents,
                                QWidget *parent = nullptr);

    EventDef selectedEvent() const;

private:
    void rebuildTable();
    QList<EventDef> triggerDefs() const;

    WidgetMeta m_meta;
    QStringList m_existingEvents;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
};
