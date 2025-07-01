#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H

#include <QDialog>

// Forward declaration para evitar include do .ui no .h
namespace Ui {
class WelcomeDialog;
}

class WelcomeDialog : public QDialog
{
    Q_OBJECT

public:
    enum class UserChoice {
        None,
        OpenExisting,
        NewMap
    };

    explicit WelcomeDialog(QWidget *parent = nullptr);
    ~WelcomeDialog();

    UserChoice getUserChoice() const;

signals:
    void openExistingMapRequested();
    void newMapRequested();

private slots:
    void on_buttonOpenExisting_clicked();
    void on_buttonNewMap_clicked();

private:
    Ui::WelcomeDialog *ui;
    UserChoice m_choice;
};

#endif // WELCOMEDIALOG_H
