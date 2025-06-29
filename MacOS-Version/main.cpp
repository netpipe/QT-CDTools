#include <QtWidgets>
#include <QProcess>
#include <QTemporaryDir>
#include <QFileInfo>

class IsoManager : public QWidget {
    Q_OBJECT
public:
    IsoManager() {
        auto mainLayout = new QVBoxLayout(this);

        // Controls
        auto btnLayout = new QHBoxLayout;
        openBtn = new QPushButton("Open ISO");
        mountBtn = new QPushButton("Mount ISO");
        unmountBtn = new QPushButton("Unmount ISO");
        extractBtn = new QPushButton("Extract File(s)");
        deleteBtn = new QPushButton("Delete File(s)");
        rebuildBtn = new QPushButton("Rebuild ISO");

        volumeLabelEdit = new QLineEdit("UPDATED_ISO");
        volumeLabelEdit->setMaximumWidth(150);
        bootableCheck = new QCheckBox("Make Bootable");

        btnLayout->addWidget(openBtn);
        btnLayout->addWidget(mountBtn);
        btnLayout->addWidget(unmountBtn);
        btnLayout->addWidget(extractBtn);
        btnLayout->addWidget(deleteBtn);
        btnLayout->addWidget(new QLabel("Volume Label:"));
        btnLayout->addWidget(volumeLabelEdit);
        btnLayout->addWidget(bootableCheck);
        btnLayout->addWidget(rebuildBtn);

        treeView = new QTreeWidget;
        treeView->setHeaderLabels({"Name"});
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        treeView->setDragDropMode(QAbstractItemView::DropOnly);
        treeView->setAcceptDrops(true);

        statusLabel = new QLabel("Ready");

        mainLayout->addLayout(btnLayout);
        mainLayout->addWidget(treeView);
        mainLayout->addWidget(statusLabel);

        mountBtn->setEnabled(false);
        unmountBtn->setEnabled(false);
        extractBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
        rebuildBtn->setEnabled(false);

        connect(openBtn, &QPushButton::clicked, this, &IsoManager::openIsoFile);
        connect(mountBtn, &QPushButton::clicked, this, &IsoManager::mountIso);
        connect(unmountBtn, &QPushButton::clicked, this, &IsoManager::unmountIso);
        connect(extractBtn, &QPushButton::clicked, this, &IsoManager::extractSelectedFiles);
        connect(deleteBtn, &QPushButton::clicked, this, &IsoManager::deleteSelectedFiles);
        connect(rebuildBtn, &QPushButton::clicked, this, &IsoManager::rebuildIso);
        connect(treeView, &QTreeWidget::itemSelectionChanged, this, [this]() {
            bool hasSelection = !treeView->selectedItems().isEmpty();
            extractBtn->setEnabled(hasSelection);
            deleteBtn->setEnabled(hasSelection);
        });

        setAcceptDrops(true);
        mounted = false;
    }

    ~IsoManager() {
        if (mounted) unmountIso();
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString localPath = url.toLocalFile();
            QFileInfo fi(localPath);
            if (fi.exists()) {
                // Ask user to specify path inside ISO or add to root for now
                bool ok;
                QString relPath = QInputDialog::getText(this, "Add File",
                                                        "Enter relative path inside ISO (e.g. folder/file.txt):",
                                                        QLineEdit::Normal, fi.fileName(), &ok);
                if (ok && !relPath.isEmpty()) {
                    addOrReplaceFile(localPath, relPath);
                } else {
                    addOrReplaceFile(localPath, fi.fileName());
                }
            }
        }
        event->acceptProposedAction();
    }

