#include "projectpropertiesdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

ProjectPropertiesDialog::ProjectPropertiesDialog(const ProjectData &project, QWidget *parent)
    : QDialog(parent)
    , m_data(project)
{
    setWindowTitle(tr("工程属性"));
    setMinimumWidth(420);
    buildUi();
    loadData();
}

ProjectData ProjectPropertiesDialog::projectData() const
{
    ProjectData d = m_data;

    d.name        = m_nameEdit->text().trimmed();
    d.description = m_descEdit->toPlainText().trimmed();
    d.author      = m_authorEdit->text().trimmed();

    d.target.width      = m_widthSpin->value();
    d.target.height     = m_heightSpin->value();
    d.target.colorDepth = m_colorDepthCombo->currentData().toInt();
    d.target.platform   = m_platformCombo->currentData().toString();

    d.resources.imagePath = m_imagePathEdit->text().trimmed();
    d.resources.linuxPath = m_linuxPathEdit->text().trimmed();

    return d;
}

void ProjectPropertiesDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // ---- 基本信息 ----
    auto *basicGroup  = new QGroupBox(tr("基本信息"), this);
    auto *basicForm   = new QFormLayout(basicGroup);

    m_nameEdit = new QLineEdit(this);
    basicForm->addRow(tr("工程名称："), m_nameEdit);

    m_authorEdit = new QLineEdit(this);
    basicForm->addRow(tr("作者："), m_authorEdit);

    m_descEdit = new QTextEdit(this);
    m_descEdit->setFixedHeight(72);
    basicForm->addRow(tr("描述："), m_descEdit);

    mainLayout->addWidget(basicGroup);

    // ---- 目标配置 ----
    auto *targetGroup = new QGroupBox(tr("目标配置"), this);
    auto *targetForm  = new QFormLayout(targetGroup);

    auto *resLayout = new QHBoxLayout;
    m_widthSpin  = new QSpinBox(this);
    m_widthSpin->setRange(1, 9999);
    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 9999);
    resLayout->addWidget(m_widthSpin);
    resLayout->addWidget(new QLabel(tr("×"), this));
    resLayout->addWidget(m_heightSpin);
    resLayout->addStretch();
    targetForm->addRow(tr("分辨率（px）："), resLayout);

    m_colorDepthCombo = new QComboBox(this);
    m_colorDepthCombo->addItem(tr("16 bit"), 16);
    m_colorDepthCombo->addItem(tr("24 bit"), 24);
    m_colorDepthCombo->addItem(tr("32 bit"), 32);
    targetForm->addRow(tr("颜色深度："), m_colorDepthCombo);

    m_platformCombo = new QComboBox(this);
    m_platformCombo->addItem(tr("Linux"),   QStringLiteral("linux"));
    m_platformCombo->addItem(tr("Windows"), QStringLiteral("windows"));
    m_platformCombo->addItem(tr("ARM"),     QStringLiteral("arm"));
    targetForm->addRow(tr("目标平台："), m_platformCombo);

    mainLayout->addWidget(targetGroup);

    // ---- 资源配置 ----
    auto *resGroup = new QGroupBox(tr("资源配置"), this);
    auto *resForm  = new QFormLayout(resGroup);

    m_imagePathEdit = new QLineEdit(this);
    resForm->addRow(tr("图片资源路径："), m_imagePathEdit);

    m_linuxPathEdit = new QLineEdit(this);
    resForm->addRow(tr("Linux 图片路径："), m_linuxPathEdit);

    mainLayout->addWidget(resGroup);

    // ---- 只读信息 ----
    auto *infoGroup = new QGroupBox(tr("工程信息"), this);
    auto *infoForm  = new QFormLayout(infoGroup);
    infoForm->addRow(tr("创建时间："), new QLabel(m_data.createdAt, this));
    infoForm->addRow(tr("更新时间："), new QLabel(m_data.updatedAt, this));
    infoForm->addRow(tr("工程 ID："),  new QLabel(m_data.id,        this));
    mainLayout->addWidget(infoGroup);

    // ---- 按钮 ----
    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void ProjectPropertiesDialog::loadData()
{
    m_nameEdit->setText(m_data.name);
    m_authorEdit->setText(m_data.author);
    m_descEdit->setPlainText(m_data.description);

    m_widthSpin->setValue(m_data.target.width);
    m_heightSpin->setValue(m_data.target.height);

    const int cdIdx = m_colorDepthCombo->findData(m_data.target.colorDepth);
    m_colorDepthCombo->setCurrentIndex(cdIdx >= 0 ? cdIdx : 0);

    const int platIdx = m_platformCombo->findData(m_data.target.platform);
    m_platformCombo->setCurrentIndex(platIdx >= 0 ? platIdx : 0);

    m_imagePathEdit->setText(m_data.resources.imagePath);
    m_linuxPathEdit->setText(m_data.resources.linuxPath);
}
