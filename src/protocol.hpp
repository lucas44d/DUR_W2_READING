/*
Auteur : Lucas Durand
Objectif : Analyse du protocole ASCII du refractometre
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Analyse du protocole ASCII du réfractomètre.
//
// Format confirmé par observation d'une vraie trame (43 octets) :
//   "23/05 04:49        8 1.33274  23.22  0.04\r\n"
//    date   heure       n° mesure  indice   temp   brix
//
// Chaque trame est une LIGNE DE TEXTE ASCII terminée par \r\n. Les champs
// sont séparés par un nombre VARIABLE d'espaces (probablement pour un
// alignement en colonnes façon LabVIEW) : on ne suppose donc PAS de largeur
// fixe, on découpe simplement sur les espaces. Cela invalide l'hypothèse
// initiale d'un offset fixe de 24 bytes + 7 bytes binaires — ce n'était pas
// le bon format.
//
// Ordre des champs : date (JJ/MM), heure (HH:MM, sans secondes), numéro de
// mesure, indice de réfraction, température, brix.

struct Measurement {
    std::string date;              // ex: "23/05" (JJ/MM, pas d'année)
    std::string time;              // ex: "04:49" (HH:MM, pas de secondes)
    long measurementNumber = 0;    // ex: 8
    double refractiveIndex = 0.0;  // ex: 1.33274
    double temperature = 0.0;      // ex: 23.22
    double brix = 0.0;             // ex: 0.04
};

// Découpe le flux d'octets bruts en lignes complètes (trames), séparées par
// '\n' (le '\r' précédent, s'il existe, est retiré). Les octets incomplets
// sont conservés d'un appel à l'autre : une trame peut très bien arriver
// répartie sur plusieurs lectures ReadFile.
class LineAccumulator {
public:
    // Ajoute des octets nouvellement reçus et renvoie les lignes complètes
    // trouvées (sans le \r\n final). Peut renvoyer 0, 1 ou plusieurs lignes
    // si plusieurs trames sont arrivées d'un coup.
    std::vector<std::string> addBytes(const std::vector<uint8_t>& data);

private:
    std::string buffer_;
};

// Tente de parser une ligne en Measurement. Renvoie std::nullopt et remplit
// errorOut si la ligne ne correspond pas au format attendu (nombre de
// champs différent de 6, ou champ non convertible en nombre).
std::optional<Measurement> parseLine(const std::string& line, std::string& errorOut);
