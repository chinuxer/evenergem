#include "mainwindow.h"
#include "topologyselectdialog.h"
#include "ui_mainwindow.h"
#include "powertopology.h"
#include <QDebug>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QGraphicsTextItem>
#include <QListView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <cmath>
#include <qnamespace.h>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QPixmap>
#include <QSlider>
#include <QGroupBox>
#include <QDesktopServices>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <cstddef>
#include "pau_feeder.h"

static size_t factorial(ID_TYPE n)
{
    size_t res = 0;

    for (int i = 1; i < n; i++)
    {
        res += i;
    }
    return res;
}
double MainWindow::getYFromLineItemX(int nodeIndex, int nodescnt, double x, double meros)
{
    QGraphicsLineItem *lineItem = m_contactorItems[nodeIndex - 1 + nodescnt / 2];

    if (!lineItem)
        return 0;

    QLineF line = lineItem->line();
    double x1 = line.x1();
    double y1 = line.y1();
    double x2 = line.x2();
    double y2 = line.y2();

    // 处理垂直线（x1 == x2）
    if (qAbs(x2 - x1) < 1e-9 && nodeIndex > 1)
    {
        QLineF lastline = m_contactorItems[nodeIndex - 2 + nodescnt / 2]->line();
        double lastx1 = lastline.x1();
        double lasty1 = lastline.y1();
        double lastx2 = lastline.x2();
        double lasty2 = lastline.y2();
        double lastk = (lasty2 - lasty1) / (lastx2 - lastx1);
        double lasty = lastk * (x - meros - lastx1) + lasty1;
        // 垂直线，无法用y = kx + b表示
        int base = (2 * y1 + y2) / 3;                         //
        return (lasty - 2 * (lasty - base) / (nodescnt / 2)); // 返回平均值，避免y值过大
    }

    // 计算斜率
    double k = (y2 - y1) / (x2 - x1);

    // 直线方程：y - y1 = k(x - x1)
    return y1 + k * (x - x1);
}

// 辅助函数：根据背景颜色自动选择黑色或白色文字
static QColor getContrastColor(const QColor &bgColor)
{
    int brightness = qRound(0.299 * bgColor.red() + 0.587 * bgColor.green() + 0.114 * bgColor.blue());
    return brightness > 128 ? Qt::black : Qt::white;
}

class PileComboDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        if (opt.state & QStyle::State_Selected)
        {
            QColor bgColor(30, 35, 50, 240);
            opt.backgroundBrush = QBrush(bgColor);
            opt.palette.setColor(QPalette::Highlight, bgColor);
            opt.palette.setColor(QPalette::HighlightedText, QColor(240, 240, 240));
            opt.font.setPointSize(11);
            opt.font.setWeight(QFont::Bold);
        }

        painter->save();
        QStyledItemDelegate::paint(painter, opt, index);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize baseSize = QStyledItemDelegate::sizeHint(option, index);
        return QSize(baseSize.width(), qMax(baseSize.height(), 30));
    }
};

