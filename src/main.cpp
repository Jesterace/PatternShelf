#include <QApplication>
#include <QAction>
#include <QClipboard>
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
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
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

static QString normalizeDmcColorToken(const QString &input) {
    QRegularExpression rx(
        "^\\s*(?:DMC\\s*)?(B5200|BLANC|WHITE|ECRU|[0-9]{1,4})\\b",
        QRegularExpression::CaseInsensitiveOption
    );

    QRegularExpressionMatch match = rx.match(input);

    if (!match.hasMatch()) {
        return "";
    }

    QString color = match.captured(1).trimmed().toUpper();

    if (color == "WHITE") {
        color = "BLANC";
    }

    return color;
}

static QStringList parseDmcColors(const QString &input) {
    QStringList colors;

    QRegularExpression rx(
        "\\b(?:DMC\\s*)?(B5200|BLANC|WHITE|ECRU|[0-9]{1,4})\\b",
        QRegularExpression::CaseInsensitiveOption
    );

    QRegularExpressionMatchIterator it = rx.globalMatch(input);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString color = match.captured(1).trimmed().toUpper();

        if (color == "WHITE") {
            color = "BLANC";
        }

        if (!color.isEmpty() && !colors.contains(color)) {
            colors.append(color);
        }
    }

    return colors;
}

static QString normalizedDmcColors(const QString &input) {
    return parseDmcColors(input).join(", ");
}

static QString dmcColorCountText(const QString &input) {
    return QString::number(parseDmcColors(input).size());
}

static QString builtInDefaultStashPath() {
    return QDir::homePath() + "/FlossKeeperSync/flosskeeper_collection.tsv";
}

static QString configuredStashPath() {
    QSettings settings("Jesterace", "PatternShelf");
    return settings.value("stash/path", builtInDefaultStashPath()).toString();
}

static void saveConfiguredStashPath(const QString &path) {
    QSettings settings("Jesterace", "PatternShelf");
    settings.setValue("stash/path", path);
}

