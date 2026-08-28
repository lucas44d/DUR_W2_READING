/*
Auteur : Lucas Durand
Objectif : Point d'entrée du visualiseur QT
Détail : Ce programme vietn lire le fichier CSV produit par le programme d'acquisition.
Usage : RefractometerViewer.exe <chemin_du_csv>

*/

#include <QApplication>
#include <QFileDialog>
#include <QString>

#include "mainwindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QString csvPath;
    if (argc >= 2) {
        csvPath = QString::fromLocal8Bit(argv[1]);
    } else {
        csvPath = QFileDialog::getOpenFileName(
            nullptr, "Choisir le fichier CSV du refractometre", "", "CSV (*.csv)");
        if (csvPath.isEmpty()) {
            return 0; // annule par l'utilisateur
        }
    }

    MainWindow window(csvPath);
    window.show();

    return app.exec();
}
