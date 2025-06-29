#include <QApplication>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QDir>

extern "C" {
    #include <libisofs/libisofs.h>
}

class IsoManager : public QWidget {
    Q_OBJECT

    QListWidget *fileList;
    QStringList addedFiles;

public:
    IsoManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("ISO Manager (libisofs)");

        QVBoxLayout *layout = new QVBoxLayout(this);
        fileList = new QListWidget(this);
        layout->addWidget(fileList);

        QPushButton *btnAdd = new QPushButton("Add File(s)", this);
        QPushButton *btnRemove = new QPushButton("Remove Selected", this);
        QPushButton *btnSave = new QPushButton("Save ISO", this);

        layout->addWidget(btnAdd);
        layout->addWidget(btnRemove);
        layout->addWidget(btnSave);

        connect(btnAdd, &QPushButton::clicked, this, &IsoManager::addFiles);
        connect(btnRemove, &QPushButton::clicked, this, &IsoManager::removeSelected);
        connect(btnSave, &QPushButton::clicked, this, &IsoManager::saveIso);
    }

private slots:
    void addFiles() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Add files");
        for (const QString &file : files) {
            if (!addedFiles.contains(file)) {
                addedFiles << file;
                fileList->addItem(file);
            }
        }
    }

    void removeSelected() {
        QListWidgetItem *item = fileList->currentItem();
        if (!item) return;
        QString path = item->text();
        addedFiles.removeAll(path);
        delete item;
    }

    void saveIso() {
        if (addedFiles.isEmpty()) {
            QMessageBox::warning(this, "No Files", "No files to save.");
            return;
        }

        QString isoPath = QFileDialog::getSaveFileName(this, "Save ISO", "", "*.iso");
        if (isoPath.isEmpty()) return;

        IsoImage *image = nullptr;
        IsoWriteOpts *opts = nullptr;
        IsoNode *root = nullptr;
        IsoFilesystem *fs = nullptr;

        iso_init();
        iso_image_new("CustomISO", &image);
      //  iso_image_get_filesystem(image, &fs);
        iso_image_get_root(image, &root);

        for (const QString &filePath : addedFiles) {
            QFileInfo fi(filePath);
            QByteArray pathUtf8 = filePath.toUtf8();
            QByteArray nameUtf8 = fi.fileName().toUtf8();

            IsoNode *node;
            int ret = iso_node_from_path(fs, pathUtf8.data(), &node);
            if (ret < 0) {
                QMessageBox::critical(this, "Error", "Failed to read file: " + filePath);
                return;
            }

            iso_node_set_name(node, nameUtf8.data());
            iso_dir_add_node(root, node);
        }

        iso_write_opts_new(&opts, 1);
        QFile isoFile(isoPath);
        if (!isoFile.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "Error", "Failed to create ISO file.");
            return;
        }

        int fd = isoFile.handle();  // get raw POSIX file descriptor
        int ret = iso_image_write(image, fd, opts);
        if (ret < 0) {
            QMessageBox::critical(this, "Error", "Failed to write ISO.");
        } else {
            QMessageBox::information(this, "Success", "ISO written successfully.");
        }

        iso_write_opts_free(opts);
        iso_image_unref(image);
        iso_finish();
    }
};

#include <main.moc>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    IsoManager w;
    w.resize(500, 400);
    w.show();
    return app.exec();
}
