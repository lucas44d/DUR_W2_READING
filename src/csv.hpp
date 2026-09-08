/*
Auteur : Lucas Durand
Objectif : Export des données du réfractomètre dans un fichier CSV
*/

#pragma once

#include <fstream>
#include <string>

#include "protocol.hpp"

class CsvWriter {
public:
    ~CsvWriter();

    CsvWriter(const CsvWriter&) = delete;
    CsvWriter& operator=(const CsvWriter&) = delete;
    CsvWriter() = default;

    // Crée le fichier (écrase s'il existe déjà) et écrit l'en-tête
    // Renvoie false si le fichier n'a pas pu être créé (ex: dossier "data/" manquant)
    bool open(const std::string& path);

    // Ajoute une ligne pour une mesure. `timestampPc` est la chaîne déjà formatée "YYYY-MM-DD HH:MM:SS.mmm" 
    // Force un flush disque après chaque ligne pour limiter la perte de données en cas d'arrêt brutal du programme.
    void writeMeasurement(const std::string& timestampPc, const Measurement& m, double concentration, double fT);

    void close();
    bool isOpen() const { return file_.is_open(); }

private:
    std::ofstream file_;
    static constexpr char kDelimiter = ';';
};