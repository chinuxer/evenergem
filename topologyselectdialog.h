#ifndef TOPOLOGYSELECTDIALOG_H
#define TOPOLOGYSELECTDIALOG_H

#include <QDialog>
#include <QButtonGroup>
#include "pwralloc/pau_broker.h"

class TopologySelectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TopologySelectDialog(QWidget *parent = nullptr);
    TOPOTYPE selectedTopology() const { return m_selected; }

private slots:
    void onButtonClicked(int id);

private:
    QButtonGroup *m_buttonGroup;
    TOPOTYPE m_selected;
};

#endif // TOPOLOGYSELECTDIALOG_H