MainWindow::MainWindow(TOPOTYPE topologyType, QWidget *parent)
    : QMainWindow(parent), m_topologyType(topologyType), ui(new Ui::MainWindow), m_topology(new SimpleTopology(this)),
      m_scene(new QGraphicsScene(this)), m_selectedNode(-1), m_selectedPile(-1)
{
    ui->setupUi(this);

    QGroupBox *modeGroup = new QGroupBox("控制模式", this);
    modeGroup->setStyleSheet(
        "QGroupBox {"
        "    font-weight: bold;"
        "    border: 1px solid #2a82da;"
        "    border-radius: 8px;"
        "    margin-top: 12px;"
        "    padding-top: 8px;"
        "    background-color: rgba(30, 35, 50, 100);"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 12px;"
        "    padding: 0 6px 0 6px;"
        "    color: #a3ccf5;"
        "}");
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    modeLayout->setContentsMargins(10, 10, 10, 10);
    modeLayout->setSpacing(12);

    QLabel *manualLabel = new QLabel("本地演示", modeGroup);
    manualLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    QLabel *remoteLabel = new QLabel("远控同步", modeGroup);
    remoteLabel->setStyleSheet("QLabel { color: #e0e0e0; }");

    m_modeSlider = new QSlider(Qt::Horizontal, modeGroup);
    m_modeSlider->setRange(0, 1);
    m_modeSlider->setValue(0);
    m_modeSlider->setFixedWidth(140);
    m_modeSlider->setStyleSheet(
        "QSlider::handle:horizontal {"
        "    background-color: #2a82da;"
        "    width: 12px;"
        "    margin: -4px 0;"
        "    border-radius: 6px;"
        "}"
        "QSlider::groove:horizontal {"
        "    height: 4px;"
        "    background-color: #555;"
        "    border-radius: 2px;"
        "}");

    modeLayout->addStretch(1);
    modeLayout->addWidget(manualLabel);
    modeLayout->addWidget(m_modeSlider);
    modeLayout->addWidget(remoteLabel);
    modeLayout->addStretch(1);

    // 将分组框插入右侧布局顶部
    QVBoxLayout *rightLayout = ui->verticalLayout;
    rightLayout->insertWidget(0, modeGroup);
    // ========== 模式分组框结束 ==========

    // 初始化其他成员
    m_remoteMode = false;
    m_telnetClient = new TelnetClient("127.0.0.1", 19021, this);
    m_logWindow = nullptr;
    ui->horizontalLayout->setStretch(0, 7);
    ui->horizontalLayout->setStretch(1, 3);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);

    // 默认配置
    ui->nodeCountSpinBox->setValue(20);
    ui->pileCountSpinBox->setValue(10);
    ui->unitPowerSpinBox->setValue(40);

    // 设置节点列表
    ui->nodeListWidget->clear();
    ui->nodeListWidget->addItem("点击节点选择");
    for (int i = 1; i <= ui->nodeCountSpinBox->value(); i++)
    {
        ui->nodeListWidget->addItem(QString("节点 %1").arg(i));
    }

    // 连接信号槽
    connect(ui->applyConfigButton, &QPushButton::clicked, this, &MainWindow::onApplyConfigClicked);
    connect(ui->requestButton, &QPushButton::clicked, this, &MainWindow::onRequestPowerClicked);
    connect(ui->releaseButton, &QPushButton::clicked, this, &MainWindow::onReleasePowerClicked);
    connect(ui->pileComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPileSelectionChanged);
    connect(ui->prioritySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onPriorityChanged);
    connect(ui->nodeListWidget, &QListWidget::currentRowChanged,
            [this](int row)
            { m_selectedNode = row; });
    connect(ui->allocateNodeButton, &QPushButton::clicked, this, &MainWindow::onAllocateNodeClicked);
    connect(ui->releaseNodeButton, &QPushButton::clicked, this, &MainWindow::onReleaseNodeClicked);
    connect(m_topology, &SimpleTopology::topologyChanged, this, &MainWindow::onTopologyChanged);
    connect(ui->saveStateButton, &QPushButton::clicked, this, &MainWindow::onSaveStateClicked);
    connect(ui->loadStateButton, &QPushButton::clicked, this, &MainWindow::onLoadStateClicked);
    connect(ui->stopChargeButton, &QPushButton::clicked, this, &MainWindow::onStopChargeClicked);
    connect(ui->toggleNodeEnableButton, &QPushButton::clicked, this, &MainWindow::onToggleNodeEnableClicked);
    connect(m_modeSlider, &QSlider::valueChanged, this, &MainWindow::onModeSliderChanged);

    onApplyConfigClicked();

    // 样式表与背景（原样保留）
    QPalette palette;
    QLinearGradient gradient(0, 0, 0, 800);
    gradient.setColorAt(0, QColor(10, 10, 20));
    gradient.setColorAt(1, QColor(20, 25, 40));
    QBrush brush(gradient);
    palette.setBrush(QPalette::Window, brush);
    this->setPalette(palette);

    QString styleSheet = R"(
    QWidget {
        font-family: 'Segoe UI', 'Microsoft YaHei UI';
        font-size: 11pt;
    }
    QPushButton {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 #2a82da, stop:1 #1e5ca6);
        border: 1px solid #1e5ca6;
        border-radius: 6px;
        color: white;
        padding: 8px 16px;
        font-weight: bold;
        text-align: center;
    }
    QPushButton:hover {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 #3a92ea, stop:1 #2a6cb6);
        border: 1px solid #2a82da;
    }
    QPushButton:pressed {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                          stop:0 #1a72ca, stop:1 #0e4c96);
    }
    QSpinBox, QDoubleSpinBox, QComboBox {
        background-color: #1a1a2e;
        border: 1px solid #2a82da;
        border-radius: 4px;
        color: #00e0ff;
        padding: 4px;
        selection-background-color: #2a82da;
    }
    QListWidget {
        background-color: rgba(20, 25, 40, 180);
        border: 1px solid #2a82da;
        border-radius: 4px;
        color: #e0e0ff;
        alternate-background-color: rgba(30, 35, 50, 180);
    }
    QTextEdit, QPlainTextEdit {
        background-color: rgba(15, 20, 35, 200);
        border: 1px solid #2a82da;
        border-radius: 4px;
        color: #a0e0ff;
        selection-background-color: #2a82da;
    }
    QMessageBox {
        color: #415b96;
    }
    QMessageBox QLabel {
        color: #b6e1e9;
    }
    QLabel {
        color: #a3ccf5;
        font-weight: bold;
    }
    QLabel[label_1], QLabel[label_4], QLabel[label_5], QLabel[label_6],
    QLabel[label_7], QLabel[label_8], QLabel[label_9] {
        color: #4574bb;
        font-size: 13pt;
        padding: 5px;
        border-bottom: 2px solid #2a82da;
        margin-top: 10px;
    }
    QLabel[label_2], QLabel[label_3], QLabel[label_unitPower] {
        color: #657eee;
        font-size: 12pt;
        font-weight: bold;
    }
    QScrollBar:vertical {
        background: rgba(30, 35, 50, 150);
        width: 12px;
        border-radius: 6px;
    }
    QScrollBar::handle:vertical {
        background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                   stop:0 #2a82da, stop:1 #00e0ff);
        border-radius: 6px;
        min-height: 20px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }
    QMessageBox QPushButton {
        min-width: 40px;
        max-width: 60px;
        min-height: 12px;
        max-height: 14px;
        padding: 4px 8px;
        font-size: 9pt;
    }
    )";
    this->setStyleSheet(styleSheet);

    QMenuBar *menuBar = new QMenuBar(this);
    QMenu *helpMenu = menuBar->addMenu(tr("帮助(&H)"));
    QAction *aboutAction = helpMenu->addAction(tr("关于(&A)"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    QAction *guideAction = helpMenu->addAction(tr("说明书(&G)"));
    connect(guideAction, &QAction::triggered, this, &MainWindow::onHelpGuideTriggered);
    setMenuBar(menuBar);

    // Telnet 客户端初始化（已在构造函数开头创建）
    connect(m_telnetClient, &TelnetClient::connected, this, &MainWindow::onTelnetConnected);
    connect(m_telnetClient, &TelnetClient::disconnected, this, &MainWindow::onTelnetDisconnected);
    connect(m_telnetClient, &TelnetClient::rawLogReceived, this, &MainWindow::onTelnetRawLog);
    connect(m_telnetClient, &TelnetClient::topologyStateReceived,
            this, &MainWindow::onExternalTopologyState);
}
MainWindow::~MainWindow()
{
    if (m_telnetClient)
        m_telnetClient->stop();
    delete ui;
}

void MainWindow::onApplyConfigClicked()
{
    int nodeCount = ui->nodeCountSpinBox->value();
    int pileCount = ui->pileCountSpinBox->value();
    int unitPower = ui->unitPowerSpinBox->value() * 10;
    (void)oprt_ratedpwr_per_module(unitPower);
    ui->powerSpinBox->setValue(ui->unitPowerSpinBox->value());
    // 验证配置
    if (nodeCount % 2 != 0)
    {
        QMessageBox::warning(this, "配置错误", "节点数量必须是偶数");
        ui->nodeCountSpinBox->setValue(nodeCount + 1);
        nodeCount = nodeCount + 1; // 更新变量
    }

    if (pileCount <= 0)
    {
        QMessageBox::warning(this, "配置错误", "充电桩数量必须大于0");
        return;
    }
    if (unitPower <= 0 || unitPower > 2000)
    {
        QMessageBox::warning(this, "配置错误", "单桩功率必须大于0kW 小于200kW");
        return;
    }
    ui->nodeListWidget->clear();
    ui->nodeListWidget->addItem("点击节点选择");
    for (int i = 1; i <= nodeCount; i++)
    {
        ui->nodeListWidget->addItem(QString("节点 %1").arg(i));
    }
    // 确保充电桩数量不超过节点数量
    if (pileCount > nodeCount)
    {
        pileCount = nodeCount;
        ui->pileCountSpinBox->setValue(pileCount);
        QMessageBox::information(this, "配置调整",
                                 QString("充电桩数量已调整为节点数量: %1").arg(pileCount));
    }

    // 创建配置
    TopologyConfig config;
    config.topotype = m_topologyType;
    config.nodeCount = nodeCount;
    config.pileCount = pileCount;
    config.unitPower = unitPower;
    config.circleRadius = 200.0;
    config.center = QPointF(300, 300);
    if (SemiHybrid == m_topologyType) // 如果是半矩阵半环形
    {
        config.circleRadius = 200.0;
        config.center = QPointF(300, 500);
    }
    (void)::database_building(m_topologyType, nodeCount, pileCount);
    for (int n = 1; n <= pileCount; n++)
    {
        clear_publish_outcomes(n);
    }
    // 初始化拓扑
    m_topology->initialize(config);

    // 更新UI
    updatePileComboBox();
    setupGraphicsScene(); // 重新设置场景
    onTopologyChanged();

    ui->logTextEdit->append(QString("✓ 配置已应用: %1节点, %2充电桩").arg(nodeCount).arg(pileCount));
}
void MainWindow::onRequestPowerClicked()
{
    int pileId = ui->pileComboBox->currentIndex() + 1;
    int power = ui->powerSpinBox->value() * 10;
    int priority = ui->prioritySpinBox->value();

    if (pileId < 1 || pileId > m_topology->getChargingPiles().size())
    {
        QMessageBox::warning(this, "错误", "请选择有效的充电桩");
        return;
    }

    // 设置优先级
    QMetaObject::invokeMethod(m_topology, "setPilePriority",
                              Qt::QueuedConnection,
                              Q_ARG(int, pileId),
                              Q_ARG(int, priority));

    // 调用算法接口
    bool success = m_topology->requestPower(pileId, power);

    if (success)
    {
        ui->logTextEdit->append(QString("✓ 充电桩%1 (优先级%2) 请求 %3kW 功率成功").arg(pileId).arg(priority).arg(power / 10.0, 0, 'f', 1));
    }
    else
    {
        ui->logTextEdit->append(QString("✗ 充电桩%1 (优先级%2) 功率请求失败").arg(pileId).arg(priority));
    }
}

void MainWindow::onReleasePowerClicked()
{
    int pileId = ui->pileComboBox->currentIndex() + 1;
    int power = ui->powerSpinBox->value() * 10;

    if (pileId < 1 || pileId > m_topology->getChargingPiles().size())
    {
        QMessageBox::warning(this, "错误", "请选择有效的充电桩");
        return;
    }

    // 调用算法接口（后续实现）
    m_topology->releasePower(pileId, power);

    ui->logTextEdit->append(QString("→ 充电桩%1 释放 %2kW 功率").arg(pileId).arg(power / 10.0, 0, 'f', 1));
}

void MainWindow::onStopChargeClicked()
{
    int pileId = ui->pileComboBox->currentIndex() + 1;
    if (pileId < 1 || pileId > m_topology->getChargingPiles().size())
    {
        QMessageBox::warning(this, "错误", "请选择有效的充电桩");
        return;
    }
    const auto &piles = m_topology->getChargingPiles();
    if (piles[pileId - 1].pau_data->state != PLUG_CHARGING)
    {
        QMessageBox::warning(this, "错误", "请选择正在充电的充电桩");
        return;
    }

    m_topology->stopCharging(pileId);
    ui->logTextEdit->append(QString("→ 充电桩%1 已结束充电").arg(pileId));
}
void MainWindow::onPileSelectionChanged(int index)
{
    if (index < 0)
        return;

    m_selectedPile = index + 1;

    // 显示充电桩信息
    const auto &piles = m_topology->getChargingPiles();
    if (index < piles.size())
    {
        const auto &pile = piles[index];
        ui->pileInfoTextEdit->setPlainText(
            QString("充电桩 %1\n"
                    "状态: %2\n"
                    "直连节点: %3\n"
                    "需求功率: %4kW\n"
                    "需求节点数: %5\n"
                    "占用节点数: %6\n"
                    "优先级: %7") // 添加优先级显示
                .arg(pile.id)
                .arg(pile.pau_data->state == PLUG_CHARGING ? "充电中" : "空闲")
                .arg(pile.pau_data->connectedNode)
                .arg(pile.pau_data->requiredPower / 10.0, 0, 'f', 1)
                .arg(pile.requiredNodes)
                .arg(pile.pau_data->allocatedNodes->size)
                .arg(pile.pau_data->priority));

        // 同步优先级选择框的值
        ui->prioritySpinBox->setValue(pile.pau_data->priority);
    }
}
void MainWindow::onPriorityChanged()
{
    int priority = ui->prioritySpinBox->value();

    if (m_selectedPile > 0)
    {
        // 调用拓扑类的方法设置优先级
        QMetaObject::invokeMethod(m_topology, "setPilePriority",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, m_selectedPile),
                                  Q_ARG(int, priority));

        ui->logTextEdit->append(QString("→ 充电桩%1优先级更新为%2").arg(m_selectedPile).arg(priority));
    }
}
void MainWindow::onTopologyChanged()
{
    updateGraphics();
    updateStatusDisplay();
    // 获取当前pileselect的索引
    int index = ui->pileComboBox->currentIndex();
    onPileSelectionChanged(index);
}

