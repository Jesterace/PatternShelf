#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

struct Pattern {
    QString name;
    QString pdfPath;
    QString category;
    QString status;
    int stitchWidth = 0;
    int stitchHeight = 0;
    int fabricCount = 14;
    double borderInches = 2.0;
    QString colors;
    QString notes;
};

static QString dataFilePath() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/patterns.json";
}

class PatternDialog : public QDialog {
public:
    PatternDialog(QWidget *parent = nullptr, const Pattern *existing = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(existing ? "Edit Pattern" : "Add Pattern");
        resize(520, 420);

        nameEdit = new QLineEdit;
        pdfEdit = new QLineEdit;
        categoryEdit = new QLineEdit;

        statusBox = new QComboBox;
        statusBox->addItems({"Backlog", "Not Started", "In Progress", "Finished"});

        widthSpin = new QSpinBox;
        widthSpin->setRange(0, 9999);

        heightSpin = new QSpinBox;
        heightSpin->setRange(0, 9999);

        fabricSpin = new QSpinBox;
        fabricSpin->setRange(6, 40);
        fabricSpin->setValue(14);

        borderSpin = new QDoubleSpinBox;
        borderSpin->setRange(0.0, 6.0);
        borderSpin->setSingleStep(0.5);
        borderSpin->setDecimals(1);
        borderSpin->setSuffix("\"");
        borderSpin->setValue(2.0);

        colorsEdit = new QLineEdit;
        colorsEdit->setPlaceholderText("Example: 310, 3843, 742");

        notesEdit = new QTextEdit;

        auto *browseButton = new QPushButton("Browse...");
        connect(browseButton, &QPushButton::clicked, this, [this]() {
            QString file = QFileDialog::getOpenFileName(
                this,
                "Choose Pattern PDF",
                QDir::homePath(),
                "PDF Files (*.pdf);;All Files (*)"
            );

            if (!file.isEmpty()) {
                pdfEdit->setText(file);
            }
        });

        auto *pdfRow = new QHBoxLayout;
        pdfRow->addWidget(pdfEdit);
        pdfRow->addWidget(browseButton);

        auto *form = new QFormLayout;
        form->addRow("Pattern name:", nameEdit);
        form->addRow("PDF file:", pdfRow);
        form->addRow("Category:", categoryEdit);
        form->addRow("Status:", statusBox);
        form->addRow("Stitch width:", widthSpin);
        form->addRow("Stitch height:", heightSpin);
        form->addRow("Fabric count:", fabricSpin);
        form->addRow("Border size:", borderSpin);
        form->addRow("DMC colors:", colorsEdit);
        form->addRow("Notes:", notesEdit);

        auto *saveButton = new QPushButton("Save");
        auto *cancelButton = new QPushButton("Cancel");

        connect(saveButton, &QPushButton::clicked, this, [this]() {
            if (nameEdit->text().trimmed().isEmpty()) {
                QMessageBox::warning(this, "Missing Name", "Pattern name cannot be empty.");
                return;
            }

            accept();
        });

        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(saveButton);
        buttons->addWidget(cancelButton);

        auto *mainLayout = new QVBoxLayout;
        mainLayout->addLayout(form);
        mainLayout->addLayout(buttons);

        setLayout(mainLayout);

        if (existing) {
            nameEdit->setText(existing->name);
            pdfEdit->setText(existing->pdfPath);
            categoryEdit->setText(existing->category);
            statusBox->setCurrentText(existing->status);
            widthSpin->setValue(existing->stitchWidth);
            heightSpin->setValue(existing->stitchHeight);
            fabricSpin->setValue(existing->fabricCount);
            borderSpin->setValue(existing->borderInches);
            colorsEdit->setText(existing->colors);
            notesEdit->setPlainText(existing->notes);
        }
    }

