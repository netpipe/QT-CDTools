#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QFileDialog>
#include <QProcess>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QInputDialog>

class XorrisoGui : public QMainWindow {
    Q_OBJECT

public:
    XorrisoGui(QWidget *parent = nullptr) : QMainWindow(parent) {
        QWidget *central = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(central);

        openBtn = new QPushButton("Open ISO", this);
        listBtn = new QPushButton("List ISO Files", this);
        extractBtn = new QPushButton("Extract File", this);
        addBtn = new QPushButton("Add File", this);
        deleteBtn = new QPushButton("Delete File", this);
        output = new QTextEdit(this);
        output->setReadOnly(true);

        layout->addWidget(openBtn);
        layout->addWidget(listBtn);
        layout->addWidget(extractBtn);
        layout->addWidget(addBtn);
        layout->addWidget(deleteBtn);
        layout->addWidget(output);

        connect(openBtn, &QPushButton::clicked, this, &XorrisoGui::selectISO);
        connect(listBtn, &QPushButton::clicked, this, &XorrisoGui::listFiles);
        connect(extractBtn, &QPushButton::clicked, this, &XorrisoGui::extractFile);
        connect(addBtn, &QPushButton::clicked, this, &XorrisoGui::addFile);
        connect(deleteBtn, &QPushButton::clicked, this, &XorrisoGui::deleteFile);

        setCentralWidget(central);
        resize(600, 400);
    }

private:
    QPushButton *openBtn, *listBtn, *extractBtn, *addBtn, *deleteBtn;
    QTextEdit *output;
    QString isoPath;

    void runCommand(const QStringList &args) {
        QProcess proc;
        proc.start("xorriso", args);
        proc.waitForFinished(-1);
        QString out = proc.readAllStandardOutput();
        QString err = proc.readAllStandardError();
        output->append(">>> " + args.join(" "));
        output->append(out + err);
    }

    void selectISO() {
        isoPath = QFileDialog::getOpenFileName(this, "Select ISO file", "", "*.iso");
        if (!isoPath.isEmpty()) {
            output->append("ISO selected: " + isoPath);
        }
    }

    void listFiles() {
        if (isoPath.isEmpty()) return;
        runCommand({"-indev", isoPath, "-ls", "/"});
    }

    void extractFile() {
        if (isoPath.isEmpty()) return;
        QString isoFile = QInputDialog::getText(this, "ISO File to Extract", "Path inside ISO:");
        QString targetDir = QFileDialog::getExistingDirectory(this, "Target Folder");
        if (!isoFile.isEmpty() && !targetDir.isEmpty()) {
            runCommand({"-osirrox", "on", "-indev", isoPath, "-extract", isoFile, targetDir});
        }
    }

    void addFile() {
        if (isoPath.isEmpty()) return;
        QString localFile = QFileDialog::getOpenFileName(this, "Select file to add");
        QString isoTarget = QInputDialog::getText(this, "ISO Target Path", "Target path in ISO:");
        if (!localFile.isEmpty() && !isoTarget.isEmpty()) {
            runCommand({"-dev", isoPath, "-update", "once", localFile, isoTarget});
        }
    }

    void deleteFile() {
        if (isoPath.isEmpty()) return;
        QString fileToDelete = QInputDialog::getText(this, "File to delete in ISO", "Path:");
        if (!fileToDelete.isEmpty()) {
            runCommand({"-dev", isoPath, "-rm", fileToDelete});
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    XorrisoGui w;
    w.setWindowTitle("Xorriso ISO Manager");
    w.show();
    return a.exec();
}
