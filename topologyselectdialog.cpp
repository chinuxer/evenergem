#include "topologyselectdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>

TopologySelectDialog::TopologySelectDialog(QWidget *parent)
    : QDialog(parent), m_selected(FullMatrix)
{
    setWindowTitle("选择拓扑结构");
    setMinimumSize(600, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *label = new QLabel("请选择功率模块的拓扑结构：", this);
    label->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(label);

    QHBoxLayout *buttonLayout = new QHBoxLayout;

    // 三个单选按钮
    QRadioButton *fullMatrixBtn = new QRadioButton("全矩阵结构", this);
    QRadioButton *ringBtn = new QRadioButton("环形结构", this);
    QRadioButton *hybridBtn = new QRadioButton("半矩阵半环形", this);

    fullMatrixBtn->setChecked(true); // 默认选中

    // 加入按钮组（便于统一管理选中状态）
    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->addButton(fullMatrixBtn, FullMatrix);
    m_buttonGroup->addButton(ringBtn, CakraWheel);
    m_buttonGroup->addButton(hybridBtn, SemiHybrid);

    connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &TopologySelectDialog::onButtonClicked);

    // 每个按钮下面可以加一个简短的说明（可选）
    buttonLayout->addWidget(fullMatrixBtn);
    buttonLayout->addWidget(ringBtn);
    buttonLayout->addWidget(hybridBtn);

    mainLayout->addLayout(buttonLayout);

    // 确定按钮
    QPushButton *okBtn = new QPushButton("确定", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    mainLayout->addWidget(okBtn, 0, Qt::AlignCenter);
}

void TopologySelectDialog::onButtonClicked(int id)
{
    m_selected = static_cast<TOPOTYPE>(id);
}