    Pattern pattern() const {
        Pattern p;
        p.name = nameEdit->text().trimmed();
        p.pdfPath = pdfEdit->text().trimmed();
        p.category = categoryEdit->text().trimmed();
        p.status = statusBox->currentText();
        p.stitchWidth = widthSpin->value();
        p.stitchHeight = heightSpin->value();
        p.fabricCount = fabricSpin->value();
        p.borderInches = borderSpin->value();
        p.colors = colorsEdit->text().trimmed();
        p.notes = notesEdit->toPlainText().trimmed();
        return p;
    }

private:
    QLineEdit *nameEdit;
    QLineEdit *pdfEdit;
    QLineEdit *categoryEdit;
    QComboBox *statusBox;
    QSpinBox *widthSpin;
    QSpinBox *heightSpin;
    QSpinBox *fabricSpin;
    QDoubleSpinBox *borderSpin;
    QLineEdit *colorsEdit;
    QTextEdit *notesEdit;
};

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("JesterPatternShelf v0.4");
        resize(1000, 600);

        auto *central = new QWidget;
        auto *layout = new QVBoxLayout;

        auto *title = new QLabel("<h2>JesterPatternShelf</h2>");
        layout->addWidget(title);

        auto *toolbar = new QHBoxLayout;

        auto *addButton = new QPushButton("Add Pattern");
        auto *importButton = new QPushButton("Import Pattern Folder");
        auto *editButton = new QPushButton("Edit");
        auto *deleteButton = new QPushButton("Delete");
        auto *openButton = new QPushButton("Open PDF");

        toolbar->addWidget(addButton);
        toolbar->addWidget(importButton);
        toolbar->addWidget(editButton);
        toolbar->addWidget(deleteButton);
        toolbar->addWidget(openButton);
        toolbar->addStretch();

        layout->addLayout(toolbar);

        searchEdit = new QLineEdit;
        searchEdit->setPlaceholderText("Search patterns...");
        layout->addWidget(searchEdit);

        table = new QTableWidget;
        table->setColumnCount(9);
        table->setHorizontalHeaderLabels({
            "Name",
            "Status",
            "Category",
            "Stitches",
            "Aida",
            "Border",
            "Fabric Cut",
            "Colors",
            "PDF"
        });

        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        layout->addWidget(table);

        notesLabel = new QLabel("Select a pattern to see notes.");
        notesLabel->setWordWrap(true);
        notesLabel->setMinimumHeight(60);
        layout->addWidget(notesLabel);

        central->setLayout(layout);
        setCentralWidget(central);

        connect(addButton, &QPushButton::clicked, this, [this]() {
            addPattern();
        });

        connect(importButton, &QPushButton::clicked, this, [this]() {
            importPatternFolder();
        });

        connect(editButton, &QPushButton::clicked, this, [this]() {
            editPattern();
        });

        connect(deleteButton, &QPushButton::clicked, this, [this]() {
            deletePattern();
        });

        connect(openButton, &QPushButton::clicked, this, [this]() {
            openPdf();
        });

        connect(searchEdit, &QLineEdit::textChanged, this, [this]() {
            refreshTable();
        });

        connect(table, &QTableWidget::itemSelectionChanged, this, [this]() {
            updateNotes();
        });

        connect(table, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
            openPdf();
        });

        loadPatterns();
        refreshTable();
    }

