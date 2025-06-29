#include <QApplication>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

class IsoManager : public QWidget {
    Q_OBJECT

    QListWidget *fileList;
    QString isoPath;
    QString tempDirPath;
    QTemporaryDir tempDir;

public:
    IsoManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("ISO Manager");

        QVBoxLayout *layout = new QVBoxLayout(this);
        fileList = new QListWidget(this);
        layout->addWidget(fileList);

        QPushButton *btnAdd = new QPushButton("Add File", this);
        QPushButton *btnRemove = new QPushButton("Remove Selected", this);
        QPushButton *btnNew = new QPushButton("New ISO", this);
        QPushButton *btnOpen = new QPushButton("Open ISO", this);
        QPushButton *btnSave = new QPushButton("Save ISO", this);

        layout->addWidget(btnAdd);
        layout->addWidget(btnRemove);
        layout->addWidget(btnNew);
        layout->addWidget(btnOpen);
        layout->addWidget(btnSave);

        connect(btnAdd, &QPushButton::clicked, this, &IsoManager::addFile);
        connect(btnRemove, &QPushButton::clicked, this, &IsoManager::removeSelected);
        connect(btnNew, &QPushButton::clicked, this, &IsoManager::newIso);
        connect(btnOpen, &QPushButton::clicked, this, &IsoManager::openIso);
        connect(btnSave, &QPushButton::clicked, this, &IsoManager::saveIso);
    }

private slots:
    void addFile() {
        QString file = QFileDialog::getOpenFileName(this, "Select file to add");
        if (!file.isEmpty()) {
            QFileInfo fi(file);
            QString dest = tempDir.path() + "/" + fi.fileName();
            QFile::copy(file, dest);
            fileList->addItem(fi.fileName());
        }
    }

    void removeSelected() {
        QListWidgetItem *item = fileList->currentItem();
        if (!item) return;
        QString path = tempDir.path() + "/" + item->text();
        QFile::remove(path);
        delete item;
    }

    void newIso() {
        tempDir.remove();
        tempDir = QTemporaryDir();  // reinitialize
        fileList->clear();
        isoPath.clear();
        QMessageBox::information(this, "New ISO", "Started a new ISO project.");
    }

    void openIso() {
        isoPath = QFileDialog::getOpenFileName(this, "Open ISO file", QString(), "*.iso");
        if (isoPath.isEmpty()) return;

        tempDir.remove();
        tempDir = QTemporaryDir();  // clean previous content

        QProcess p;
        QStringList args = {"-i", isoPath, "-x", tempDir.path()};
        QString command = QString("7z x \"%1\" -o\"%2\" -y").arg(isoPath, tempDir.path());
        p.start(command);
        if (!p.waitForFinished() || p.exitCode() != 0) {
            QMessageBox::critical(this, "Error", "Failed to extract ISO (requires 7z).");
            return;
        }

        fileList->clear();
        QDir dir(tempDir.path());
        for (QString f : dir.entryList(QDir::Files)) {
            fileList->addItem(f);
        }
        QMessageBox::information(this, "ISO Loaded", "ISO contents loaded.");
    }

    void saveIso() {
        if (fileList->count() == 0) {
            QMessageBox::warning(this, "Empty", "No files to write to ISO.");
            return;
        }

        QString savePath = QFileDialog::getSaveFileName(this, "Save ISO As", "", "*.iso");
        if (savePath.isEmpty()) return;

        QProcess process;
        QStringList args = {
            "-o", savePath,
            "-J", "-R", "-V", "CustomISO",
            tempDir.path()
        };

        process.start("genisoimage", args);
        if (!process.waitForFinished() || process.exitCode() != 0) {
            QMessageBox::critical(this, "Error", "Failed to create ISO. Is 'genisoimage' installed?");
        } else {
            QMessageBox::information(this, "Success", "ISO created successfully.");
        }
    }
};

#include <main.moc>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    IsoManager w;
    w.resize(400, 400);
    w.show();
    return app.exec();
}
