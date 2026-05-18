#include "projectpropertiesdialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include "HttpServer.h"
ProjectPropertiesDialog::ProjectPropertiesDialog(const ProjectData &project,
                                                 const QString &projectDir,
                                                 QWidget *parent)
    : QDialog(parent)
    , m_data(project)
    , m_projectDir(projectDir)
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

    d.font.file = m_fontFileEdit->text().trimmed();
    d.font.size = m_fontSizeSpin->value();

    d.dataClient.enabled = m_dataEnabledCheck->isChecked();
    d.dataClient.server = m_gatewayHmiAddressEdit->text().trimmed();
    d.dataClient.websocketPath = m_dataWsPathEdit->text().trimmed();
    d.dataClient.token = m_dataTokenEdit->text().trimmed();
    d.dataClient.timeoutMs = m_dataTimeoutSpin->value();

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

    // ---- 字体配置 ----
    auto *fontGroup = new QGroupBox(tr("字体"), this);
    auto *fontForm  = new QFormLayout(fontGroup);

    auto *fontFileLayout = new QHBoxLayout;
    m_fontFileEdit = new QLineEdit(this);
    m_fontFileEdit->setReadOnly(true);
    m_fontFileEdit->setPlaceholderText(tr("（未选择，使用默认字体）"));
    m_fontBrowseBtn = new QPushButton(tr("浏览…"), this);
    m_fontClearBtn  = new QPushButton(tr("清除"),  this);
    if (m_projectDir.isEmpty()) {
        m_fontBrowseBtn->setEnabled(false);
        m_fontBrowseBtn->setToolTip(tr("请先保存工程后再选择字体文件"));
    }
    fontFileLayout->addWidget(m_fontFileEdit, 1);
    fontFileLayout->addWidget(m_fontBrowseBtn);
    fontFileLayout->addWidget(m_fontClearBtn);
    fontForm->addRow(tr("字体文件："), fontFileLayout);

    m_fontSizeSpin = new QSpinBox(this);
    m_fontSizeSpin->setRange(6, 200);
    m_fontSizeSpin->setSuffix(tr(" px"));
    fontForm->addRow(tr("默认字号："), m_fontSizeSpin);

    connect(m_fontBrowseBtn, &QPushButton::clicked,
            this, &ProjectPropertiesDialog::onBrowseFont);
    connect(m_fontClearBtn,  &QPushButton::clicked,
            this, &ProjectPropertiesDialog::onClearFont);

    mainLayout->addWidget(fontGroup);

    // ---- HMI 网关配置 ----
    auto *gatewayGroup = new QGroupBox(tr("HMI网关配置"), this);
    auto *gatewayForm = new QFormLayout(gatewayGroup);

    m_gatewayHmiAddressEdit = new QLineEdit(this);
    m_gatewayHmiAddressEdit->setPlaceholderText(tr("例如：192.168.1.100"));
    gatewayForm->addRow(tr("HMI 地址："), m_gatewayHmiAddressEdit);

    mainLayout->addWidget(gatewayGroup);

    // ---- 数据接口配置 ----
    auto *dataGroup = new QGroupBox(tr("数据接口"), this);
    auto *dataForm = new QFormLayout(dataGroup);

    m_dataEnabledCheck = new QCheckBox(tr("启用启动时自动连接"), this);
    dataForm->addRow(QString(), m_dataEnabledCheck);

    m_dataWsPathEdit = new QLineEdit(this);
    m_dataWsPathEdit->setPlaceholderText(QStringLiteral("/ws"));
    dataForm->addRow(tr("WebSocket 路径："), m_dataWsPathEdit);

    m_dataTokenEdit = new QLineEdit(this);
    m_dataTokenEdit->setPlaceholderText(tr("HTTP 查询 token，可为空"));
    dataForm->addRow(tr("Token："), m_dataTokenEdit);

    m_dataTimeoutSpin = new QSpinBox(this);
    m_dataTimeoutSpin->setRange(1000, 60000);
    m_dataTimeoutSpin->setSingleStep(500);
    m_dataTimeoutSpin->setSuffix(tr(" ms"));
    dataForm->addRow(tr("连接超时："), m_dataTimeoutSpin);

    mainLayout->addWidget(dataGroup);

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

  //  connect(buttons, &QDialogButtonBox::accepted, this, &ProjectPropertiesDialog::onOkClicked);
   // connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

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

    m_fontFileEdit->setText(m_data.font.file);
    m_fontSizeSpin->setValue(m_data.font.size > 0 ? m_data.font.size : 16);

    m_gatewayHmiAddressEdit->setText(m_data.dataClient.server);

    m_dataEnabledCheck->setChecked(m_data.dataClient.enabled);
    m_dataWsPathEdit->setText(m_data.dataClient.websocketPath.isEmpty()
                                  ? QStringLiteral("/ws")
                                  : m_data.dataClient.websocketPath);
    m_dataTokenEdit->setText(m_data.dataClient.token);
    m_dataTimeoutSpin->setValue(m_data.dataClient.timeoutMs > 0
                                    ? m_data.dataClient.timeoutMs
                                    : 5000);
}

// 用户点击"浏览…"，把所选 TTF/OTF 文件拷贝到 <projectDir>/fonts/ 中，
// 并把相对路径（如 "fonts/MyFont.ttf"）写入字体文件输入框。
void ProjectPropertiesDialog::onBrowseFont()
{
    if (m_projectDir.isEmpty()) return;

    const QString picked = QFileDialog::getOpenFileName(
        this, tr("选择字体文件"), QString(),
        tr("字体文件 (*.ttf *.otf);;所有文件 (*.*)"));
    if (picked.isEmpty()) return;

    const QString fontsDirRel = QStringLiteral("fonts");
    QDir projDir(m_projectDir);
    if (!projDir.exists(fontsDirRel) && !projDir.mkpath(fontsDirRel)) {
        QMessageBox::warning(this, tr("字体"),
                             tr("无法创建字体目录：%1")
                                 .arg(projDir.filePath(fontsDirRel)));
        return;
    }

    const QFileInfo srcFi(picked);
    const QString dstAbs = projDir.absoluteFilePath(
        fontsDirRel + QLatin1Char('/') + srcFi.fileName());

    // 若已存在，先尝试删除以便覆盖（同名不同源会被覆盖）
    if (QFile::exists(dstAbs) && QFileInfo(picked) != QFileInfo(dstAbs)) {
        QFile::remove(dstAbs);
    }

    if (QFileInfo(picked) != QFileInfo(dstAbs)) {
        if (!QFile::copy(picked, dstAbs)) {
            QMessageBox::warning(this, tr("字体"),
                                 tr("无法拷贝字体文件到：%1").arg(dstAbs));
            return;
        }
    }

    const QString relPath = fontsDirRel + QLatin1Char('/') + srcFi.fileName();
    m_fontFileEdit->setText(relPath);
}

void ProjectPropertiesDialog::onClearFont()
{
    m_fontFileEdit->clear();
}


void ProjectPropertiesDialog::onOkClicked()
{
    // 1. 先拿到当前界面所有填写好的数据
    ProjectData pData = projectData();

    // 2. 取出填写的网关地址
    QString gatewayAddr = pData.dataClient.server;

    // 3. 调用函数，把地址传进去
    getGatewayTags(gatewayAddr.toUtf8().constData());


    // 最后关闭对话框
    accept();
}