private slots:
    void openIsoFile() {
        QString iso = QFileDialog::getOpenFileName(this, "Open ISO file", "", "*.iso");
        if (iso.isEmpty()) return;
        isoFilePath = iso;
        statusLabel->setText("ISO selected: " + iso);
        mountBtn->setEnabled(true);
        unmountBtn->setEnabled(false);
        extractBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
        rebuildBtn->setEnabled(false);
        clearTree();
        modifiedFiles.clear();
        deletedFiles.clear();
        mounted = false;
    }

    void mountIso() {
        if (isoFilePath.isEmpty()) return;

        if (mounted) {
            statusLabel->setText("Already mounted");
            return;
        }

        mountPoint = QDir::tempPath() + "/iso_mnt_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(mountPoint);

        QProcess proc;
        proc.start("hdiutil", {"attach", isoFilePath, "-mountpoint", mountPoint, "-readonly"});
        proc.waitForFinished();
        if (proc.exitCode() != 0) {
            QMessageBox::critical(this, "Mount failed", proc.readAllStandardError());
            return;
        }

        mounted = true;
        statusLabel->setText("Mounted at: " + mountPoint);
        mountBtn->setEnabled(false);
        unmountBtn->setEnabled(true);
        rebuildBtn->setEnabled(true);

        loadDirectoryTree();
    }

    void unmountIso() {
        if (!mounted) return;

        QProcess proc;
        proc.start("hdiutil", {"detach", mountPoint});
        proc.waitForFinished();

        if (proc.exitCode() == 0) {
            mounted = false;
            mountBtn->setEnabled(true);
            unmountBtn->setEnabled(false);
            extractBtn->setEnabled(false);
            deleteBtn->setEnabled(false);
            rebuildBtn->setEnabled(false);
            clearTree();
            modifiedFiles.clear();
            deletedFiles.clear();
            statusLabel->setText("ISO unmounted");
        } else {
            QMessageBox::critical(this, "Unmount failed", proc.readAllStandardError());
        }
    }

    void loadDirectoryTree() {
        clearTree();
        addDirectoryItems(nullptr, mountPoint);
    }

    void addDirectoryItems(QTreeWidgetItem *parent, const QString &path) {
        QDir dir(path);
        QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &fi : list) {
            // Skip deleted files
            QString relPath = relativePath(fi.absoluteFilePath());
            if (deletedFiles.contains(relPath)) continue;

            QTreeWidgetItem *item = nullptr;
            if (parent) {
                item = new QTreeWidgetItem(parent, QStringList() << fi.fileName());
                parent->addChild(item);
            } else {
                item = new QTreeWidgetItem(treeView, QStringList() << fi.fileName());
                treeView->addTopLevelItem(item);
            }
            item->setData(0, Qt::UserRole, fi.absoluteFilePath());
            item->setData(0, Qt::UserRole + 1, relPath); // store relative path for tracking

            if (fi.isDir()) {
                addDirectoryItems(item, fi.absoluteFilePath());
            }

            // Mark replaced files in tree
            if (modifiedFiles.contains(relPath)) {
                QFont f = item->font(0);
                f.setItalic(true);
                item->setFont(0, f);
                item->setForeground(0, QBrush(Qt::darkGreen));
            }
        }
        treeView->expandAll();

        // Also show newly added files not in original ISO
        for (auto it = modifiedFiles.constBegin(); it != modifiedFiles.constEnd(); ++it) {
            if (!fileExistsInTree(it.key())) {
                addFileToTree(it.key());
            }
        }
    }

    bool fileExistsInTree(const QString &relPath) {
        QList<QTreeWidgetItem *> allItems;
        for (int i=0; i < treeView->topLevelItemCount(); i++) {
            collectAllItems(treeView->topLevelItem(i), allItems);
        }
        for (QTreeWidgetItem *item : allItems) {
            if (item->data(0, Qt::UserRole + 1).toString() == relPath) return true;
        }
        return false;
    }

    void collectAllItems(QTreeWidgetItem *parent, QList<QTreeWidgetItem*> &out) {
        out.append(parent);
        for (int i=0; i < parent->childCount(); ++i)
            collectAllItems(parent->child(i), out);
    }

    void addFileToTree(const QString &relPath) {
        QStringList parts = relPath.split('/');
        QTreeWidgetItem *parent = nullptr;
        QList<QTreeWidgetItem*> currentLevel;

        // Find or create the top-level item
        currentLevel = treeView->findItems(parts[0], Qt::MatchExactly);
        QTreeWidgetItem *item = nullptr;
        if (!currentLevel.isEmpty()) {
            item = currentLevel[0];
        } else {
            item = new QTreeWidgetItem(treeView, QStringList() << parts[0]);
            treeView->addTopLevelItem(item);
        }
        parent = item;

        // Create intermediate folders and file item
        for (int i = 1; i < parts.size(); ++i) {
            bool foundChild = false;
            for (int c = 0; c < parent->childCount(); ++c) {
                if (parent->child(c)->text(0) == parts[i]) {
                    parent = parent->child(c);
                    foundChild = true;
                    break;
                }
            }
            if (!foundChild) {
                QTreeWidgetItem *newItem = new QTreeWidgetItem(parent, QStringList() << parts[i]);
                parent->addChild(newItem);
                parent = newItem;
            }
        }

        // Store the relative path for this newly added item
        parent->setData(0, Qt::UserRole + 1, relPath);

        // Mark it as modified (newly added)
        QFont f = parent->font(0);
        f.setItalic(true);
        parent->setFont(0, f);
        parent->setForeground(0, QBrush(Qt::darkBlue));
    }

    QString relativePath(const QString &absPath) const {
        if (absPath.startsWith(mountPoint))
            return absPath.mid(mountPoint.length() + 1);
        return absPath;
    }

    void addOrReplaceFile(const QString &sourcePath, const QString &relPath) {
        // Mark file for addition/replacement
        modifiedFiles[relPath] = sourcePath;
        deletedFiles.remove(relPath); // If previously marked for deletion, unmark it

        statusLabel->setText(QString("Added/Replaced: %1").arg(relPath));
        loadDirectoryTree(); // refresh tree view
        rebuildBtn->setEnabled(true);
    }

    void extractSelectedFiles() {
        QList<QTreeWidgetItem*> items = treeView->selectedItems();
        if (items.isEmpty()) return;

        QString targetDir = QFileDialog::getExistingDirectory(this, "Select extraction folder");
        if (targetDir.isEmpty()) return;

        for (QTreeWidgetItem *item : items) {
            QString relPath = item->data(0, Qt::UserRole + 1).toString();
            QString srcPath;
            if (modifiedFiles.contains(relPath)) {
                srcPath = modifiedFiles[relPath];
            } else {
                srcPath = mountPoint + "/" + relPath;
            }
            QFileInfo fi(srcPath);
            if (!fi.exists()) continue;

            QString destPath = QDir(targetDir).filePath(relPath);
            QDir().mkpath(QFileInfo(destPath).path());
            QFile::copy(srcPath, destPath);
        }
        statusLabel->setText("Selected files extracted.");
    }

    void deleteSelectedFiles() {
        QList<QTreeWidgetItem*> items = treeView->selectedItems();
        if (items.isEmpty()) return;

        for (QTreeWidgetItem *item : items) {
            QString relPath = item->data(0, Qt::UserRole + 1).toString();
            deletedFiles.insert(relPath);
            modifiedFiles.remove(relPath);
        }
        statusLabel->setText("Selected files marked for deletion.");
        loadDirectoryTree();
        rebuildBtn->setEnabled(true);
    }

    void rebuildIso() {
        if (!mounted) {
            QMessageBox::warning(this, "Rebuild ISO", "Please mount an ISO first.");
            return;
        }

        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            QMessageBox::critical(this, "Error", "Failed to create temporary directory.");
            return;
        }

        QString tempPath = tempDir.path();

        // Copy mounted ISO contents except deleted files
        copyDirectoryFiltered(mountPoint, tempPath, deletedFiles);

        // Apply modified files
        for (auto it = modifiedFiles.constBegin(); it != modifiedFiles.constEnd(); ++it) {
            QString destFile = QDir(tempPath).filePath(it.key());
            QDir().mkpath(QFileInfo(destFile).path());
            QFile::remove(destFile); // remove old if exists
            QFile::copy(it.value(), destFile);
        }

        QString outIso = QFileDialog::getSaveFileName(this, "Save new ISO", "updated.iso", "*.iso");
        if (outIso.isEmpty()) return;

        // Prepare hdiutil arguments
        QString volLabel = volumeLabelEdit->text().trimmed();
        if (volLabel.isEmpty()) volLabel = "NEW_ISO";

        QStringList args;
        args << "-o" << outIso
             << "-hfs" << "-joliet" << "-iso"
             << "-default-volume-name" << volLabel;

        if (bootableCheck->isChecked()) {
            // For bootable ISO, user should provide boot image or logic here to add it
            // This is a placeholder, you may want to customize
            QMessageBox::information(this, "Bootable ISO",
                                     "Bootable ISO option selected, but boot image handling is not implemented.");
        }

        args << tempPath;

        QProcess proc;
        statusLabel->setText("Building ISO...");
        proc.start("hdiutil", args);
        proc.waitForFinished(-1);

        if (proc.exitCode() == 0) {
            statusLabel->setText("ISO rebuilt successfully: " + outIso);
            isoFilePath = outIso;
            rebuildBtn->setEnabled(false);
            modifiedFiles.clear();
            deletedFiles.clear();
            mountBtn->setEnabled(true);
            unmountBtn->setEnabled(false);
            clearTree();
        } else {
            QString err = proc.readAllStandardError();
            QMessageBox::critical(this, "Error building ISO", err);
            statusLabel->setText("Failed to build ISO.");
        }
    }

    void copyDirectoryFiltered(const QString &srcPath, const QString &dstPath, const QSet<QString> &excludeFiles) {
        QDir srcDir(srcPath);
        QDir dstDir(dstPath);

        if (!dstDir.exists()) dstDir.mkpath(".");

        QFileInfoList entries = srcDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
        for (const QFileInfo &entry : entries) {
            QString rel = relativePath(entry.absoluteFilePath());
            if (excludeFiles.contains(rel)) continue;

            QString dstFilePath = dstDir.filePath(entry.fileName());

            if (entry.isDir()) {
                copyDirectoryFiltered(entry.absoluteFilePath(), dstFilePath, excludeFiles);
            } else if (entry.isFile()) {
                QFile::copy(entry.absoluteFilePath(), dstFilePath);
            }
        }
    }

    void clearTree() {
        treeView->clear();
    }

private:
    QPushButton *openBtn, *mountBtn, *unmountBtn, *extractBtn, *deleteBtn, *rebuildBtn;
    QTreeWidget *treeView;
    QLabel *statusLabel;
    QLineEdit *volumeLabelEdit;
    QCheckBox *bootableCheck;

    QString isoFilePath;
    QString mountPoint;
    bool mounted;

    QHash<QString, QString> modifiedFiles; // rel path -> source path
    QSet<QString> deletedFiles;
};

#include "main.moc"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    IsoManager w;
    w.setWindowTitle("ISO Manager");
    w.resize(900, 600);
    w.show();
    return app.exec();
}
