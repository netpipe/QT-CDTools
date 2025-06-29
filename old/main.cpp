#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QLineEdit>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QDebug>

class IsoManager : public QWidget {
    Q_OBJECT

    QListWidget *fileList;
    QPushButton *btnAddFiles;
    QPushButton *btnAddFolder;
    QPushButton *btnRemove;
    QPushButton *btnCreateIso;
    QLineEdit *labelInput;

    QTemporaryDir tempDir;

public:
    IsoManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("ISO Manager (mkisofs)");
        setAcceptDrops(true);

        auto *mainLayout = new QVBoxLayout(this);
        fileList = new QListWidget(this);
        mainLayout->addWidget(fileList);

        labelInput = new QLineEdit(this);
        labelInput->setPlaceholderText("Enter ISO Label (optional)");
        mainLayout->addWidget(labelInput);

        auto *btnLayout = new QHBoxLayout();
        btnAddFiles = new QPushButton("Add File(s)", this);
        btnAddFolder = new QPushButton("Add Folder", this);
        btnRemove = new QPushButton("Remove Selected", this);
        btnCreateIso = new QPushButton("Create ISO", this);

        btnLayout->addWidget(btnAddFiles);
        btnLayout->addWidget(btnAddFolder);
        btnLayout->addWidget(btnRemove);
        btnLayout->addWidget(btnCreateIso);

        mainLayout->addLayout(btnLayout);

        connect(btnAddFiles, &QPushButton::clicked, this, &IsoManager::addFiles);
        connect(btnAddFolder, &QPushButton::clicked, this, &IsoManager::addFolder);
        connect(btnRemove, &QPushButton::clicked, this, &IsoManager::removeSelected);
        connect(btnCreateIso, &QPushButton::clicked, this, &IsoManager::createIso);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void dropEvent(QDropEvent *event) override {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString localPath = url.toLocalFile();
            QFileInfo fi(localPath);
            if (fi.exists()) {
                if (fi.isDir()) {
                    copyFolderRecursively(localPath, tempDir.path() + "/" + fi.fileName());
                } else {
                    QFile::copy(localPath, tempDir.path() + "/" + fi.fileName());
                }
                fileList->addItem(localPath);
            }
        }
    }

private slots:
    void addFiles() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Add Files");
        for (const QString &file : files) {
            QFileInfo fi(file);
            QString destPath = tempDir.path() + "/" + fi.fileName();
            if (QFile::exists(destPath)) QFile::remove(destPath);
            QFile::copy(file, destPath);
            fileList->addItem(file);
        }
    }

    void addFolder() {
        QString folder = QFileDialog::getExistingDirectory(this, "Select Folder");
        if (!folder.isEmpty()) {
            QFileInfo fi(folder);
            QString dest = tempDir.path() + "/" + fi.fileName();
            copyFolderRecursively(folder, dest);
            fileList->addItem(folder);
        }
    }

    void removeSelected() {
        QListWidgetItem *item = fileList->takeItem(fileList->currentRow());
        if (!item) return;
        QFileInfo fi(item->text());
        QString target = tempDir.path() + "/" + fi.fileName();
        if (QFileInfo(target).isDir()) {
            QDir(target).removeRecursively();
        } else {
            QFile::remove(target);
        }
        delete item;
    }

    void createIso() {
        if (fileList->count() == 0) {
            QMessageBox::warning(this, "No files", "Add some files or folders first.");
            return;
        }

        QString isoPath = QFileDialog::getSaveFileName(this, "Save ISO Image", "", "*.iso");
        if (isoPath.isEmpty()) return;

        QString label = labelInput->text();
        QStringList args;
        args << "-o" << isoPath;
        if (!label.isEmpty()) args << "-V" << label;
        args << "-J" << "-R" << ".";

        QProcess proc;
        proc.setWorkingDirectory(tempDir.path());
        proc.start("mkisofs", args);
        proc.waitForFinished();

        QString stdOut = proc.readAllStandardOutput();
        QString stdErr = proc.readAllStandardError();

        if (proc.exitCode() == 0) {
            QMessageBox::information(this, "Success", "ISO created successfully.");
        } else {
            QMessageBox::critical(this, "mkisofs failed",
                "Command: mkisofs " + args.join(" ") + "\n\nError:\n" + stdErr + "\nOutput:\n" + stdOut);
        }
    }


private:
    void copyFolderRecursively(const QString &srcPath, const QString &destPath) {
        QDir srcDir(srcPath);
        QDir destDir;
        destDir.mkpath(destPath);
        for (const QFileInfo &entry : srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
            QString src = entry.absoluteFilePath();
            QString dest = destPath + "/" + entry.fileName();
            if (entry.isDir()) {
                copyFolderRecursively(src, dest);
            } else {
                QFile::copy(src, dest);
            }
        }
    }
};

#include <main.moc>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    IsoManager w;
    w.resize(640, 480);
    w.show();
    return app.exec();
}
