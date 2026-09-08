/*
Auteur : Lucas durand
Objectif : Fenetre affichant l'indice de réfraction en fonction du temps
Détail : Cette fenetre affiche l'indice de réfraction, lu en continu depuis le fichier CSV (produit par le programme d'acquisition)
On vient seulement lire le CSV (pas le port série) au fur et à mesure (toutes les 500ms)
*/

#pragma once

#include <QDateTime>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QTimer>
 
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& csvPath, QWidget* parent = nullptr);

private slots:
    void pollCsvFile();
    void onExportImage();

private:
    void processNewData(const QByteArray& newData);
    void addRefractiveIndexPoint(qint64 timestampMs, double refractiveIndex);
    void addRtdPoint(qint64 timestampMs, double concentration, double fT);
    void updateStatsLabel();
    void updateRtdLabel();
    
    QString csvPath_;
    qint64 lastReadPos_ = 0;
    QString pendingPartialLine_;
    bool headerSkipped_ = false;

    // graphique 1 : Indice de réfraction
    QChart* chart_;
    QLineSeries* series_;
    QDateTimeAxis* axisX_;
    QValueAxis* axisY_;
    QChartView* chartView_;
    QLabel* statsLabel_;
    QPushButton* exportButton_;

    // Statistiques cumulees sur l'ensemble de la session (pas seulement
    // sur les points actuellement affiches a l'ecran).
    long long countAll_ = 0;
    double sumAll_ = 0.0;
    double minAll_ = std::numeric_limits<double>::infinity();
    double maxAll_ = -std::numeric_limits<double>::infinity();
 
    // graphique 2 : concentration + F(t)
    QChart* chartRtd_;
    QLineSeries* concentrationSeries_;
    QLineSeries* fTSeries_;
    QDateTimeAxis* axisXRtd_;
    QValueAxis* axisYConcentration_;
    QValueAxis* axisYFt_;
    QChartView* chartViewRtd_;
    QLabel* rtdLabel_;

    // Accumulation du temps de residence
    bool hasPrevTimestampForTau_ = false;
    qint64 prevTimestampMsForTau_ = 0;
    double runningTau_ = 0.0;
    double lastFt_ = 0.0;
 
    QTimer* pollTimer_;

    // Fenetre glissante : nombre max de points affiches a l'ecran, pour
    // rester lisible et performant meme sur une acquisition longue.
    static constexpr int kMaxPointsDisplayed = 200;
};