void MainWindow::onAllocateNodeClicked()
{
    if (m_selectedNode <= 0 || m_selectedPile <= 0)
    {
        QMessageBox::warning(this, "错误", "请先选择节点和充电桩");
        return;
    }

    // 手动分配节点（测试用）
    m_topology->allocateNodes_manu(m_selectedNode, m_selectedPile);
    ui->logTextEdit->append(QString("→ 手动分配: 节点%1 -> 充电桩%2").arg(m_selectedNode).arg(m_selectedPile));
}

void MainWindow::onReleaseNodeClicked()
{
    if (m_selectedNode <= 0)
    {
        QMessageBox::warning(this, "错误", "请先选择节点");
        return;
    }

    // 手动释放节点（测试用）
    m_topology->releaseNodes_manu(m_selectedNode);
    ui->logTextEdit->append(QString("→ 手动释放: 节点%1").arg(m_selectedNode));
}

void MainWindow::onSaveStateClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("保存工况"), QString(),
                                                    tr("工况文件 (*.json);;所有文件 (*)"));
    if (fileName.isEmpty())
        return;

    QJsonObject state = m_topology->saveState();
    QJsonDocument doc(state);
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "错误", "无法写入文件");
        return;
    }
    file.write(doc.toJson());
    file.close();
    ui->logTextEdit->append(QString("✓ 工况已保存至 %1").arg(fileName));
}
void MainWindow::onLoadStateClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("加载工况"), QString(),
                                                    tr("工况文件 (*.json);;所有文件 (*)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "错误", "无法读取文件");
        return;
    }
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
    {
        QMessageBox::warning(this, "错误", "无效的JSON文件");
        return;
    }
    onApplyConfigClicked();
    bool ok = m_topology->loadState(doc.object());
    if (ok)
    {
        ui->logTextEdit->append(QString("✓ 已从 %1 加载工况").arg(fileName));
        // 刷新UI显示
        updatePileComboBox();
        onTopologyChanged(); // 会刷新图形和状态文本
    }
    else
    {
        QMessageBox::warning(this, "错误", "工况加载失败：配置不匹配或数据损坏");
    }
}
void MainWindow::setupGraphicsScene()
{
    m_scene->clear();
    QBrush backgroundBrush(QColor(15, 20, 35));
    m_scene->setBackgroundBrush(backgroundBrush);

    m_nodeItems.clear();
    m_contactorItems.clear();
    m_pileItems.clear();
    m_pileConnections.clear();
    m_pileLabelItems.clear();           // 状态标签
    m_nodeLabelItems.clear();           // 节点编号标签
    m_pileIdLabelItems.clear();         // 充电桩ID标签
    m_koinonItems.clear();              // 矩阵和线环之间的线
    m_semiMatrixContactorItems.clear(); // 半矩阵接触器
    m_semiMatrixJointItems.clear();     // 半矩阵接触器相交点
    m_semiMatrixBusItems.clear();       // 半矩阵母线
    m_matrixNodeItems.clear();          // 矩阵节点
    m_jointItems.clear();               // 矩阵节点与对角线交点

    const auto &config = m_topology->getConfig();
    const auto &nodes = m_topology->getNodes();
    const auto &contactors = m_topology->getContactors();
    const auto &piles = m_topology->getChargingPiles();

    // 创建节点图形项
    m_nodeItems.resize(nodes.size());
    m_nodeLabelItems.resize(nodes.size());
    for (int i = 0; i < nodes.size(); i++)
    {
        const auto &node = nodes[i];
        QPointF pos = calculateNodePosition(node.id);

        QGraphicsEllipseItem *item = new QGraphicsEllipseItem(-12, -12, 24, 24);
        item->setPos(pos);
        item->setBrush(Qt::lightGray);
        item->setPen(QPen(QColor(15, 20, 35), 1, Qt::SolidLine, Qt::RoundCap));
        item->setData(0, node.id); // 存储节点ID
        item->setToolTip(QString("节点容量 %1kW").arg(node.pau_data->power_available / 10.0, 0, 'f', 1));
        item->setAcceptHoverEvents(true);
        item->setZValue(98);
        m_scene->addItem(item);
        m_nodeItems[i] = item;

        // 节点编号标签
        QGraphicsTextItem *label = new QGraphicsTextItem(QString::number(node.id));
        label->setDefaultTextColor(Qt::white); // 临时颜色，后续动态调整
        label->setFont(QFont("Arial", 8, QFont::Bold));
        label->setZValue(99);
        QRectF labelRect = label->boundingRect();
        label->setPos(pos.x() - labelRect.width() / 2.0, pos.y() - labelRect.height() / 2.0);
        m_scene->addItem(label);
        m_nodeLabelItems[i] = label;
    }

    // 创建环形接触器图形项
    m_contactorItems.resize(2 * nodes.size());
    for (int i = 0; i < 2 * nodes.size(); i++)
    {
        const auto &contactor = contactors[i];

        // 检查节点ID是否有效
        if (contactor.pau_data->node1 < 1 || contactor.pau_data->node1 > NODE_MAX ||
            contactor.pau_data->node2 < 1 || contactor.pau_data->node2 > NODE_MAX * CONTACTOR_SPLICE_MULTIPLE)
        {
            qWarning() << "无效的接触器节点:" << contactor.id << contactor.pau_data->node1 << "-" << contactor.pau_data->node2;
            continue;
        }

        QPointF pos1 = calculateNodePosition(contactor.pau_data->node1);
        QPointF pos2 = calculateNodePosition(contactor.pau_data->node2);
        QGraphicsLineItem *line;
        if (i < config.nodeCount)
        {
            line = new QGraphicsLineItem(pos1.x(), pos1.y(), pos2.x(), pos2.y());
        }
        else
        {
            line = new QGraphicsLineItem(pos1.x(), pos1.y(), config.center.x(), config.center.y());
        }

        // 前一半是环形接触器 <gray>，后一半是对角线接触器<darkgray>
        if (i < config.nodeCount)
        {
            line->setPen(QPen(Qt::gray, 2, Qt::DashLine)); // 环形接触器
            line->setToolTip(QString("接触器编号 %1, 1%2")
                                 .arg(contactor.pau_data->id)
                                 .arg(i + 1, 2, 10, QChar('0')));
        }
        else
        {
            line->setPen(QPen(Qt::darkGray, 2, Qt::DotLine)); // 对角线接触器
            line->setToolTip(QString("接触器编号 %1, 2%2")
                                 .arg(contactor.pau_data->id)
                                 .arg(i + 1 - config.nodeCount, 2, 10, QChar('0')));
        }

        if (SemiHybrid != m_topologyType || i < config.nodeCount)
        {
            m_scene->addItem(line);
        }
        m_contactorItems[i] = line;
    }
    // 创建矩阵节点和对角线交点
    if (SemiHybrid == m_topologyType)
    {
        int matrix_contactors_num = ::factorial(config.nodeCount / 2);
        m_koinonItems.resize(config.nodeCount);                   // 矩阵和线环之间的线
        m_semiMatrixContactorItems.resize(matrix_contactors_num); // 半矩阵接触器
        m_semiMatrixJointItems.resize(matrix_contactors_num);     // 半矩阵接触器相交点
        m_semiMatrixBusItems.resize(config.nodeCount / 2);        // 半矩阵母线
        m_matrixNodeItems.resize(config.nodeCount);               // 矩阵节点
        m_jointItems.resize(config.nodeCount);                    // 矩阵节点与对角线交点

        for (int i = 1; i <= config.nodeCount / 2; i++)
        {
            QGraphicsEllipseItem *circle = new QGraphicsEllipseItem(QRectF(-5, -5, 10, 10));
            circle->setPos(calculateJointPosition(config.nodeCount + i));
            circle->setBrush(QBrush(Qt::darkGray));
            circle->setZValue(99);
            m_scene->addItem(circle);
            m_jointItems[i - 1] = circle;
            // 根据m_jointItems修改环形接触器的起始点
            QLineF lf = m_contactorItems[3 * config.nodeCount / 2 + i - 1]->line();
            lf.setP2(QPointF(m_jointItems[i - 1]->x(), m_jointItems[i - 1]->y()));
            m_contactorItems[3 * config.nodeCount / 2 + i - 1]->setLine(lf);
            m_contactorItems[3 * config.nodeCount / 2 + i - 1]->setPen(QPen(Qt::darkGray, 2, Qt::DashLine));
            lf = m_contactorItems[config.nodeCount + i - 1]->line();
            lf.setP2(QPointF(m_jointItems[i - 1]->x(), m_jointItems[i - 1]->y()));
            m_contactorItems[config.nodeCount + i - 1]->setLine(lf);
            m_contactorItems[config.nodeCount + i - 1]->setPen(QPen(Qt::darkGray, 2, Qt::SolidLine));
            m_scene->addItem(m_contactorItems[3 * config.nodeCount / 2 + i - 1]);
            m_scene->addItem(m_contactorItems[config.nodeCount + i - 1]);
            //  打印接触器起始位置
            // qDebug() << "接触器" << (3 * config.nodeCount / 2 + i - 1) << "起始位置:" << m_contactorItems[3 * config.nodeCount / 2 + i - 1]->line().p1() << "终止位置:" << m_contactorItems[3 * config.nodeCount / 2 + i - 1]->line().p2();
            // qDebug() << "接触器" << (config.nodeCount + i - 1) << "起始位置:" << m_contactorItems[config.nodeCount + i - 1]->line().p1() << "终止位置:" << m_contactorItems[config.nodeCount + i - 1]->line().p2();
        }
        // 创建半矩阵节点图形项
        double meros = (config.circleRadius + 25) / (config.nodeCount / 2 + 1);
        for (int i = 0; i < config.nodeCount / 2; i++)
        {
            QPointF pos = QPointF(m_jointItems[i]->x(), meros + i * meros);
            QGraphicsRectItem *item = new QGraphicsRectItem(-12, -8, 24, 16);
            item->setPos(pos);
            item->setBrush(Qt::lightGray);
            item->setPen(QPen(QColor(15, 20, 35), 1, Qt::SolidLine, Qt::RoundCap));
            item->setZValue(98);
            m_scene->addItem(item);
            m_matrixNodeItems[i] = item;
            QGraphicsTextItem *label = new QGraphicsTextItem(QString::number(config.nodeCount + i + 1));
            label->setPos(pos.x() - 10, pos.y() - 11);
            label->setDefaultTextColor(Qt::black); // 临时颜色，后续动态调整
            label->setFont(QFont("Arial", 8, QFont::Bold));
            label->setZValue(99);
            m_scene->addItem(label);
        }
        for (int i = 0; i < config.nodeCount / 2; i++)
        {
            const auto &contactor = contactors[i + matrix_contactors_num + 2 * config.nodeCount];
            QGraphicsLineItem *connLine = new QGraphicsLineItem(
                m_matrixNodeItems[i]->x(), m_matrixNodeItems[i]->y(), m_jointItems[i]->x(), m_jointItems[i]->y());
            QLinearGradient grad(m_matrixNodeItems[i]->x(), m_matrixNodeItems[i]->y(), m_jointItems[i]->x(), m_jointItems[i]->y());
            grad.setColorAt(0, QColor(200, 200, 200, 255)); // 起点：完全不透明
            grad.setColorAt(1, QColor(200, 200, 200, 0));   // 终点：完全透明

            QPen pen;
            pen.setStyle(Qt::SolidLine);
            // pen.setDashOffset(20);
            // pen.setDashPattern({3, 5});
            // pen.setCapStyle(Qt::RoundCap);
            pen.setBrush(grad);
            pen.setWidth(2); // 粗线才能看清渐变
            pen.setCapStyle(Qt::RoundCap);
            connLine->setPen(pen);
            connLine->setToolTip(QString("接触器编号 %1, 4%2")
                                     .arg(i + matrix_contactors_num + 2 * config.nodeCount + 1)
                                     .arg(i + 1, 2, 10, QChar('0')));

            m_scene->addItem(connLine);
            m_koinonItems[i] = connLine;
        }
        for (int i = 0; i < config.nodeCount / 2; i++)
        {
            QGraphicsLineItem *bus = new QGraphicsLineItem(
                m_matrixNodeItems[i]->x(), m_matrixNodeItems[0]->y() - 2, m_matrixNodeItems[i]->x(), m_matrixNodeItems[i]->y());
            bus->setPen(QPen(Qt::gray, 3, Qt::SolidLine, Qt::RoundCap));
            // 阴影投影线，实现凸起
            QGraphicsLineItem *busShadow = new QGraphicsLineItem(
                bus->line().x1() + 2, bus->line().y1() + 2,
                bus->line().x2() + 2, bus->line().y2() + 2);
            busShadow->setPen(QPen(QColor(30, 30, 30, 120), 2, Qt::SolidLine, Qt::RoundCap));
            busShadow->setZValue(-1);
            busShadow->setParentItem(bus);
            m_scene->addItem(bus);
            m_semiMatrixBusItems[i] = bus;
        }

        int contactorIdx = 0;
        for (int node1 = 1; node1 < config.nodeCount / 2 && contactorIdx < matrix_contactors_num; node1++)
        {
            for (int node2 = node1 + 1; node2 <= config.nodeCount / 2 && contactorIdx < matrix_contactors_num; node2++)
            {
                const auto &contactor = contactors[contactorIdx + 2 * config.nodeCount];
                QGraphicsLineItem *connLine = new QGraphicsLineItem(m_matrixNodeItems[node1 - 1]->x(), m_matrixNodeItems[node1 - 1]->y(), m_matrixNodeItems[node2 - 1]->x(), m_matrixNodeItems[node1 - 1]->y());
                connLine->setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
                connLine->setData(keyNode1, node1);
                connLine->setData(keyNode2, node2);
                connLine->setToolTip(QString("接触器编号 %1, 3%2")
                                         .arg(contactorIdx + 1 + 2 * config.nodeCount)
                                         .arg(contactorIdx + 1, 2, 10, QChar('0')));

                connLine->setZValue(80 - contactorIdx);
                connLine->setData(vislevel, 80 - contactorIdx);
                m_scene->addItem(connLine);
                m_semiMatrixContactorItems[contactorIdx] = connLine;

                QGraphicsRectItem *item = new QGraphicsRectItem(-4, -4, 8, 8);
                item->setPos(QPointF(m_matrixNodeItems[node2 - 1]->x(), m_matrixNodeItems[node1 - 1]->y()));
                item->setBrush(Qt::lightGray);

                item->setZValue(98);
                item->setData(keyNode1, node1);
                item->setData(keyNode2, node2);
                m_scene->addItem(item);
                m_semiMatrixJointItems[contactorIdx] = item;
                contactorIdx++;
            }
        }
    }

    // 创建充电桩图形项
    m_pileItems.resize(piles.size());
    m_pileConnections.resize(piles.size());
    m_pileIdLabelItems.resize(piles.size());
    m_pileLabelItems.resize(piles.size()); // 状态标签
    for (int i = 0; i < piles.size(); i++)
    {
        const auto &pile = piles[i];

        // 检查充电桩连接节点是否有效
        if (pile.pau_data->connectedNode < 1 || pile.pau_data->connectedNode > config.nodeCount)
        {
            qWarning() << "充电桩" << pile.id << "连接了无效的节点:" << pile.pau_data->connectedNode;
            continue;
        }

        // 充电桩位置
        QPointF pilePos = calculatePilePosition(i);

        // 充电桩图形
        QGraphicsEllipseItem *pileItem = new QGraphicsEllipseItem(-15, -15, 30, 30);
        pileItem->setPos(pilePos);
        pileItem->setBrush(pile.color);
        pileItem->setPen(QPen(QColor(15, 20, 35), 1, Qt::SolidLine, Qt::RoundCap));
        pileItem->setData(0, pile.id); // 存储充电桩ID
        m_scene->addItem(pileItem);
        m_pileItems[i] = pileItem;

        // 充电桩ID标签（P1, P2...）
        QGraphicsTextItem *idLabel = new QGraphicsTextItem(QString("P%1").arg(pile.id));
        idLabel->setPos(pilePos.x() - 12, pilePos.y() - 12);
        idLabel->setDefaultTextColor(Qt::white); // 临时颜色，后续动态调整
        idLabel->setFont(QFont("Arial", 8, QFont::Bold));
        QRectF labelRect = idLabel->boundingRect();
        idLabel->setPos(pilePos.x() - labelRect.width() / 2.0, pilePos.y() - labelRect.height() / 2.0);
        idLabel->setZValue(1);
        m_scene->addItem(idLabel);
        m_pileIdLabelItems[i] = idLabel;

        // 充电桩状态标签（显示节点数/优先级）
        QGraphicsTextItem *statusLabel = new QGraphicsTextItem();
        statusLabel->setFont(QFont("Arial", 8, QFont::Bold));
        statusLabel->setDefaultTextColor(Qt::white); // 临时颜色
        statusLabel->setZValue(1);
        m_scene->addItem(statusLabel);
        m_pileLabelItems[i] = statusLabel;

        // 亚克力框（背景框）
        QGraphicsPathItem *backgroundBox = new QGraphicsPathItem();
        int boxWidth = 70;
        int boxHeight = 40;
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, boxWidth, boxHeight), 3, 3);
        backgroundBox->setPath(path);
        backgroundBox->setPos(pilePos.x() - boxWidth / 2, pilePos.y() + boxHeight / 2);
        QBrush bgBrush(QColor(255, 255, 255, 80));
        backgroundBox->setBrush(bgBrush);
        QPen borderPen(QColor(255, 255, 255, 150), 1);
        backgroundBox->setPen(borderPen);
        m_scene->addItem(backgroundBox);

        // 连接线
        QPointF nodePos = calculateNodePosition(pile.pau_data->connectedNode);
        QGraphicsLineItem *connLine = new QGraphicsLineItem(
            nodePos.x(), nodePos.y(), pilePos.x(), pilePos.y());
        connLine->setPen(QPen(Qt::lightGray, 2, Qt::DashDotLine));
        m_scene->addItem(connLine);
        m_pileConnections[i] = connLine;
    }
}