static QSet<QString> loadOwnedDmcColorsFromFile(const QString &path) {
    QSet<QString> owned;

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return owned;
    }

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();

        if (line.isEmpty()) {
            continue;
        }

        QStringList fields = line.split('\t');

        for (int i = 0; i < fields.size() && i < 3; ++i) {
            QString color = normalizeDmcColorToken(fields[i]);

            if (!color.isEmpty()) {
                owned.insert(color);
                break;
            }
        }
    }

    return owned;
}

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
        colorsEdit->setPlaceholderText("Example: 310, 3843, 742, B5200");

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
        p.colors = normalizedDmcColors(colorsEdit->text());
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
        setWindowTitle("PatternShelf v1.4");
        setWindowIcon(QIcon::fromTheme("patternshelf"));
        resize(1000, 600);

        createMenus();

        auto *central = new QWidget;
        auto *layout = new QVBoxLayout;

        auto *title = new QLabel("<h2>PatternShelf</h2>");
        layout->addWidget(title);

        auto *toolbar = new QHBoxLayout;

        auto *addButton = new QPushButton("Add Pattern");
        auto *importButton = new QPushButton("Import Pattern Folder");
        auto *reloadStashButton = new QPushButton("Reload Stash");
        auto *setStashPathButton = new QPushButton("Set Stash Path");
        auto *importCsvColorsButton = new QPushButton("Import Colors from CSV");
        auto *copyBuyButton = new QPushButton("Copy Need-to-Buy List");
        auto *editButton = new QPushButton("Edit");
        auto *deleteButton = new QPushButton("Delete");
        auto *openButton = new QPushButton("Open PDF");

        toolbar->addWidget(addButton);
        toolbar->addWidget(importButton);
        toolbar->addWidget(reloadStashButton);
        toolbar->addWidget(setStashPathButton);
        toolbar->addWidget(importCsvColorsButton);
        toolbar->addWidget(copyBuyButton);
        toolbar->addWidget(editButton);
        toolbar->addWidget(deleteButton);
        toolbar->addWidget(openButton);
        toolbar->addStretch();

        layout->addLayout(toolbar);

        auto *filterRow = new QHBoxLayout;

        searchEdit = new QLineEdit;
        searchEdit->setPlaceholderText("Search patterns...");

        filterBox = new QComboBox;
        filterBox->addItems({
            "All Patterns",
            "Backlog",
            "Not Started",
            "In Progress",
            "Finished",
            "Missing Colors",
            "Ready to Stitch"
        });

        filterRow->addWidget(searchEdit);
        filterRow->addWidget(filterBox);

        layout->addLayout(filterRow);

        stashLabel = new QLabel;
        stashLabel->setWordWrap(true);
        layout->addWidget(stashLabel);

        table = new QTableWidget;
        table->setColumnCount(12);
        table->setHorizontalHeaderLabels({
            "Name",
            "Status",
            "Category",
            "Stitches",
            "Aida",
            "Border",
            "Fabric Cut",
            "Color Count",
            "Have",
            "Missing",
            "DMC Colors",
            "PDF"
        });

        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        layout->addWidget(table);

        detailsText = new QTextEdit;
        detailsText->setReadOnly(true);
        detailsText->setMinimumHeight(150);
        detailsText->setPlaceholderText("Select a pattern to see details.");
        layout->addWidget(detailsText);

        central->setLayout(layout);
        setCentralWidget(central);

        connect(addButton, &QPushButton::clicked, this, [this]() {
            addPattern();
        });

        connect(importButton, &QPushButton::clicked, this, [this]() {
            importPatternFolder();
        });

        connect(reloadStashButton, &QPushButton::clicked, this, [this]() {
            reloadStash();
        });

        connect(setStashPathButton, &QPushButton::clicked, this, [this]() {
            chooseStashPath();
        });

        connect(importCsvColorsButton, &QPushButton::clicked, this, [this]() {
            importColorsFromCsvForSelected();
        });

        connect(copyBuyButton, &QPushButton::clicked, this, [this]() {
            copyNeedToBuyList();
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

        connect(filterBox, &QComboBox::currentTextChanged, this, [this]() {
            refreshTable();
        });

        connect(table, &QTableWidget::itemSelectionChanged, this, [this]() {
            updateNotes();
        });

        connect(table, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
            openPdf();
        });

        loadPatterns();
        reloadStash(false);
        refreshTable();
    }

