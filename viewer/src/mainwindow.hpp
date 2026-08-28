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


// totalement inchange.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const QString& csvPath, QWidget* parent = nullptr);

private slots:
    void pollCsvFile();
    void onExportImage();

private:
    void processNewData(const QByteArray& newData);
    void addPoint(qint64 timestampMs, double refractiveIndex);
    void updateStatsLabel();
    
    QString csvPath_;
    qint64 lastReadPos_ = 0;
    QString pendingPartialLine_;
    bool headerSkipped_ = false;

    QChart* chart_;
    QLineSeries* series_;
    QDateTimeAxis* axisX_;
    QValueAxis* axisY_;
    QChartView* chartView_;
    QTimer* pollTimer_;
    QLabel* statsLabel_;
    QPushButton* exportButton_;

    // Statistiques cumulees sur l'ensemble de la session (pas seulement
    // sur les points actuellement affiches a l'ecran).
    long long countAll_ = 0;
    double sumAll_ = 0.0;
    double minAll_ = std::numeric_limits<double>::infinity();
    double maxAll_ = -std::numeric_limits<double>::infinity();
 
    // Fenetre glissante : nombre max de points affiches a l'ecran, pour
    // rester lisible et performant meme sur une acquisition longue.
    static constexpr int kMaxPointsDisplayed = 200;
};
