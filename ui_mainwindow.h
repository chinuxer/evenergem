/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.12.12
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QGraphicsView *graphicsView;
    QVBoxLayout *verticalLayout;
    QLabel *label;

    QLabel *label_2;
    QSpinBox *nodeCountSpinBox;
    QHBoxLayout *horizontalLayout_nodePile;
    QLabel *label_3;
    QSpinBox *pileCountSpinBox;
    QLabel *label_unitPower;
    QDoubleSpinBox *unitPowerSpinBox;
    QPushButton *applyConfigButton;
    QLabel *label_4;
    QComboBox *pileComboBox;
    QSpinBox *powerSpinBox;
    QLabel *label_9;
    QSpinBox *prioritySpinBox;
    QHBoxLayout *horizontalLayout_4;
    QPushButton *requestButton;
    QPushButton *releaseButton;
    QPushButton *stopChargeButton;
    QLabel *label_5;
    QListWidget *nodeListWidget;
    QHBoxLayout *horizontalLayout_5;
    QPushButton *allocateNodeButton;
    QPushButton *releaseNodeButton;
    QPushButton *toggleNodeEnableButton;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_6;
    QTextEdit *pileInfoTextEdit;
    QLabel *label_7;
    QTextEdit *statusTextEdit;
    QLabel *label_8;
    QTextEdit *logTextEdit;
    QPushButton *saveStateButton;
    QPushButton *loadStateButton;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1400, 800);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        graphicsView = new QGraphicsView(centralwidget);
        graphicsView->setObjectName(QString::fromUtf8("graphicsView"));

        horizontalLayout->addWidget(graphicsView);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label = new QLabel(centralwidget);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout->addWidget(label);
        horizontalLayout_nodePile = new QHBoxLayout();
        horizontalLayout_nodePile->setObjectName(QString::fromUtf8("horizontalLayout_nodePile"));
        label_2 = new QLabel(centralwidget);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        horizontalLayout_nodePile->addWidget(label_2);
        nodeCountSpinBox = new QSpinBox(centralwidget);
        nodeCountSpinBox->setObjectName(QString::fromUtf8("nodeCountSpinBox"));
        nodeCountSpinBox->setMinimum(6);
        nodeCountSpinBox->setMaximum(QTDEMOD_NODES_CAPACITY);
        nodeCountSpinBox->setValue(20);
        horizontalLayout_nodePile->addWidget(nodeCountSpinBox);
        label_3 = new QLabel(centralwidget);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        horizontalLayout_nodePile->addWidget(label_3);

        pileCountSpinBox = new QSpinBox(centralwidget);
        pileCountSpinBox->setObjectName(QString::fromUtf8("pileCountSpinBox"));
        pileCountSpinBox->setMinimum(1);
        pileCountSpinBox->setMaximum(18);
        pileCountSpinBox->setValue(10);
        horizontalLayout_nodePile->addWidget(pileCountSpinBox);

        label_unitPower = new QLabel(centralwidget);
        label_unitPower->setObjectName(QString::fromUtf8("label_unitPower"));
        horizontalLayout_nodePile->addWidget(label_unitPower);

        unitPowerSpinBox = new QDoubleSpinBox(centralwidget);
        unitPowerSpinBox->setObjectName(QString::fromUtf8("unitPowerSpinBox"));
        unitPowerSpinBox->setDecimals(1);
        unitPowerSpinBox->setMinimum(1.0);
        unitPowerSpinBox->setMaximum(200.0);
        unitPowerSpinBox->setValue(40.0);
        unitPowerSpinBox->setSingleStep(0.5);
        horizontalLayout_nodePile->addWidget(unitPowerSpinBox);

        verticalLayout->addLayout(horizontalLayout_nodePile);

        applyConfigButton = new QPushButton(centralwidget);
        applyConfigButton->setObjectName(QString::fromUtf8("applyConfigButton"));

        verticalLayout->addWidget(applyConfigButton);

        label_4 = new QLabel(centralwidget);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        verticalLayout->addWidget(label_4);

        pileComboBox = new QComboBox(centralwidget);
        pileComboBox->setObjectName(QString::fromUtf8("pileComboBox"));

        verticalLayout->addWidget(pileComboBox);

        powerSpinBox = new QSpinBox(centralwidget);
        powerSpinBox->setObjectName(QString::fromUtf8("powerSpinBox"));

        powerSpinBox->setMinimum(1);
        powerSpinBox->setMaximum(2000);
        powerSpinBox->setValue(40);

        verticalLayout->addWidget(powerSpinBox);

        label_9 = new QLabel(centralwidget);
        label_9->setObjectName(QString::fromUtf8("label_9"));

        verticalLayout->addWidget(label_9);

        prioritySpinBox = new QSpinBox(centralwidget);
        prioritySpinBox->setObjectName(QString::fromUtf8("prioritySpinBox"));
        prioritySpinBox->setMinimum(1);
        prioritySpinBox->setMaximum(4);
        prioritySpinBox->setValue(1);

        verticalLayout->addWidget(prioritySpinBox);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        requestButton = new QPushButton(centralwidget);
        requestButton->setObjectName(QString::fromUtf8("requestButton"));

        horizontalLayout_4->addWidget(requestButton);

        releaseButton = new QPushButton(centralwidget);
        releaseButton->setObjectName(QString::fromUtf8("releaseButton"));

        horizontalLayout_4->addWidget(releaseButton);

        stopChargeButton = new QPushButton(centralwidget);
        stopChargeButton->setObjectName(QString::fromUtf8("stopChargeButton"));

        horizontalLayout_4->addWidget(stopChargeButton);

        verticalLayout->addLayout(horizontalLayout_4);

        label_5 = new QLabel(centralwidget);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        verticalLayout->addWidget(label_5);

        nodeListWidget = new QListWidget(centralwidget);
        nodeListWidget->setObjectName(QString::fromUtf8("nodeListWidget"));

        verticalLayout->addWidget(nodeListWidget);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        allocateNodeButton = new QPushButton(centralwidget);
        allocateNodeButton->setObjectName(QString::fromUtf8("allocateNodeButton"));

        horizontalLayout_5->addWidget(allocateNodeButton);

        releaseNodeButton = new QPushButton(centralwidget);
        releaseNodeButton->setObjectName(QString::fromUtf8("releaseNodeButton"));

        horizontalLayout_5->addWidget(releaseNodeButton);

        toggleNodeEnableButton = new QPushButton(centralwidget);
        toggleNodeEnableButton->setObjectName(QString::fromUtf8("toggleNodeEnableButton"));

        horizontalLayout_5->addWidget(toggleNodeEnableButton);

        verticalLayout->addLayout(horizontalLayout_5);

        label_6 = new QLabel(centralwidget);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        verticalLayout->addWidget(label_6);

        pileInfoTextEdit = new QTextEdit(centralwidget);
        pileInfoTextEdit->setObjectName(QString::fromUtf8("pileInfoTextEdit"));
        pileInfoTextEdit->setReadOnly(true);

        verticalLayout->addWidget(pileInfoTextEdit);

        label_7 = new QLabel(centralwidget);
        label_7->setObjectName(QString::fromUtf8("label_7"));

        verticalLayout->addWidget(label_7);

        statusTextEdit = new QTextEdit(centralwidget);
        statusTextEdit->setObjectName(QString::fromUtf8("statusTextEdit"));
        statusTextEdit->setReadOnly(true);

        verticalLayout->addWidget(statusTextEdit);

        label_8 = new QLabel(centralwidget);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        verticalLayout->addWidget(label_8);

        logTextEdit = new QTextEdit(centralwidget);
        logTextEdit->setObjectName(QString::fromUtf8("logTextEdit"));
        logTextEdit->setReadOnly(true);

        verticalLayout->addWidget(logTextEdit);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        saveStateButton = new QPushButton(centralwidget);
        saveStateButton->setObjectName(QString::fromUtf8("saveStateButton"));

        horizontalLayout_6->addWidget(saveStateButton);

        loadStateButton = new QPushButton(centralwidget);
        loadStateButton->setObjectName(QString::fromUtf8("loadStateButton"));

        horizontalLayout_6->addWidget(loadStateButton);

        verticalLayout->addLayout(horizontalLayout_6);

        horizontalLayout->addLayout(verticalLayout);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);

    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "evenergem", nullptr));
        label->setText(QApplication::translate("MainWindow", "\347\263\273\347\273\237\351\205\215\347\275\256", nullptr));
        label_2->setText(QApplication::translate("MainWindow", "线环节点数:", nullptr));
        label_3->setText(QApplication::translate("MainWindow", "充电桩数:", nullptr));
        applyConfigButton->setText(QApplication::translate("MainWindow", "\345\272\224\347\224\250\351\205\215\347\275\256", nullptr));
        label_4->setText(QApplication::translate("MainWindow", "\345\212\237\347\216\207\346\216\247\345\210\266", nullptr));
        powerSpinBox->setSuffix(QApplication::translate("MainWindow", " kW", nullptr));
        label_9->setText(QApplication::translate("MainWindow", "\345\205\205\347\224\265\346\241\251\344\274\230\345\205\210\347\272\247:", nullptr));
        requestButton->setText(QApplication::translate("MainWindow", "\350\257\267\346\261\202\345\212\237\347\216\207", nullptr));
        releaseButton->setText(QApplication::translate("MainWindow", "\351\207\212\346\224\276\345\212\237\347\216\207", nullptr));
        label_5->setText(QApplication::translate("MainWindow", "\346\211\213\345\212\250\346\223\215\344\275\234\357\274\210\346\265\213\350\257\225\357\274\211", nullptr));
        allocateNodeButton->setText(QApplication::translate("MainWindow", "\345\210\206\351\205\215\350\212\202\347\202\271", nullptr));
        releaseNodeButton->setText(QApplication::translate("MainWindow", "\351\207\212\346\224\276\350\212\202\347\202\271", nullptr));
        label_6->setText(QApplication::translate("MainWindow", "\345\205\205\347\224\265\346\241\251\344\277\241\346\201\257", nullptr));
        label_7->setText(QApplication::translate("MainWindow", "\347\263\273\347\273\237\347\212\266\346\200\201", nullptr));
        label_8->setText(QApplication::translate("MainWindow", "\346\223\215\344\275\234\346\227\245\345\277\227", nullptr));
        saveStateButton->setText(QApplication::translate("MainWindow", "保存工况", nullptr));
        loadStateButton->setText(QApplication::translate("MainWindow", "加载工况", nullptr));
        label_unitPower->setText(QApplication::translate("MainWindow", "单机功率:", nullptr));
        unitPowerSpinBox->setSuffix(QApplication::translate("MainWindow", " kW", nullptr));
        stopChargeButton->setText(QApplication::translate("MainWindow", "停止充电", nullptr));
        toggleNodeEnableButton->setText(QApplication::translate("MainWindow", "可用启停", nullptr));

    } // retranslateUi
};

namespace Ui
{
    class MainWindow : public Ui_MainWindow
    {
    };
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
