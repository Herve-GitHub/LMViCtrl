#pragma once

#include <QDockWidget>
#include <QPointer>

#include "widgemeta.h"

class BindingGraphView;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QWidget;

class BindingDetailDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit BindingDetailDock(QWidget *parent = nullptr);

    void setProjectData(ProjectData *project);
    void setWidgetMetas(const QList<WidgetMeta> &metas);
    void setGraphView(BindingGraphView *view);

public slots:
    void setCurrentEdge(const QString &edgeId);
    void setCurrentNode(const QString &nodeId);
    void refreshPanel();

private:
    void buildUi();
    void clearForms();
    void buildEmptyPanel();
    void buildEdgePanel(const BindingEdge &edge);
    void buildSequenceList(const BindingEdge &edge);
    void buildNodePanel(const BindingNode &node);
    void buildNodeActionSequences(const BindingNode &node);
    void applyChanges();
    QString endpointTitle(const BindingEndpoint &endpoint) const;
    QString nodeTitle(const QString &nodeId) const;
    QString edgeTitle(const BindingEdge &edge) const;
    QString paramsToJson(const QVariantMap &params) const;
    bool paramsFromJson(const QString &text, QVariantMap *params) const;

    ProjectData *m_project = nullptr;
    QList<WidgetMeta> m_metas;
    QPointer<BindingGraphView> m_graphView;
    QString m_currentEdgeId;
    QString m_currentNodeId;
    bool m_loading = false;

    QScrollArea *m_scroll = nullptr;
    QWidget *m_root = nullptr;
    QGroupBox *m_summaryBox = nullptr;
    QGroupBox *m_sequenceBox = nullptr;
    QGroupBox *m_editBox = nullptr;
    QFormLayout *m_summaryForm = nullptr;
    QFormLayout *m_editForm = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_targetEdit = nullptr;
    QLineEdit *m_typeEdit = nullptr;
    QListWidget *m_sequenceList = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QSpinBox *m_orderSpin = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QLineEdit *m_conditionEdit = nullptr;
    QPlainTextEdit *m_paramsEdit = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_clearButton = nullptr;
};
