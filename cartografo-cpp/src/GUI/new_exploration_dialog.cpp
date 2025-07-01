#include "new_exploration_dialog.h"
#include "ui_new_exploration_dialog.h" // Gerado pelo UIC
#include <QStringList>
#include <QMessageBox>

NewExplorationDialog::NewExplorationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewExplorationDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Iniciar Nova Exploração de Rede"));

    // Valores padrão para os parâmetros
    ui->spinBoxMaxHops->setValue(30);
    ui->spinBoxQueriesPerHop->setValue(3);
    ui->checkBoxUseIcmp->setChecked(false); // UDP por padrão
    // ui->checkBoxUseTcp->setChecked(false); // Se adicionar opção TCP

    // Validadores para os campos, se necessário (ex: para targets)
    // ui->textEditTargets->setPlaceholderText(tr("Um IP ou hostname por linha, ou separados por vírgula."));
}

NewExplorationDialog::~NewExplorationDialog()
{
    delete ui;
}

ExplorationSettings NewExplorationDialog::getSettings() const
{
    ExplorationSettings settings;

    // Obter alvos do QTextEdit (um por linha ou separados por vírgula/espaço)
    QString targetsText = ui->textEditTargets->toPlainText().trimmed();
    if (targetsText.contains(',') || targetsText.contains(' ') || targetsText.contains('\n')) {
        // Divide por vírgula, espaço, ou nova linha, e remove entradas vazias
        settings.targets = targetsText.split(QRegularExpression("[,\\s\\n]+"), Qt::SkipEmptyParts);
    } else if (!targetsText.isEmpty()) {
        settings.targets.append(targetsText);
    }

    settings.params.maxHops = ui->spinBoxMaxHops->value();
    settings.params.queriesPerHop = ui->spinBoxQueriesPerHop->value();
    settings.params.useIcmp = ui->checkBoxUseIcmp->isChecked();
    // settings.params.useTcp = ui->checkBoxUseTcp->isChecked(); // Se TCP for opção

    return settings;
}

void NewExplorationDialog::on_buttonBox_accepted()
{
    ExplorationSettings currentSettings = getSettings();
    if (currentSettings.targets.isEmpty()) {
        QMessageBox::warning(this, tr("Entrada Inválida"), tr("Por favor, insira pelo menos um alvo para exploração."));
        // Não fecha o diálogo, permite ao usuário corrigir.
        // Para fazer isso, precisamos evitar que QDialog::accept() seja chamado automaticamente.
        // Uma forma é não conectar o slot accepted() do buttonBox diretamente,
        // mas sim conectar o clicked(QAbstractButton*) e verificar o role do botão.
        // Ou, mais simples, se a validação falhar, não fazer nada aqui,
        // e o código que chama exec() verificará se os targets estão vazios.
        // Por agora, vamos permitir que feche e o chamador valide.
        // Se quisermos manter aberto:
        // done(QDialog::Accepted); // Força o fechamento com Accepted
        // Para manter aberto, não chame done() ou accept().
        // O QDialogButtonBox::accepted() já chama QDialog::accept().
        // A validação deve ocorrer antes de chamar accept() ou no código que usa o diálogo.
        // Para este exemplo, deixaremos o chamador validar após o diálogo fechar.
    }

    // Se tudo ok, o QDialogButtonBox já chama accept() e o diálogo fecha.
    // Se quiséssemos impedir o fechamento em caso de erro, teríamos que:
    // 1. Desconectar o sinal `accepted` do `QDialogButtonBox` do slot `accept` do `QDialog`.
    // 2. Conectar `accepted` a um slot customizado.
    // 3. Nesse slot customizado, fazer a validação. Se ok, chamar `QDialog::accept()`. Senão, mostrar mensagem.
    // Ou, no slot on_buttonBox_accepted(), se a validação falhar, não fazer nada e o diálogo não fechará
    // se o sinal `accepted` do `QDialogButtonBox` não estiver conectado ao slot `accept` do `QDialog`.
    // A forma mais simples é validar *depois* que o diálogo retorna, no código que o chamou.

    accept(); // Garante que o diálogo feche com o estado Accepted.
}
// Opcional: on_buttonBox_rejected() para lidar com o cancelamento, mas QDialog já faz isso.

/*
// Exemplo de como impedir o fechamento se a validação falhar:
void NewExplorationDialog::accept() // Sobrescreve o slot accept do QDialog
{
    ExplorationSettings currentSettings = getSettings();
    if (currentSettings.targets.isEmpty()) {
        QMessageBox::warning(this, tr("Entrada Inválida"), tr("Por favor, insira pelo menos um alvo para exploração."));
        return; // Não chama QDialog::accept(), diálogo permanece aberto
    }
    QDialog::accept(); // Chama a implementação base para fechar
}
*/