private:
    QTableWidget *table;
    QLineEdit *searchEdit;
    QLabel *notesLabel;
    QVector<Pattern> patterns;

    void loadPatterns() {
        QFile file(dataFilePath());

        if (!file.exists()) {
            return;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, "Load Error", "Could not read pattern library.");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isArray()) {
            return;
        }

        patterns.clear();

        for (const auto &value : doc.array()) {
            QJsonObject obj = value.toObject();

            Pattern p;
            p.name = obj["name"].toString();
            p.pdfPath = obj["pdfPath"].toString();
            p.category = obj["category"].toString();
            p.status = obj["status"].toString("Backlog");
            p.stitchWidth = obj["stitchWidth"].toInt();
            p.stitchHeight = obj["stitchHeight"].toInt();
            p.fabricCount = obj["fabricCount"].toInt(14);
            p.borderInches = obj["borderInches"].toDouble(2.0);
            p.colors = obj["colors"].toString();
            p.notes = obj["notes"].toString();

            if (!p.name.isEmpty()) {
                patterns.push_back(p);
            }
        }
    }

    void savePatterns() {
        QJsonArray array;

        for (const Pattern &p : patterns) {
            QJsonObject obj;
            obj["name"] = p.name;
            obj["pdfPath"] = p.pdfPath;
            obj["category"] = p.category;
            obj["status"] = p.status;
            obj["stitchWidth"] = p.stitchWidth;
            obj["stitchHeight"] = p.stitchHeight;
            obj["fabricCount"] = p.fabricCount;
            obj["borderInches"] = p.borderInches;
            obj["colors"] = p.colors;
            obj["notes"] = p.notes;
            array.append(obj);
        }

        QFile file(dataFilePath());

        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, "Save Error", "Could not save pattern library.");
            return;
        }

        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
        file.close();
    }

    bool matchesSearch(const Pattern &p, const QString &term) const {
        if (term.isEmpty()) {
            return true;
        }

        QString blob = QString("%1 %2 %3 %4 %5")
            .arg(p.name)
            .arg(p.category)
            .arg(p.status)
            .arg(p.colors)
            .arg(p.notes)
            .toLower();

        return blob.contains(term.toLower());
    }

    QString inchPair(double width, double height) const {
        return QString("%1\" x %2\"")
            .arg(QString::number(width, 'f', 2))
            .arg(QString::number(height, 'f', 2));
    }

    QString designSizeText(const Pattern &p) const {
        if (p.stitchWidth <= 0 || p.stitchHeight <= 0 || p.fabricCount <= 0) {
            return "Unknown";
        }

        double width = static_cast<double>(p.stitchWidth) / p.fabricCount;
        double height = static_cast<double>(p.stitchHeight) / p.fabricCount;

        return inchPair(width, height);
    }

    QString fabricCutText(const Pattern &p) const {
        if (p.stitchWidth <= 0 || p.stitchHeight <= 0 || p.fabricCount <= 0) {
            return "Unknown";
        }

        double designWidth = static_cast<double>(p.stitchWidth) / p.fabricCount;
        double designHeight = static_cast<double>(p.stitchHeight) / p.fabricCount;

        double cutWidth = designWidth + (p.borderInches * 2.0);
        double cutHeight = designHeight + (p.borderInches * 2.0);

        return inchPair(cutWidth, cutHeight);
    }

    void refreshTable() {
        QString term = searchEdit->text().trimmed();

        table->setRowCount(0);

        for (int i = 0; i < patterns.size(); ++i) {
            const Pattern &p = patterns[i];

            if (!matchesSearch(p, term)) {
                continue;
            }

            int row = table->rowCount();
            table->insertRow(row);

            auto *nameItem = new QTableWidgetItem(p.name);
            nameItem->setData(Qt::UserRole, i);

            table->setItem(row, 0, nameItem);
            table->setItem(row, 1, new QTableWidgetItem(p.status));
            table->setItem(row, 2, new QTableWidgetItem(p.category));
            table->setItem(row, 3, new QTableWidgetItem(QString("%1 x %2").arg(p.stitchWidth).arg(p.stitchHeight)));
            table->setItem(row, 4, new QTableWidgetItem(QString::number(p.fabricCount)));
            table->setItem(row, 5, new QTableWidgetItem(QString("%1\"").arg(QString::number(p.borderInches, 'f', 1))));
            table->setItem(row, 6, new QTableWidgetItem(fabricCutText(p)));
            table->setItem(row, 7, new QTableWidgetItem(p.colors));
            table->setItem(row, 8, new QTableWidgetItem(p.pdfPath));
        }

        updateNotes();
    }

    int selectedPatternIndex() const {
        QList<QTableWidgetItem *> selected = table->selectedItems();

        if (selected.isEmpty()) {
            return -1;
        }

        int row = selected.first()->row();
        QTableWidgetItem *nameItem = table->item(row, 0);

        if (!nameItem) {
            return -1;
        }

        return nameItem->data(Qt::UserRole).toInt();
    }

    void updateNotes() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            notesLabel->setText("Select a pattern to see notes.");
            return;
        }

        const Pattern &p = patterns[index];

        QString text = QString("<b>%1</b><br>Status: %2<br>Stitches: %3 x %4<br>Design size: %5<br>Fabric cut: %6 with %7\" border<br>Notes: %8")
            .arg(p.name.toHtmlEscaped())
            .arg(p.status.toHtmlEscaped())
            .arg(p.stitchWidth)
            .arg(p.stitchHeight)
            .arg(designSizeText(p).toHtmlEscaped())
            .arg(fabricCutText(p).toHtmlEscaped())
            .arg(QString::number(p.borderInches, 'f', 1))
            .arg(p.notes.isEmpty() ? "No notes." : p.notes.toHtmlEscaped());

        notesLabel->setText(text);
    }

    bool hasPdfPath(const QString &path) const {
        for (const Pattern &p : patterns) {
            if (QFileInfo(p.pdfPath).absoluteFilePath() == QFileInfo(path).absoluteFilePath()) {
                return true;
            }
        }

        return false;
    }

    QString niceNameFromPdf(const QString &path) const {
        QString name = QFileInfo(path).completeBaseName();
        name.replace("_", " ");
        name.replace("-", " ");

        QStringList words = name.split(" ", Qt::SkipEmptyParts);
        for (QString &word : words) {
            if (!word.isEmpty()) {
                word[0] = word[0].toUpper();
            }
        }

        return words.join(" ");
    }

    void importPatternFolder() {
        QString folder = QFileDialog::getExistingDirectory(
            this,
            "Import Pattern Folder",
            QDir::homePath()
        );

        if (folder.isEmpty()) {
            return;
        }

        QStringList filters;
        filters << "*.pdf" << "*.PDF";

        QDirIterator it(folder, filters, QDir::Files, QDirIterator::Subdirectories);

        int imported = 0;
        int skipped = 0;

        while (it.hasNext()) {
            QString pdfPath = it.next();

            if (hasPdfPath(pdfPath)) {
                skipped++;
                continue;
            }

            QFileInfo info(pdfPath);

            Pattern pattern;
            pattern.name = niceNameFromPdf(pdfPath);
            pattern.pdfPath = info.absoluteFilePath();
            pattern.category = info.dir().dirName();
            pattern.status = "Backlog";
            pattern.stitchWidth = 0;
            pattern.stitchHeight = 0;
            pattern.fabricCount = 14;
            pattern.borderInches = 2.0;
            pattern.colors = "";
            pattern.notes = QString("Imported from %1").arg(folder);

            patterns.push_back(pattern);
            imported++;
        }

        if (imported > 0) {
            savePatterns();
            refreshTable();
        }

        QMessageBox::information(
            this,
            "Import Complete",
            QString("Imported %1 pattern PDF(s).\nSkipped %2 duplicate PDF(s).")
                .arg(imported)
                .arg(skipped)
        );
    }

    void addPattern() {
        PatternDialog dialog(this);

        if (dialog.exec() == QDialog::Accepted) {
            patterns.push_back(dialog.pattern());
            savePatterns();
            refreshTable();
        }
    }

    void editPattern() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            QMessageBox::information(this, "No Pattern Selected", "Select a pattern first.");
            return;
        }

        PatternDialog dialog(this, &patterns[index]);

        if (dialog.exec() == QDialog::Accepted) {
            patterns[index] = dialog.pattern();
            savePatterns();
            refreshTable();
        }
    }

    void deletePattern() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            QMessageBox::information(this, "No Pattern Selected", "Select a pattern first.");
            return;
        }

        int answer = QMessageBox::question(
            this,
            "Delete Pattern",
            "Delete this pattern from the shelf?\n\nThis will not delete the PDF file."
        );

        if (answer == QMessageBox::Yes) {
            patterns.removeAt(index);
            savePatterns();
            refreshTable();
        }
    }

    void openPdf() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            QMessageBox::information(this, "No Pattern Selected", "Select a pattern first.");
            return;
        }

        QString path = patterns[index].pdfPath;

        if (path.isEmpty()) {
            QMessageBox::information(this, "No PDF", "This pattern does not have a PDF file set.");
            return;
        }

        if (!QFile::exists(path)) {
            QMessageBox::warning(this, "PDF Missing", "The PDF file could not be found.");
            return;
        }

        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QApplication::setApplicationName("JesterPatternShelf");
    QApplication::setOrganizationName("Jesterace");

    MainWindow window;
    window.show();

    return app.exec();
}
