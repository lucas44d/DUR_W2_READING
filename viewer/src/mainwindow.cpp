/*
Auteur : Lucas durand
*/

#include "mainwindow.hpp"

#include <algorithm>

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(const QString& csvPath, QWidget* parent)
    : QMainWindow(parent), csvPath_(csvPath) {
    series_ = new QLineSeries();
    series_->setName("Indice de refraction");

    series_->setPointsVisible(true);
    series_->setMarkerSize(8.0);

    chart_ = new QChart();
    chart_->addSeries(series_);
    chart_->setTitle("Indice de refraction en temps reel");

    axisX_ = new QDateTimeAxis();
    axisX_->setFormat("HH:mm:ss");
    axisX_->setTitleText("Heure (PC)");
    chart_->addAxis(axisX_, Qt::AlignBottom);
    series_->attachAxis(axisX_);

    axisY_ = new QValueAxis();
    axisY_->setTitleText("Indice de refraction");
    axisY_->setLabelFormat("%.5f");
    chart_->addAxis(axisY_, Qt::AlignLeft);
    series_->attachAxis(axisY_);

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    
    statsLabel_ = new QLabel("En attente de mesures...");
    statsLabel_->setStyleSheet("font-size: 13px; padding: 4px;");

    exportButton_ = new QPushButton("Exporter en image...");
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::onExportImage);
 
    auto* bottomBar = new QWidget();
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(8, 4, 8, 4);
    bottomLayout->addWidget(statsLabel_, /*stretch=*/1);
    bottomLayout->addWidget(exportButton_, /*stretch=*/0);
 
    auto* central = new QWidget();
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(chartView_, /*stretch=*/1);
    layout->addWidget(bottomBar, /*stretch=*/0);
    setCentralWidget(central);
 
    resize(900, 650);
    setWindowTitle("Refractometer - visualisation temps reel (" + csvPath_ + ")");

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::pollCsvFile);
    pollTimer_->start(500); // Rafraichissement des données toutes les 500ms (2Hz)

    // Lecture immediate au demarrage, au cas ou le fichier contienne deja des donnees 
    pollCsvFile();
}

void MainWindow::pollCsvFile() {
    QFile file(csvPath_);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Le fichier n'existe pas encore, ou est momentanement inaccessible : ce n'est pas fatal, le code retente au projet tick timer
        return;
    }

    if (!file.seek(lastReadPos_)) {
        file.close();
        return;
    }

    QByteArray newData = file.readAll();
    lastReadPos_ = file.pos();
    file.close();

    if (!newData.isEmpty()) {
        processNewData(newData);
    }
}

void MainWindow::processNewData(const QByteArray& newData) {
    QString text = pendingPartialLine_ + QString::fromUtf8(newData);
    QStringList lines = text.split('\n');

    // La derniere ligne peut etre incomplete si on a lu au milieu d'une ecriture cote programme d'acquisition
    pendingPartialLine_ = lines.isEmpty() ? QString() : lines.takeLast();

    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (!headerSkipped_) {
            // La toute premiere ligne non vide est l'en-tete du CSV
            headerSkipped_ = true;
            continue;
        }

        // Colonnes attendues (voir csv.cpp du programme d'acquisition) :
        // 0 timestamp_pc, 1 measurement_number, 2 refractive_index,
        // 3 temperature, 4 brix, 5 device_date, 6 device_time
        QStringList fields = line.split(';');
        if (fields.size() != 7) {
            continue; // ligne inattendue : ignoree dans le graphique
        }

        QDateTime ts = QDateTime::fromString(fields[0], "yyyy-MM-dd HH:mm:ss.zzz");
        bool ok = false;
        double refractiveIndex = fields[2].toDouble(&ok);

        if (ts.isValid() && ok) {
            addPoint(ts.toMSecsSinceEpoch(), refractiveIndex);
        }
    }
}

void MainWindow::addPoint(qint64 timestampMs, double refractiveIndex) {
    
    ++countAll_;
    sumAll_ += refractiveIndex;
    minAll_ = std::min(minAll_, refractiveIndex);
    maxAll_ = std::max(maxAll_, refractiveIndex);
    updateStatsLabel();
    
    series_->append(static_cast<qreal>(timestampMs), refractiveIndex);

    if (series_->count() > kMaxPointsDisplayed) {
        series_->remove(0);
    }

    if (series_->count() == 0) return;

    qint64 minX = static_cast<qint64>(series_->at(0).x());
    qint64 maxX = static_cast<qint64>(series_->at(series_->count() - 1).x());
    axisX_->setRange(QDateTime::fromMSecsSinceEpoch(minX),
                      QDateTime::fromMSecsSinceEpoch(maxX));

    double minY = series_->at(0).y();
    double maxY = minY;
    for (int i = 1; i < series_->count(); ++i) {
        double y = series_->at(i).y();
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }

    // Petite marge pour eviter que la courbe touche les bords du graphique
    double margin = (maxY - minY) * 0.1;
    if (margin < 1e-6) margin = 0.0005; // cas ou toutes les valeurs sont quasi identiques
    axisY_->setRange(minY - margin, maxY + margin);
}

void MainWindow::updateStatsLabel() {
    if (countAll_ == 0) {
        statsLabel_->setText("En attente de mesures...");
        return;
    }
 
    double mean = sumAll_ / static_cast<double>(countAll_);
    statsLabel_->setText(QString(
        "Mesures : %1  |  Min : %2  |  Max : %3  |  Moyenne : %4")
        .arg(countAll_)
        .arg(minAll_, 0, 'f', 5)
        .arg(maxAll_, 0, 'f', 5)
        .arg(mean, 0, 'f', 5));
}
 
void MainWindow::onExportImage() {
    QString defaultName = "refractometer_graphique_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png";
 
    QString path = QFileDialog::getSaveFileName(
        this, "Exporter le graphique en image", defaultName, "Image PNG (*.png)");
 
    if (path.isEmpty()) return; // annule par l'utilisateur
 
    QPixmap pixmap = centralWidget()->grab();
    if (!pixmap.save(path, "PNG")) {
        QMessageBox::warning(this, "Export echoue",
            "Impossible d'enregistrer l'image a l'emplacement choisi.");
    }
}