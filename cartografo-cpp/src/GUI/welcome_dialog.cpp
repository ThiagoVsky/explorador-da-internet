#include "welcome_dialog.h"
#include "ui_welcome_dialog.h" // Qt UIC irá gerar este arquivo a partir do .ui

WelcomeDialog::WelcomeDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WelcomeDialog),
    m_choice(UserChoice::None)
{
    ui->setupUi(this);
    setWindowTitle(tr("Bem-vindo ao Cartógrafo de Rede"));

    // Conexões podem ser feitas aqui ou automaticamente pelo nome (on_NOMEOBJETO_SINAL)
    // connect(ui->buttonOpenExisting, &QPushButton::clicked, this, &WelcomeDialog::on_buttonOpenExisting_clicked);
    // connect(ui->buttonNewMap, &QPushButton::clicked, this, &WelcomeDialog::on_buttonNewMap_clicked);
}

WelcomeDialog::~WelcomeDialog()
{
    delete ui;
}

WelcomeDialog::UserChoice WelcomeDialog::getUserChoice() const
{
    return m_choice;
}

void WelcomeDialog::on_buttonOpenExisting_clicked()
{
    m_choice = UserChoice::OpenExisting;
    emit openExistingMapRequested();
    accept(); // Fecha o diálogo com QDialog::Accepted
}

void WelcomeDialog::on_buttonNewMap_clicked()
{
    m_choice = UserChoice::NewMap;
    emit newMapRequested();
    accept(); // Fecha o diálogo com QDialog::Accepted
}

// Se o usuário fechar o diálogo sem clicar nos botões (ex: pelo 'X' da janela),
// m_choice permanecerá None, e o diálogo retornará QDialog::Rejected por padrão.
// Podemos tratar isso no código que chama exec() se necessário.
