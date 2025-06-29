#include <QApplication>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QProcess>
#include <QDir>
#include <QInputDialog>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QTemporaryDir>
 
class IsoManager : public QWidget {
    Q_OBJECT
 
    QTreeWidget *tree;
    QPushButton *btnAdd, *btnRemove, *btnNew, *btnOpen, *btnSave, *btnAddFolder;
    QTemporaryDir tempDir;
    QString isoPath;
 
public:
    IsoManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("ISO Manager");
        setAcceptDrops(true);
 
        QVBoxLayout *layout = new QVBoxLayout(this);
        tree = new QTreeWidget(this);
        tree->setHeaderLabels({"ISO Contents"});
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        layout->addWidget(tree);
 
        btnAdd = new QPushButton("Add File(s)", this);
        btnAddFolder = new QPushButton("Add Folder", this);
        btnRemove = new QPushButton("Remove Selected", this);
        btnNew = new QPushButton("New ISO", this);
        btnOpen = new QPushButton("Open ISO", this);
        btnSave = new QPushButton("Save ISO", this);
 
        layout->addWidget(btnAdd);
        layout->addWidget(btnAddFolder);
        layout->addWidget(btnRemove);
        layout->addWidget(btnNew);
        layout->addWidget(btnOpen);
        layout->addWidget(btnSave);
 
        connect(btnAdd, &QPushButton::clicked, this, &IsoManager::addFiles);
        connect(btnAddFolder, &QPushButton::clicked, this, &IsoManager::addFolder);
        connect(btnRemove, &QPushButton::clicked, this, &IsoManager::removeSelected);
        connect(btnNew, &QPushButton::clicked, this, &IsoManager::newIso);
        connect(btnOpen, &QPushButton::clicked, this, &IsoManager::openIso);
        connect(btnSave, &QPushButton::clicked, this, &IsoManager::saveIso);
 
        refreshTree();
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
            QFileInfo info(localPath);
            QString destPath = tempDir.path() + "/" + info.fileName();
            if (info.isDir()) {
                QDir().mkpath(destPath);
            } else {
                QFile::copy(localPath, destPath);
            }
        }
        refreshTree();
    }
 
private slots:
    void addFiles() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select File(s)");
        for (const QString &file : files) {
            QFileInfo fi(file);
            QFile::copy(file, tempDir.path() + "/" + fi.fileName());
        }
        refreshTree();
    }
 
    void addFolder() {
        bool ok;
        QString name = QInputDialog::getText(this, "New Folder", "Folder Name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            QDir dir(tempDir.path());
            dir.mkpath(name);
            refreshTree();
        }
    }
 
    void removeSelected() {
        QTreeWidgetItem *item = tree->currentItem();
        if (!item) return;
        QString fullPath = tempDir.path() + "/" + item->data(0, Qt::UserRole).toString();
        QFileInfo info(fullPath);
        if (info.isDir()) {
            QDir(fullPath).removeRecursively();
        } else {
            QFile::remove(fullPath);
        }
        delete item;
    }
 
    void newIso() {
        tempDir.remove();  // auto-cleans
        refreshTree();
        QMessageBox::information(this, "New ISO", "New ISO project started.");
    }
 
    void openIso() {
        QString file = QFileDialog::getOpenFileName(this, "Open ISO File", "", "*.iso");
        if (file.isEmpty()) return;
        isoPath = file;
        tempDir.remove();
        QProcess p;
        QString command = QString("7z x \"%1\" -o\"%2\" -y").arg(isoPath, tempDir.path());
        p.start(command);
        if (!p.waitForFinished() || p.exitCode() != 0) {
            QMessageBox::critical(this, "Error", "Failed to extract ISO. Ensure 7z is installed.");
            return;
        }
        refreshTree();
    }
 
    void saveIso() {
        QString outFile = QFileDialog::getSaveFileName(this, "Save ISO", "", "*.iso");
        if (outFile.isEmpty()) return;
 
        QProcess p;
        QStringList args = {
            "-o", outFile,
            "-J", "-R", "-V", "MyISO",
            "-graft-points"
        };
 
        QStringList graftArgs;
        QDir base(tempDir.path());
        QStringList files = listRecursive(tempDir.path(), base);
        for (const QString &f : files) {
            graftArgs << QString("%1=%2").arg(f, tempDir.path() + "/" + f);
        }
 
        args.append(graftArgs);
        p.start("genisoimage", args);
        if (!p.waitForFinished() || p.exitCode() != 0) {
            QMessageBox::critical(this, "Error", "ISO creation failed. Is genisoimage installed?");
        } else {
            QMessageBox::information(this, "Done", "ISO saved successfully.");
        }
    }
 
    void refreshTree() {
        tree->clear();
        addToTree(tempDir.path(), nullptr);
    }
 
    void addToTree(const QString &path, QTreeWidgetItem *parent) {
        QDir dir(path);
        QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &info : entries) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, info.fileName());
            item->setData(0, Qt::UserRole, QDir(tempDir.path()).relativeFilePath(info.filePath()));
            if (info.isDir()) {
                item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                addToTree(info.filePath(), item);
            } else {
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
            }
            if (parent) parent->addChild(item);
            else tree->addTopLevelItem(item);
        }
    }
 
    QStringList listRecursive(const QString &root, const QDir &base) {
        QStringList out;
        QFileInfoList files = QDir(root).entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &info : files) {
            QString relPath = base.relativeFilePath(info.filePath());
            if (info.isDir()) {
                out << relPath;
                out += listRecursive(info.filePath(), base);
            } else {
                out << relPath;
            }
        }
        return out;
    }
};
 
#include <main.moc>
 
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    IsoManager win;
    win.resize(600, 500);
    win.show();
    return app.exec();
}
 