void MainWindow::updateGraphics()
{
    const auto &nodes = m_topology->getNodes();
    const auto &contactors = m_topology->getContactors();
    const auto &piles = m_topology->getChargingPiles();
    const auto &matrixnodes = m_topology->getMatrixNodes();
    const auto &config = m_topology->getConfig();

    // 更新节点颜色
    for (int i = 0; i < nodes.size() && i < m_nodeItems.size(); i++)
    {
        const auto &node = nodes[i];
        QBrush brush = Qt::lightGray;

        if (node.pau_data->state == NODE_OCCUPIED && node.pau_data->plug_id > 0)
        {
            // 根据充电桩颜色着色
            int chargerIndex = node.pau_data->plug_id - 1;
            if (chargerIndex >= 0 && chargerIndex < piles.size())
            {
                brush = piles[chargerIndex].color;
            }
        }

        else if (node.pau_data->state == NODE_DISABLED)
        {
            brush = QColor(15, 20, 34); // 深灰色
        }

        if (m_nodeItems[i])
        {
            m_nodeItems[i]->setBrush(brush);
        }
    }

    // 更新节点标签颜色（根据节点圆圈背景色）
    for (int i = 0; i < nodes.size() && i < m_nodeLabelItems.size(); i++)
    {
        if (m_nodeLabelItems[i])
        {
            QColor bgColor = m_nodeItems[i]->brush().color();
            QColor textColor = getContrastColor(bgColor);
            m_nodeLabelItems[i]->setDefaultTextColor(textColor);
        }
    }

    // 更新接触器 - 根据连接的充电桩着色
    for (int i = 0; i < contactors.size() && i < m_contactorItems.size() && i < 2 * config.nodeCount; i++)
    {
        if (m_contactorItems[i])
        {
            const auto &contactor = contactors[i];
            QPen pen;

            if (contactor.pau_data->isClosed)
            {
                // 确定使用哪个充电桩的颜色，如果两个节点连接的充电桩不同，则使用默认灰色
                int chargerId = get_contactor_pwrflow_dest(contactor.pau_data->id);

                if (chargerId > 0 && chargerId <= piles.size())
                {
                    // 使用充电桩的颜色，加粗显示
                    QColor pileColor = piles[chargerId - 1].color;
                    pen = QPen(pileColor, 2, Qt::SolidLine);
                }
                else
                {
                    // 默认灰色
                    pen = QPen(Qt::gray, 2, Qt::DashLine);
                }
            }
            else
            {
                // 未闭合的接触器显示为灰色虚线
                if (i < config.nodeCount)
                {
                    pen = QPen(Qt::gray, 2, Qt::DashLine); // 环形接触器
                }
                else
                {
                    pen = QPen(Qt::darkGray, 2, Qt::DotLine); // 对角线接触器
                }
            }
            m_contactorItems[i]->setPen(pen);
        }
    }

    // 更新充电桩连接线 - 根据充电桩状态着色
    for (int i = 0; i < piles.size() && i < m_pileConnections.size(); i++)
    {
        if (m_pileConnections[i])
        {
            const auto &pile = piles[i];

            // 如果充电桩有分配的节点，则连接线使用充电桩颜色
            if (pile.pau_data->allocatedNodes->size > 0)
            {
                m_pileConnections[i]->setPen(QPen(pile.color, 3, Qt::SolidLine));
            }
            else
            {
                m_pileConnections[i]->setPen(QPen(Qt::lightGray, 2, Qt::DashDotLine));
            }
            m_pileConnections[i]->setZValue(-1);
        }
    }

    // 更新充电桩状态文本和颜色
    for (int i = 0; i < piles.size() && i < m_pileItems.size(); i++)
    {
        const auto &pile = piles[i];
        QColor pileColor = pile.color;
        QColor textColor = getContrastColor(pileColor);

        // 更新充电桩图形颜色
        m_pileItems[i]->setBrush(pileColor);

        // 更新ID标签颜色
        if (i < m_pileIdLabelItems.size() && m_pileIdLabelItems[i])
            m_pileIdLabelItems[i]->setDefaultTextColor(textColor);

        // 更新状态标签内容和颜色
        if (i < m_pileLabelItems.size() && m_pileLabelItems[i])
        {
            if (pile.pau_data->state == PLUG_CHARGING)
            {
                // Format %3 as a float with 1 decimal place
                // Assuming requiredPower is in units of 0.1kW, so divide by 10.0
                QString labelText = QString("%1:%2 %3\n%4级")
                                        .arg(pile.pau_data->allocatedNodes->size)
                                        .arg(pile.pau_data->shortage)
                                        .arg(pile.pau_data->requiredPower / 10.0, 0, 'f', 1) // Float format, 1 decimal place
                                        .arg(pile.pau_data->priority);
                m_pileLabelItems[i]->setPlainText(labelText);
            }
            else
            {
                m_pileLabelItems[i]->setPlainText(QString("- : -\n-"));
            }
            m_pileLabelItems[i]->setDefaultTextColor(Qt::black);

            // Adjust position if necessary due to font size change
            QPointF pos = m_pileItems[i]->pos();
            m_pileLabelItems[i]->setPos(pos.x() - 35, pos.y() + 20);
        }
    }
    if (SemiHybrid == m_topologyType)
    {
        // 更新矩阵和线环之间的线
        for (int i = 0; i < config.nodeCount / 2; i++)
        {
            if (m_koinonItems[i])
            {
                int baseindex = contactors.size() - config.nodeCount / 2;
                const auto &contactor = contactors[baseindex + i];
                QColor color;

                if (contactor.pau_data->isClosed)
                {
                    // 确定使用哪个充电桩的颜色，如果两个节点连接的充电桩不同，则使用默认灰色
                    int chargerId = get_contactor_pwrflow_dest(contactor.pau_data->id);

                    if (chargerId > 0 && chargerId <= piles.size())
                    {
                        // 使用充电桩的颜色，加粗显示
                        color = piles[chargerId - 1].color;

                        m_jointItems[i]->setBrush(QBrush(color));
                    }
                    else
                    {
                        // 默认灰色
                        color = Qt::gray;
                        m_jointItems[i]->setBrush(QBrush(Qt::gray));
                    }
                }
                else
                {
                    color = Qt::gray;
                    m_jointItems[i]->setBrush(QBrush(Qt::gray));
                }
                // 重新创建渐变笔，保持从起点到终点的渐变
                QLineF line = m_koinonItems[i]->line();
                QLinearGradient grad(line.p1(), line.p2());
                grad.setColorAt(0, color);
                grad.setColorAt(1, QColor(color.red(), color.green(), color.blue(), 0)); // 终点透明

                QPen pen;
                pen.setBrush(grad);
                pen.setWidth(2);
                pen.setStyle(Qt::SolidLine);
                pen.setCapStyle(Qt::RoundCap);
                m_koinonItems[i]->setPen(pen);
            }
        }
        for (int i = 2 * config.nodeCount + 1; i <= contactors.size() - config.nodeCount / 2; i++)
        {
            if (m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1])
            {

                const auto &contactor = contactors[i - 1];
                QPen pen;
                if (contactor.pau_data->isClosed)
                {
                    // 确定使用哪个充电桩的颜色，如果两个节点连接的充电桩不同，则使用默认灰色
                    int chargerId = get_contactor_pwrflow_dest(contactor.pau_data->id);

                    if (chargerId > 0 && chargerId <= piles.size())
                    {
                        // 使用充电桩的颜色，加粗显示
                        QColor pileColor = piles[chargerId - 1].color;
                        pen = QPen(pileColor, 2, Qt::SolidLine);
                        m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setPen(pen);
                        m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setZValue(80 + 1);
                        m_semiMatrixJointItems[i - 2 * config.nodeCount - 1]->setBrush(QBrush(pen.color()));
                    }
                    else
                    {
                        // 默认灰色
                        pen = QPen(Qt::gray, 1, Qt::DotLine);
                        m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setPen(pen);
                        int level = m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->data(vislevel).toInt();
                        m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setZValue(level);
                        m_semiMatrixJointItems[i - 2 * config.nodeCount - 1]->setBrush(QBrush(pen.color()));
                    }
                }
                else
                {
                    pen = QPen(Qt::gray, 1, Qt::DotLine); //
                    m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setPen(pen);
                    int level = m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->data(vislevel).toInt();
                    m_semiMatrixContactorItems[i - 2 * config.nodeCount - 1]->setZValue(level);
                    m_semiMatrixJointItems[i - 2 * config.nodeCount - 1]->setBrush(QBrush(pen.color()));
                }
            }
        }
        for (int i = 0; i < matrixnodes.size(); i++)
        {
            const auto &node = matrixnodes[i];
            QBrush brush = Qt::lightGray;
            QColor color;
            if (node.pau_data->state == NODE_OCCUPIED && node.pau_data->plug_id > 0)
            {
                int chargerIndex = node.pau_data->plug_id - 1;
                if (chargerIndex >= 0 && chargerIndex < piles.size())
                {
                    color = piles[chargerIndex].color;
                    brush = color;
                }
                else
                {
                    color = Qt::gray;
                    brush = Qt::gray;
                }
            }
            else if (node.pau_data->state == NODE_DISABLED)
            {
                color = QColor(15, 20, 34);
                brush = color;
            }
            else
            {
                color = Qt::gray;
                brush = Qt::gray;
            }

            if (m_matrixNodeItems[i])
            {
                m_matrixNodeItems[i]->setBrush(brush);
            }

            if (m_semiMatrixBusItems[i])
            {
                // 保留原有的宽度（3）和实线样式，只修改颜色
                QPen pen = m_semiMatrixBusItems[i]->pen();
                pen.setColor(color);
                pen.setWidth(3); // 保持原始宽度
                pen.setStyle(Qt::SolidLine);
                m_semiMatrixBusItems[i]->setPen(pen);
            }
        }
    }
}

