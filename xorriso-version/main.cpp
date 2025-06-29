#include <QApplication>
#include <QMainWindow>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QLabel>
#include <QTextEdit>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>

class XorrisoIsoManager : public QMainWindow {
    Q_OBJECT

public:
    XorrisoIsoManager(QWidget *parent = nullptr) : QMainWindow(parent) {
        setAcceptDrops(true);

        QWidget *central = new QWidget(this);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        QHBoxLayout *topLayout = new QHBoxLayout();
        QPushButton *openBtn = new QPushButton("Open ISO");
        QPushButton *extractBtn = new QPushButton("Extract");
        QPushButton *addBtn = new QPushButton("Add");
        QPushButton *deleteBtn = new QPushButton("Delete");
        QPushButton *rebuildBtn = new QPushButton("Rebuild ISO");
        topLayout->addWidget(openBtn);
        topLayout->addWidget(extractBtn);
        topLayout->addWidget(addBtn);
        topLayout->addWidget(deleteBtn);
        topLayout->addWidget(rebuildBtn);

        tree = new QTreeWidget();
        tree->setHeaderLabel("ISO Contents");

        output = new QTextEdit();
        output->setReadOnly(true);

        QSplitter *splitter = new QSplitter(Qt::Vertical);
        splitter->addWidget(tree);
        splitter->addWidget(output);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 1);

        mainLayout->addLayout(topLayout);
        mainLayout->addWidget(splitter);

        setCentralWidget(central);
        resize(800, 600);

        connect(openBtn, &QPushButton::clicked, this, &XorrisoIsoManager::openIso);
        connect(extractBtn, &QPushButton::clicked, this, &XorrisoIsoManager::extractFile);
        connect(addBtn, &QPushButton::clicked, this, &XorrisoIsoManager::addFile);
        connect(deleteBtn, &QPushButton::clicked, this, &XorrisoIsoManager::deleteFile);
        connect(rebuildBtn, &QPushButton::clicked, this, &XorrisoIsoManager::rebuildIso);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (QFileInfo(path).isFile()) pendingFiles << path;
        }
        output->append("Files queued to add: " + pendingFiles.join(", "));
    }

private:
    QTreeWidget *tree;
    QTextEdit *output;
    QString isoPath;
    QStringList pendingFiles;

    void runXorriso(const QStringList &args) {
        QProcess proc;
        proc.start("xorriso", args);
        proc.waitForFinished(-1);
        output->append(">>> " + args.join(" "));
        output->append(proc.readAllStandardOutput());
        output->append(proc.readAllStandardError());
    }

    void openIso() {
        isoPath = QFileDialog::getOpenFileName(this, "Open ISO", "", "*.iso");
        if (isoPath.isEmpty()) return;
        tree->clear();
        QProcess proc;
        proc.start("xorriso", {"-indev", isoPath, "-ls_r", "/"});
        proc.waitForFinished();
        QStringList lines = QString(proc.readAllStandardOutput()).split('\n');
        for (const QString &line : lines) {
            if (line.startsWith("/")) addPathToTree(line);
        }
    }

    void addPathToTree(const QString &path) {
        QStringList parts = path.split("/", QString::SkipEmptyParts);
        QTreeWidgetItem *parent = nullptr;
        for (const QString &part : parts) {
            QTreeWidgetItem *item = nullptr;
            if (!parent) {
                for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                    if (tree->topLevelItem(i)->text(0) == part) {
                        item = tree->topLevelItem(i);
                        break;
                    }
                }
                if (!item) {
                    item = new QTreeWidgetItem(QStringList(part));
                    tree->addTopLevelItem(item);
                }
            } else {
                bool found = false;
                for (int i = 0; i < parent->childCount(); ++i) {
                    if (parent->child(i)->text(0) == part) {
                        item = parent->child(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    item = new QTreeWidgetItem(QStringList(part));
                    parent->addChild(item);
                }
            }
            parent = item;
        }
    }

    void extractFile() {
        if (!tree->currentItem()) return;
        QString isoItem = getFullPath(tree->currentItem());
        QString outDir = QFileDialog::getExistingDirectory(this, "Select extraction directory");
        if (!outDir.isEmpty()) {
            runXorriso({"-osirrox", "on", "-indev", isoPath, "-extract", isoItem, outDir});
        }
    }

    void addFile() {
        if (isoPath.isEmpty() || pendingFiles.isEmpty()) return;
        for (const QString &file : pendingFiles) {
            QString isoTarget = QInputDialog::getText(this, "Target Path", "Enter target path in ISO for: " + file);
            if (!isoTarget.isEmpty()) {
                runXorriso({"-dev", isoPath, "-update", "once", file, isoTarget});
            }
        }
        pendingFiles.clear();
        openIso(); // refresh
    }

    void deleteFile() {
        if (!tree->currentItem()) return;
        QString isoItem = getFullPath(tree->currentItem());
        runXorriso({"-dev", isoPath, "-rm", isoItem});
        openIso(); // refresh
    }

    void rebuildIso() {
        QString outFile = QFileDialog::getSaveFileName(this, "Save Rebuilt ISO", "rebuilt.iso");
        if (!outFile.isEmpty()) {
            runXorriso({"-indev", isoPath, "-outdev", outFile, "-commit"});
        }
    }

    QString getFullPath(QTreeWidgetItem *item) {
        QStringList parts;
        while (item) {
            parts.prepend(item->text(0));
            item = item->parent();
        }
        return "/" + parts.join("/");
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    XorrisoIsoManager win;
    win.setWindowTitle("Xorriso ISO Manager");
    win.show();
    return app.exec();
}