private:
    void createMenus() {
        auto *fileMenu = menuBar()->addMenu("&File");

        auto *openPdfAction = fileMenu->addAction("&Open Selected PDF");
        connect(openPdfAction, &QAction::triggered, this, [this]() {
            openPdf();
        });

        fileMenu->addSeparator();

        auto *quitAction = fileMenu->addAction("&Quit");
        connect(quitAction, &QAction::triggered, this, [this]() {
            close();
        });

        auto *helpMenu = menuBar()->addMenu("&Help");

        auto *aboutAction = helpMenu->addAction("&About PatternShelf");
        connect(aboutAction, &QAction::triggered, this, [this]() {
            QMessageBox::about(
                this,
                "About PatternShelf",
                "<h3>PatternShelf v1.4</h3>"
                "<p>A personal cross-stitch pattern library manager.</p>"
                "<p>Tracks pattern PDFs, stitch sizes, fabric cuts, DMC colors, "
                "FlossKeeper stash matches, missing colors, and need-to-buy lists.</p>"
            );
        });

        auto *aboutQtAction = helpMenu->addAction("About &Qt");
        connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    }

    QTableWidget *table;
    QLineEdit *searchEdit;
    QComboBox *filterBox;
    QTextEdit *detailsText;
    QLabel *stashLabel;
    QVector<Pattern> patterns;
    QSet<QString> ownedDmcColors;

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

    bool matchesFilter(const Pattern &p) const {
        if (!filterBox) {
            return true;
        }

        QString filter = filterBox->currentText();

        if (filter == "All Patterns") {
            return true;
        }

        if (filter == "Missing Colors") {
            if (ownedDmcColors.isEmpty()) {
                return false;
            }

            return !parseDmcColors(p.colors).isEmpty() && !missingColorsFor(p).isEmpty();
        }

        if (filter == "Ready to Stitch") {
            if (ownedDmcColors.isEmpty()) {
                return false;
            }

            return !parseDmcColors(p.colors).isEmpty() && missingColorsFor(p).isEmpty();
        }

        return p.status == filter;
    }

    void chooseStashPath() {
        QString currentPath = configuredStashPath();
        QFileInfo currentInfo(currentPath);

        QString startDir = currentInfo.exists()
            ? currentInfo.absolutePath()
            : QDir::homePath();

        QString path = QFileDialog::getOpenFileName(
            this,
            "Choose FlossKeeper Stash TSV",
            startDir,
            "TSV Files (*.tsv);;Text Files (*.txt);;All Files (*)"
        );

        if (path.isEmpty()) {
            return;
        }

        saveConfiguredStashPath(path);
        reloadStash(false);

        QMessageBox::information(
            this,
            "Stash Path Updated",
            QString("FlossKeeper stash path set to:\n%1").arg(path)
        );
    }

    void updateStashLabel() {
        QString path = configuredStashPath();

        if (!QFileInfo::exists(path)) {
            stashLabel->setText(QString("FlossKeeper stash not found: %1").arg(path.toHtmlEscaped()));
            return;
        }

        stashLabel->setText(
            QString("FlossKeeper stash loaded: %1 owned DMC color(s) from %2")
                .arg(ownedDmcColors.size())
                .arg(path.toHtmlEscaped())
        );
    }

    void reloadStash(bool showMessage = true) {
        ownedDmcColors = loadOwnedDmcColorsFromFile(configuredStashPath());
        updateStashLabel();
        refreshTable();

        if (showMessage) {
            QMessageBox::information(
                this,
                "Stash Reloaded",
                QString("Loaded %1 owned DMC color(s).").arg(ownedDmcColors.size())
            );
        }
    }

    QStringList missingColorsFor(const Pattern &p) const {
        QStringList missing;

        if (ownedDmcColors.isEmpty()) {
            return missing;
        }

        for (const QString &color : parseDmcColors(p.colors)) {
            if (!ownedDmcColors.contains(color)) {
                missing.append(color);
            }
        }

        return missing;
    }

    int haveColorCountFor(const Pattern &p) const {
        if (ownedDmcColors.isEmpty()) {
            return -1;
        }

        int have = 0;

        for (const QString &color : parseDmcColors(p.colors)) {
            if (ownedDmcColors.contains(color)) {
                have++;
            }
        }

        return have;
    }

    QString haveColorCountText(const Pattern &p) const {
        int count = haveColorCountFor(p);

        if (count < 0) {
            return "—";
        }

        return QString::number(count);
    }

    QString missingColorCountText(const Pattern &p) const {
        if (ownedDmcColors.isEmpty()) {
            return "—";
        }

        return QString::number(missingColorsFor(p).size());
    }

    QString missingColorListText(const Pattern &p) const {
        if (ownedDmcColors.isEmpty()) {
            return "Stash not loaded.";
        }

        QStringList missing = missingColorsFor(p);

        if (missing.isEmpty()) {
            return "None.";
        }

        return missing.join(", ");
    }

    void importColorsFromCsvForSelected() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            QMessageBox::information(this, "No Pattern Selected", "Select a pattern first.");
            return;
        }

        QString csvPath = QFileDialog::getOpenFileName(
            this,
            "Import DMC Colors from CSV",
            QDir::homePath(),
            "CSV Files (*.csv);;All Files (*)"
        );

        if (csvPath.isEmpty()) {
            return;
        }

        QStringList importedColors = colorsFromCsvFile(csvPath);

        if (importedColors.isEmpty()) {
            QMessageBox::warning(
                this,
                "No DMC Colors Found",
                "PatternShelf could not find any DMC colors in that CSV file."
            );
            return;
        }

        QString before = patterns[index].colors;
        QString merged = normalizedDmcColors(before + ", " + importedColors.join(", "));

        patterns[index].colors = merged;

        QFileInfo info(csvPath);
        QString noteLine = QString("DMC colors imported from CSV: %1").arg(info.fileName());

        if (!patterns[index].notes.contains(noteLine)) {
            if (!patterns[index].notes.trimmed().isEmpty()) {
                patterns[index].notes += "\n";
            }

            patterns[index].notes += noteLine;
        }

        savePatterns();
        refreshTable();

        QMessageBox::information(
            this,
            "DMC Colors Imported",
            QString("Imported/merged %1 DMC color(s) from CSV.\nTotal colors for this pattern: %2")
                .arg(importedColors.size())
                .arg(parseDmcColors(patterns[index].colors).size())
        );
    }

    void copyNeedToBuyList() {
        int index = selectedPatternIndex();

        if (index < 0 || index >= patterns.size()) {
            QMessageBox::information(this, "No Pattern Selected", "Select a pattern first.");
            return;
        }

        const Pattern &p = patterns[index];
        QStringList patternColors = parseDmcColors(p.colors);

        if (patternColors.isEmpty()) {
            QMessageBox::information(
                this,
                "No DMC Colors",
                "This pattern does not have any DMC colors listed yet."
            );
            return;
        }

        if (ownedDmcColors.isEmpty()) {
            QMessageBox::information(
                this,
                "Stash Not Loaded",
                "No FlossKeeper stash colors are loaded. Check the stash path or click Reload Stash."
            );
            return;
        }

        QStringList missing = missingColorsFor(p);

        if (missing.isEmpty()) {
            QApplication::clipboard()->setText(QString("No missing DMC colors for %1.").arg(p.name));

            QMessageBox::information(
                this,
                "Nothing Missing",
                "You already have all listed DMC colors for this pattern."
            );
            return;
        }

        QString text = QString("Need to buy for %1:\n%2")
            .arg(p.name)
            .arg(missing.join("\n"));

        QApplication::clipboard()->setText(text);

        QMessageBox::information(
            this,
            "Copied",
            QString("Copied %1 missing DMC color(s) to the clipboard.").arg(missing.size())
        );
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

            if (!matchesSearch(p, term) || !matchesFilter(p)) {
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
            table->setItem(row, 7, new QTableWidgetItem(dmcColorCountText(p.colors)));
            table->setItem(row, 8, new QTableWidgetItem(haveColorCountText(p)));
            table->setItem(row, 9, new QTableWidgetItem(missingColorCountText(p)));
            table->setItem(row, 10, new QTableWidgetItem(p.colors));
            table->setItem(row, 11, new QTableWidgetItem(p.pdfPath));
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
            detailsText->setPlainText("Select a pattern to see details.");
            return;
        }

        const Pattern &p = patterns[index];

        QString colorText = p.colors.isEmpty() ? "None listed." : p.colors.toHtmlEscaped();
        QString haveText = haveColorCountText(p);
        QString missingCountText = missingColorCountText(p);
        QString missingList = missingColorListText(p).toHtmlEscaped();

        QString notesText = p.notes.isEmpty() ? "No notes." : p.notes.toHtmlEscaped();

        QString text = QString(
            "<h3>%1</h3>"
            "<table cellspacing='4'>"
            "<tr><td><b>Status:</b></td><td>%2</td></tr>"
            "<tr><td><b>Category:</b></td><td>%3</td></tr>"
            "<tr><td><b>Stitches:</b></td><td>%4 x %5</td></tr>"
            "<tr><td><b>Aida count:</b></td><td>%6</td></tr>"
            "<tr><td><b>Design size:</b></td><td>%7</td></tr>"
            "<tr><td><b>Fabric cut:</b></td><td>%8 with %9&quot; border</td></tr>"
            "<tr><td><b>Color count:</b></td><td>%10</td></tr>"
            "<tr><td><b>Have:</b></td><td>%11</td></tr>"
            "<tr><td><b>Missing:</b></td><td>%12</td></tr>"
            "<tr><td><b>Missing colors:</b></td><td>%13</td></tr>"
            "<tr><td><b>DMC colors:</b></td><td>%14</td></tr>"
            "<tr><td><b>PDF:</b></td><td>%15</td></tr>"
            "</table>"
            "<p><b>Notes:</b></p>"
            "<p>%16</p>"
        )
            .arg(p.name.toHtmlEscaped())
            .arg(p.status.toHtmlEscaped())
            .arg(p.category.isEmpty() ? "Uncategorized" : p.category.toHtmlEscaped())
            .arg(p.stitchWidth)
            .arg(p.stitchHeight)
            .arg(p.fabricCount)
            .arg(designSizeText(p).toHtmlEscaped())
            .arg(fabricCutText(p).toHtmlEscaped())
            .arg(QString::number(p.borderInches, 'f', 1))
            .arg(dmcColorCountText(p.colors))
            .arg(haveText)
            .arg(missingCountText)
            .arg(missingList)
            .arg(colorText)
            .arg(p.pdfPath.isEmpty() ? "No PDF set." : p.pdfPath.toHtmlEscaped())
            .arg(notesText);

        detailsText->setHtml(text);
    }

    int existingPatternIndexByPdfPath(const QString &path) const {
        QString wantedPath = QFileInfo(path).absoluteFilePath();

        for (int i = 0; i < patterns.size(); ++i) {
            if (QFileInfo(patterns[i].pdfPath).absoluteFilePath() == wantedPath) {
                return i;
            }
        }

        return -1;
    }

    bool hasPdfPath(const QString &path) const {
        return existingPatternIndexByPdfPath(path) >= 0;
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

    QStringList splitCsvLoose(const QString &line) const {
        QString cleaned = line;
        cleaned.replace("\"", "");

        return cleaned.split(QRegularExpression("[,;\\t]"), Qt::KeepEmptyParts);
    }

    QStringList colorsFromCsvFile(const QString &csvPath) const {
        QStringList colors;

        QFile file(csvPath);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return colors;
        }

        int dmcColumn = -1;
        bool headerChecked = false;

        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine()).trimmed();

            if (line.isEmpty()) {
                continue;
            }

            QStringList fields = splitCsvLoose(line);

            if (fields.isEmpty()) {
                continue;
            }

            if (!headerChecked) {
                for (int i = 0; i < fields.size(); ++i) {
                    QString field = fields[i].trimmed().toLower();

                    if (
                        field == "dmc" ||
                        field == "dmc code" ||
                        field == "dmc_code" ||
                        field == "dmc color" ||
                        field == "dmc_color" ||
                        field == "floss" ||
                        field == "floss code" ||
                        field == "color" ||
                        field == "colour"
                    ) {
                        dmcColumn = i;
                        break;
                    }
                }

                headerChecked = true;

                if (dmcColumn >= 0) {
                    continue;
                }
            }

            QString color;

            // Best case: CSV has a real DMC/floss/color column.
            if (dmcColumn >= 0 && dmcColumn < fields.size()) {
                color = normalizeDmcColorToken(fields[dmcColumn]);
            }

            // Common SpriteStitcher-style fallback:
            // Symbol,DMC,Name,Count
            if (color.isEmpty() && fields.size() >= 2) {
                color = normalizeDmcColorToken(fields[1]);
            }

            // Another possible layout:
            // DMC,Name,Symbol,Count
            if (color.isEmpty() && fields.size() >= 1) {
                color = normalizeDmcColorToken(fields[0]);
            }

            // Last fallback for lines like "DMC 310".
            if (color.isEmpty() && line.contains("DMC", Qt::CaseInsensitive)) {
                QStringList parsed = parseDmcColors(line);

                if (!parsed.isEmpty()) {
                    color = parsed.first();
                }
            }

            if (!color.isEmpty() && !colors.contains(color)) {
                colors.append(color);
            }
        }

        return colors;
    }

    QFileInfoList csvFilesUnderFolder(const QString &folder) const {
        QFileInfoList files;

        QDirIterator it(
            folder,
            QStringList() << "*.csv" << "*.CSV",
            QDir::Files,
            QDirIterator::Subdirectories
        );

        while (it.hasNext()) {
            files.append(QFileInfo(it.next()));
        }

        return files;
    }

    QString colorsFromNearbyCsv(const QString &pdfPath, const QString &importRoot = QString(), int *checkedCsvCount = nullptr) const {
        QFileInfo pdfInfo(pdfPath);
        QString pdfBase = pdfInfo.completeBaseName().toLower();

        QFileInfoList csvFiles;

        // Same folder as PDF.
        QDir pdfDir = pdfInfo.dir();
        csvFiles.append(pdfDir.entryInfoList(QStringList() << "*.csv" << "*.CSV", QDir::Files));

        // Whole imported folder tree.
        if (!importRoot.isEmpty()) {
            csvFiles.append(csvFilesUnderFolder(importRoot));
        }

        // De-duplicate CSV paths.
        QSet<QString> seen;
        QFileInfoList uniqueCsvFiles;

        for (const QFileInfo &csvInfo : csvFiles) {
            QString path = csvInfo.absoluteFilePath();

            if (!seen.contains(path)) {
                seen.insert(path);
                uniqueCsvFiles.append(csvInfo);
            }
        }

        if (checkedCsvCount) {
            *checkedCsvCount += uniqueCsvFiles.size();
        }

        QStringList bestColors;
        int bestScore = -1;

        for (const QFileInfo &csvInfo : uniqueCsvFiles) {
            QString csvName = csvInfo.completeBaseName().toLower();
            QString csvFileName = csvInfo.fileName().toLower();

            QStringList colors = colorsFromCsvFile(csvInfo.absoluteFilePath());

            if (colors.isEmpty()) {
                continue;
            }

            int score = 0;

            // Strong filename match.
            if (csvName == pdfBase) {
                score += 200;
            } else if (csvName.contains(pdfBase) || pdfBase.contains(csvName)) {
                score += 120;
            }

            // Common legend names.
            if (
                csvFileName.contains("legend") ||
                csvFileName.contains("floss") ||
                csvFileName.contains("dmc") ||
                csvFileName.contains("color") ||
                csvFileName.contains("colour")
            ) {
                score += 80;
            }

            // Same folder as PDF.
            if (csvInfo.dir().absolutePath() == pdfInfo.dir().absolutePath()) {
                score += 60;
            }

            // More colors is usually more likely to be the real legend.
            score += colors.size();

            if (score > bestScore) {
                bestScore = score;
                bestColors = colors;
            }
        }

        return normalizedDmcColors(bestColors.join(", "));
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
        int importedWithColors = 0;
        int updatedWithColors = 0;
        int skipped = 0;
        int checkedCsvCount = 0;

        while (it.hasNext()) {
            QString pdfPath = it.next();

            int existingIndex = existingPatternIndexByPdfPath(pdfPath);

            if (existingIndex >= 0) {
                QString colors = colorsFromNearbyCsv(pdfPath, folder, &checkedCsvCount);

                if (patterns[existingIndex].colors.trimmed().isEmpty() && !colors.isEmpty()) {
                    patterns[existingIndex].colors = colors;

                    if (!patterns[existingIndex].notes.contains("DMC colors imported from nearby CSV legend.")) {
                        patterns[existingIndex].notes += "\nDMC colors imported from nearby CSV legend.";
                    }

                    updatedWithColors++;
                } else {
                    skipped++;
                }

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
            pattern.colors = colorsFromNearbyCsv(pdfPath, folder, &checkedCsvCount);
            pattern.notes = QString("Imported from %1").arg(folder);

            if (!pattern.colors.isEmpty()) {
                pattern.notes += "\nDMC colors imported from nearby CSV legend.";
                importedWithColors++;
            }

            patterns.push_back(pattern);
            imported++;
        }

        if (imported > 0 || updatedWithColors > 0) {
            savePatterns();
            refreshTable();
        }

        QMessageBox::information(
            this,
            "Import Complete",
            QString("Imported %1 pattern PDF(s).\nImported DMC colors for %2 new pattern(s).\nUpdated DMC colors for %3 existing pattern(s).\nSkipped %4 duplicate PDF(s).\nChecked %5 CSV file(s).")
                .arg(imported)
                .arg(importedWithColors)
                .arg(updatedWithColors)
                .arg(skipped)
                .arg(checkedCsvCount)
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

    QApplication::setApplicationName("PatternShelf");
    QApplication::setOrganizationName("Jesterace");

    MainWindow window;
    window.show();

    return app.exec();
}