void MainWindow::updateStatusDisplay()
{
    const auto &nodes = m_topology->getNodes();
    const auto &piles = m_topology->getChargingPiles();

    QString statusText;

    // 充电桩状态
    statusText += "=== 充电桩状态 ===\n";
    for (const auto &pile : piles)
    {
        QString nodeList;
        QList<int> allocated = get_plug_allocated_nodes(pile.id);
        std::sort(allocated.begin(), allocated.end());

        for (int nodeId : allocated)
        {
            nodeList += QString::number(nodeId) + " ";
        }

        statusText += QString("桩%1: %2/%3 [%4]\n")
                          .arg(pile.id)
                          .arg(allocated.size())
                          .arg(pile.requiredNodes)
                          .arg(nodeList.isEmpty() ? "无节点" : nodeList);
    }

    // 节点状态
    statusText += "\n=== 节点状态 ===\n";
    int occupied = 0, idle = 0, disabled = 0;
    for (const auto &node : nodes)
    {
        if (node.pau_data->state == NODE_OCCUPIED)
            occupied++;
        else if (node.pau_data->state == NODE_IDLE)
            idle++;
        else if (node.pau_data->state == NODE_DISABLED)
            disabled++;
    }
    statusText += QString("占用: %1 | 空闲: %2 | 禁用: %3 | 总数: %4\n")
                      .arg(occupied)
                      .arg(idle)
                      .arg(disabled)
                      .arg(nodes.size());

    ui->statusTextEdit->setPlainText(statusText);
}

