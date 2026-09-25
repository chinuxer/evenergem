#include "topologyselectdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QMessageBox>

TopologySelectDialog::TopologySelectDialog(QWidget *parent)
    : QDialog(parent), m_selected(FullMatrix)
{
    setWindowTitle("选择拓扑结构");
    setMinimumSize(600, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QString lightBlueStyle = "color: rgb(102, 153, 255);";
    QLabel *label = new QLabel("请选择功率模块的拓扑结构：", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(lightBlueStyle); // 设置为浅蓝色
    mainLayout->addWidget(label);

    QHBoxLayout *buttonLayout = new QHBoxLayout;

    // 四个单选按钮
    QRadioButton *fullMatrixBtn = new QRadioButton("全矩阵结构", this);
    QRadioButton *ringBtn = new QRadioButton("环形结构", this);
    QRadioButton *hybridBtn = new QRadioButton("半矩阵半环形", this);
    QRadioButton *dualHybridBtn = new QRadioButton("双半矩阵半环形", this);

       fullMatrixBtn->setStyleSheet(lightBlueStyle);
    ringBtn->setStyleSheet(lightBlueStyle);
    hybridBtn->setStyleSheet(lightBlueStyle);
    dualHybridBtn->setStyleSheet(lightBlueStyle);

    fullMatrixBtn->setChecked(true); // 默认选中

    // 加入按钮组（便于统一管理选中状态）
    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->addButton(fullMatrixBtn, FullMatrix);
    m_buttonGroup->addButton(ringBtn, CakraWheel);
    m_buttonGroup->addButton(hybridBtn, SemiHybrid);
    m_buttonGroup->addButton(dualHybridBtn, DualSemiHybrid);

    connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &TopologySelectDialog::onButtonClicked);

    // 每个按钮下面可以加一个简短的说明（可选）
    buttonLayout->addWidget(fullMatrixBtn);
    buttonLayout->addWidget(ringBtn);
    buttonLayout->addWidget(hybridBtn);
    buttonLayout->addWidget(dualHybridBtn);

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

void TopologySelectDialog::accept()
{
    // 全矩阵结构尚未完成，提示并回退到选择界面
    if (FullMatrix == m_selected)
    {
        QMessageBox box(this);
        box.setWindowTitle(tr("提示"));
        box.setIcon(QMessageBox::Warning);
        box.setText(tr("作者正加班完善该工作"));
        box.setStandardButtons(QMessageBox::Ok);
        box.setButtonText(QMessageBox::Ok, tr("返回"));
        box.exec();
        return; // 不关闭选择对话框
    }
    QDialog::accept();
}
