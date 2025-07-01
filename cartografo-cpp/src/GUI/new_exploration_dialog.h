#ifndef NEWEXPLORATIONDIALOG_H
#define NEWEXPLORATIONDIALOG_H

#include <QDialog>
#include <QStringList>
#include "Core/traceroute_task.h" // Para TracerouteParams

namespace Ui {
class NewExplorationDialog;
}

struct ExplorationSettings {
    QStringList targets;
    TracerouteParams params;
};

class NewExplorationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewExplorationDialog(QWidget *parent = nullptr);
    ~NewExplorationDialog();

    ExplorationSettings getSettings() const;

private slots:
    void on_buttonBox_accepted();
    // void on_buttonBox_rejected(); // QDialog lida com isso por padrão

private:
    Ui::NewExplorationDialog *ui;
};

#endif // NEWEXPLORATIONDIALOG_H