void MainWindow::updatePileComboBox()
{
    ui->pileComboBox->clear();

    const auto &piles = m_topology->getChargingPiles();
    for (const auto &pile : piles)
    {
        QPixmap iconPixmap(20, 20);
        iconPixmap.fill(Qt::transparent);
        QPainter painter(&iconPixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(pile.color);
        painter.setPen(QPen(pile.color, 1));
        painter.drawEllipse(QRectF(1, 1, iconPixmap.width() - 2, iconPixmap.height() - 2));
        painter.end();

        QString labelText = QString("充电桩%1 (节点%2)").arg(pile.id).arg(pile.pau_data->connectedNode);
        ui->pileComboBox->addItem(QIcon(iconPixmap), labelText);
        int itemIndex = ui->pileComboBox->count() - 1;
        ui->pileComboBox->setItemData(itemIndex, QBrush(pile.color), Qt::ForegroundRole);
        ui->pileComboBox->setItemData(itemIndex, QFont("Segoe UI", 10, QFont::Bold), Qt::FontRole);
    }

    if (!piles.isEmpty())
    {
        QListView *listView = qobject_cast<QListView *>(ui->pileComboBox->view());
        if (!listView)
        {
            listView = new QListView(ui->pileComboBox);
            ui->pileComboBox->setView(listView);
        }
        listView->setSpacing(4);
        listView->setUniformItemSizes(false);
        listView->setWordWrap(true);
        listView->setTextElideMode(Qt::ElideRight);
        listView->setStyleSheet(
            "QListView { background-color: rgba(15, 20, 35, 240); color: #e0e0e0; }"
            "QListView::item { min-height: 28px; padding: 6px 10px; }");

        ui->pileComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        ui->pileComboBox->setIconSize(QSize(18, 18));
        ui->pileComboBox->setStyleSheet(
            "QComboBox { background-color: #121822; color: #e0e0e0; border: 1px solid rgba(100, 130, 180, 120); }");
        listView->setItemDelegate(new PileComboDelegate(listView));

        ui->pileComboBox->setCurrentIndex(0);
        onPileSelectionChanged(0);
    }
}

QPointF MainWindow::calculateNodePosition(int nodeId)
{
    const auto &config = m_topology->getConfig();

    double angle = 2 * M_PI * (nodeId - 1) / config.nodeCount;
    if (SemiHybrid == m_topologyType) // 如果是SemiHybrid结构
    {
        angle += M_PI / config.nodeCount;
    }
    double x = config.center.x() + config.circleRadius * cos(angle);
    double y = config.center.y() + config.circleRadius * sin(angle);

    return QPointF(x, y);
}

QPointF MainWindow::calculatePilePosition(int pileIndex)
{
    const auto &config = m_topology->getConfig();
    const auto &piles = m_topology->getChargingPiles();

    if (pileIndex < 0 || pileIndex >= piles.size())
    {
        return QPointF();
    }

    int nodeId = piles[pileIndex].pau_data->connectedNode;
    QPointF nodePos = calculateNodePosition(nodeId);
    QPointF center = config.center;

    // 计算从圆心到节点的方向向量
    QPointF direction = nodePos - center;
    double length = sqrt(direction.x() * direction.x() + direction.y() * direction.y());

    if (length > 0)
    {
        direction = direction / length;
    }

    // 在节点外侧延伸
    return nodePos + direction * 80;
}

QPointF MainWindow::calculateJointPosition(int nodeIndex)
{
    const auto &config = m_topology->getConfig();
    if (nodeIndex <= config.nodeCount || nodeIndex > 2 * config.nodeCount)
    {
        return QPointF(0, 0);
    }
    double diameter = config.circleRadius * 2;
    double meros = diameter / (config.nodeCount / 2 + 1);
    double x = config.center.x() - config.circleRadius;
    x += meros * (nodeIndex - config.nodeCount);
    double y = 0;
    y = getYFromLineItemX(nodeIndex, config.nodeCount, x, meros);
    if (y != 0)
    {
        return QPointF(x, y);
    }
    else
    {
        return QPointF(0, 0);
    }
}
void MainWindow::showAboutDialog()
{
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle(tr("关于本软件"));
    aboutDialog.setMinimumSize(500, 400);

    QVBoxLayout *layout = new QVBoxLayout(&aboutDialog);

    // 软件图标（可选，可放置一个 QLabel 显示图片）
    QLabel *iconLabel = new QLabel();
    iconLabel->setPixmap(QPixmap(":/icon.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // 软件名称和版本
    QLabel *titleLabel = new QLabel(tr("<h2>evenergem</h2>"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QLabel *versionLabel = new QLabel(tr("版本：1.0.1"));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    // 版权信息
    QLabel *copyrightLabel = new QLabel(tr("© 2026 袁博"));
    copyrightLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyrightLabel);

    // 分隔线
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    // 描述文字
    QLabel *descLabel = new QLabel(tr(
        "\n直流充电桩环形拓扑功率分配演示系统\n"
        "用于教学、开发思路验证及图论算法研究\n\n"
        "本软件仅供非商业用途使用，不作为开发者所在公司售卖产品\n详情见免责声明"));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // 开发者联系方式
    QLabel *contactLabel = new QLabel(tr(
        ""));
    contactLabel->setOpenExternalLinks(true);
    contactLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(contactLabel);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(tr("关闭"));
    connect(closeBtn, &QPushButton::clicked, &aboutDialog, &QDialog::accept);
    layout->addWidget(closeBtn, 0, Qt::AlignCenter);

    aboutDialog.exec();
}
void MainWindow::onHelpGuideTriggered()
{
    // 资源路径
    QString resourcePath = ":manual.pdf";

    // 检查资源是否存在
    QFile resFile(resourcePath);
    if (!resFile.exists())
    {
        QMessageBox::warning(this, tr("错误"), tr("说明书文件未找到"));
        return;
    }

    // 复制到临时目录（保留 .pdf 扩展名以便系统正确关联）
    QString tempFilePath = QDir::temp().absoluteFilePath("evenergem_manual.pdf");

    // 如果已有旧文件，先删除
    if (QFile::exists(tempFilePath))
    {
        QFile::remove(tempFilePath);
    }

    // 复制资源到临时文件
    if (!QFile::copy(resourcePath, tempFilePath))
    {
        QMessageBox::warning(this, tr("错误"), tr("无法复制说明书文件"));
        return;
    }

    // 设置文件权限（确保可读）
    QFile::setPermissions(tempFilePath, QFileDevice::ReadUser | QFileDevice::WriteUser);

    // 使用系统默认程序打开 PDF
    bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(tempFilePath));
    if (!success)
    {
        QMessageBox::warning(this, tr("错误"), tr("无法打开 PDF 文件，请确保已安装 PDF 阅读器"));
    }
}
void MainWindow::onToggleNodeEnableClicked()
{
    if (m_selectedNode <= 0)
    {
        QMessageBox::warning(this, "错误", "请先选择一个节点");
        return;
    }

    bool enabled = m_topology->toggleNodeEnabled(m_selectedNode);
    if (enabled)
    {
        ui->logTextEdit->append(QString("→ 节点%1 已启用").arg(m_selectedNode));
    }
    else
    {
        ui->logTextEdit->append(QString("→ 节点%1 已禁用").arg(m_selectedNode));
    }
    // 刷新图形显示
    onTopologyChanged();
}

void MainWindow::onTelnetConnected()
{
    if (!m_remoteMode)
    {
        m_telnetClient->stop();
        return;
    }
    if (!m_logWindow)
    {
        m_logWindow = new LogWindow(this);
        m_logWindow->show();
    }
    // 禁用右侧所有控制按钮
    m_remoteControlMode = true;
    ui->applyConfigButton->setEnabled(false);
    ui->nodeCountSpinBox->setEnabled(false);
    ui->pileCountSpinBox->setEnabled(false);
    ui->unitPowerSpinBox->setEnabled(false);
    ui->requestButton->setEnabled(false);
    ui->releaseButton->setEnabled(false);
    ui->stopChargeButton->setEnabled(false);
    ui->prioritySpinBox->setEnabled(false);
    ui->allocateNodeButton->setEnabled(false);
    ui->releaseNodeButton->setEnabled(false);
    ui->toggleNodeEnableButton->setEnabled(false);
    ui->saveStateButton->setEnabled(false);
    ui->loadStateButton->setEnabled(false);
    ui->powerSpinBox->setEnabled(false);
    ui->pileComboBox->setEnabled(false);
    ui->nodeListWidget->setEnabled(false);
    ui->logTextEdit->append(QString("✓ 已连接到远程服务器，已切换到远程控制模式"));
}

void MainWindow::onTelnetDisconnected()
{
    if (m_logWindow)
    {
        m_logWindow->appendLog("--- 与远程服务器断开连接 ---");
    }
    // 可选：是否重新启用本地按钮，按需求不启用，保持灰锁。但是为了体验，可以再连接成功后再次禁用即可，断开不再恢复。
}

void MainWindow::onTelnetRawLog(const QString &log)
{
    if (m_logWindow)
    {
        m_logWindow->appendLog(log);
    }
}

void MainWindow::onExternalTopologyState(int nodeCount, int pileCount,
                                         const QVector<int> &nodeOwners,
                                         const QVector<bool> &contactorStates,
                                         const QMap<int, QPair<int, int>> &chargingPiles)
{
    // 校验与当前配置是否一致
    const TopologyConfig &cfg = m_topology->getConfig();
    if (nodeCount != cfg.nodeCount || pileCount != cfg.pileCount)
    {
        QString err = QString("外部状态不匹配: 期望 (%1,%2) 实际 (%3,%4)")
                          .arg(cfg.nodeCount)
                          .arg(cfg.pileCount)
                          .arg(nodeCount)
                          .arg(pileCount);
        qWarning() << err;
        if (m_logWindow)
            m_logWindow->appendLog(err);
        return;
    }

    // 先清除所有现有分配
    for (auto &pile : m_topology->getChargingPiles())
    {
        ::releasePower(pile.id, 0);
    }
    // 重置所有节点为空闲
    for (int i = 0; i < cfg.nodeCount; ++i)
    {
        PowerNode &node = const_cast<PowerNode &>(m_topology->getNodes()[i]);
        node.pau_data->state = NODE_IDLE;
        node.pau_data->plug_id = 0;
    }

    // 根据 nodeOwners 重新分配节点
    for (int i = 0; i < nodeOwners.size(); ++i)
    {
        int nodeId = i + 1;
        int pileId = nodeOwners[i];
        if (pileId >= 1 && pileId <= pileCount)
        {
            // 分配节点给桩，但不发射信号（批量完成后统一刷新）
            m_topology->allocateNodeToPile(nodeId, pileId, false);
        }
    }

    // 更新充电桩需求功率和优先级以及状态
    for (auto it = chargingPiles.begin(); it != chargingPiles.end(); ++it)
    {
        int pileId = it.key();
        int requiredPower = it.value().first;
        int priority = it.value().second;
        if (pileId >= 1 && pileId <= pileCount)
        {
            ChargingPile &pile = const_cast<ChargingPile &>(m_topology->getChargingPiles()[pileId - 1]);
            pile.pau_data->requiredPower = requiredPower;
            pile.requiredNodes = (requiredPower + cfg.unitPower - 1) / cfg.unitPower;
            pile.pau_data->priority = (PRIOR)priority;
            pile.pau_data->state = (requiredPower > 0) ? PLUG_CHARGING : PLUG_IDLE;
            // 清理 disabledNodes（外部状态没有禁用概念，清空）
            pau_vector_clear(pile.pau_data->disabledNodes);
        }
    }
    // 对于未出现在 chargingPiles 中的充电桩，设为空闲
    for (int i = 0; i < pileCount; ++i)
    {
        int pileId = i + 1;
        if (!chargingPiles.contains(pileId))
        {
            ChargingPile &pile = const_cast<ChargingPile &>(m_topology->getChargingPiles()[i]);
            pile.pau_data->requiredPower = 0;
            pile.requiredNodes = 0;
            pile.pau_data->state = PLUG_IDLE;
            pau_vector_clear(pile.pau_data->disabledNodes);
        }
    }

    // 更新接触器状态
    QVector<Contactor> &contactors = const_cast<QVector<Contactor> &>(m_topology->getContactors());
    int maxIdx = qMin(contactors.size(), contactorStates.size());
    for (int i = 0; i < maxIdx; ++i)
    {
        contactors[i].pau_data->isClosed = contactorStates[i];
    }

    // 强制刷新界面
    emit m_topology->topologyChanged();
    updatePileComboBox();
    onTopologyChanged();
    if (m_logWindow)
    {
        m_logWindow->appendLog("已根据远程指令更新拓扑状态。");
    }
}

void MainWindow::onModeSliderChanged(int value)
{
    bool remote = (value == 1);
    if (remote == m_remoteMode)
        return;

    m_remoteMode = remote;

    if (remote)
    {
        // 切换到远程模式：启动 TelnetClient，禁用所有控制控件
        m_telnetClient->start();
        ui->applyConfigButton->setEnabled(false);
        ui->nodeCountSpinBox->setEnabled(false);
        ui->pileCountSpinBox->setEnabled(false);
        ui->unitPowerSpinBox->setEnabled(false);
        ui->requestButton->setEnabled(false);
        ui->releaseButton->setEnabled(false);
        ui->stopChargeButton->setEnabled(false);
        ui->prioritySpinBox->setEnabled(false);
        ui->allocateNodeButton->setEnabled(false);
        ui->releaseNodeButton->setEnabled(false);
        ui->toggleNodeEnableButton->setEnabled(false);
        ui->saveStateButton->setEnabled(false);
        ui->loadStateButton->setEnabled(false);
        ui->powerSpinBox->setEnabled(false);
        ui->pileComboBox->setEnabled(false);
        ui->nodeListWidget->setEnabled(false);
        ui->logTextEdit->append("已切换到远程模式，正在连接服务器...");
    }
    else
    {
        // 切换到手动模式：停止 TelnetClient，恢复所有控件
        m_telnetClient->stop();
        m_remoteControlMode = false;
        // 关闭日志窗口（可选）
        if (m_logWindow)
        {
            m_logWindow->close();
            delete m_logWindow;
            m_logWindow = nullptr;
        }
        // 恢复控件
        ui->applyConfigButton->setEnabled(true);
        ui->nodeCountSpinBox->setEnabled(true);
        ui->pileCountSpinBox->setEnabled(true);
        ui->unitPowerSpinBox->setEnabled(true);
        ui->requestButton->setEnabled(true);
        ui->releaseButton->setEnabled(true);
        ui->stopChargeButton->setEnabled(true);
        ui->prioritySpinBox->setEnabled(true);
        ui->allocateNodeButton->setEnabled(true);
        ui->releaseNodeButton->setEnabled(true);
        ui->toggleNodeEnableButton->setEnabled(true);
        ui->saveStateButton->setEnabled(true);
        ui->loadStateButton->setEnabled(true);
        ui->powerSpinBox->setEnabled(true);
        ui->pileComboBox->setEnabled(true);
        ui->nodeListWidget->setEnabled(true);
        ui->logTextEdit->append("已切换到手动模式，本地控制已恢复。");
    }
}
