/* 
Auteur : Lucas Durand
Objectif du projet : Lecture du port série du refractometre DUR-W2 de Schmidt+Haensch
Detail : 
    - passage du port COM du refractometre en argument (a chercher dans le gestionnaire des peripheriques et à ajouter dans launch.json)
    - Configuration de la communication 
    - Lecture des donnees brutes
    - Extraction et conversion des champs (fait dans protocol.cpp)
    - timestamp (heure de réception du PC)
    - Export au format CSV

    NOTE: il reste l'interface graphique avec qt à faire
*/ 

#include "serial.hpp"
#include "protocol.hpp"
#include "csv.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <windows.h>

namespace {

// Permet un arrêt propre du programme avec Ctrl+C, plutôt qu'un kill brutal
std::atomic<bool> g_stopRequested{false};

BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        g_stopRequested = true;
        return TRUE;
    }
    return FALSE;
}

// Affiche un buffer d'octets en ASCII, en remplaçant les caractères non
// imprimables par un '.' pour ne pas casser l'affichage terminal.
void printAscii(const std::vector<uint8_t>& data) {
    for (uint8_t b : data) {
        putchar((b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.');
    }
    printf("\n");
}

// Heure système locale du PC : "YYYY-MM-DD HH:MM:SS.mmm"
std::string nowTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tmBuf;
    localtime_s(&tmBuf, &t); // disponible avec le runtime ucrt (MinGW winlibs)

    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// Récupération de l'heure du système mais sans caractere interdit pour générer le fichier csv (pas de ':')
std::string nowFilenameTimestamp() {
    using namespace std::chrono;
    std::time_t t = system_clock::to_time_t(system_clock::now());
    std::tm tmBuf;
    localtime_s(&tmBuf, &t);

    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y%m%d_%H%M%S");
    return oss.str();
}

} // namespace

int main(int argc, char* argv[]) {
    //Vérification si un port à été assigné 
    if (argc < 2) {
        std::cerr << "Le numero de port COM doit d'abord etre identifie dans le gestionnaire de peripheriques Windows\n"
                  << "Il doit ensuite etre transmis dans les inputs du fichier launch.json\n";
        return 1;
    }

    std::string portName = argv[1];
    DWORD baudRate = 9600; // valeur connue pour le réfractomètre (9600 8N1)
    if (argc >= 3) {
        baudRate = static_cast<DWORD>(std::stoul(argv[2]));
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

    SerialPort port;
    std::cout << "Ouverture de " << portName << " a " << baudRate << " bauds (8N1)...\n";

    if (!port.open(portName, baudRate, /*byteSize=*/8, NOPARITY, ONESTOPBIT)) {
        std::cerr << "Erreur : " << port.lastError() << "\n";
        return 1;
    }

    std::cout << portName << " ouvert avec succes. Lecture en cours (Ctrl+C pour arreter)...\n\n";

    std::string csvPath = "data/refractometer_" + nowFilenameTimestamp() + ".csv";
    CsvWriter csv;
    if (!csv.open(csvPath)) {
        std::cerr << "Erreur : impossible de creer " << csvPath
                  << " (le dossier data/ existe-t-il ?)\n";
        port.close();
        return 1;
    }
    std::cout << "Enregistrement CSV dans : " << csvPath << "\n";
    std::cout << "Lecture en cours (Ctrl+C pour arreter proprement)...\n\n";

    LineAccumulator accumulator;
    std::vector<uint8_t> buffer;

    while (!g_stopRequested) {
        size_t n = port.readAvailable(buffer);

        if (n > 0) {
            std::string arrivalTime = nowTimestamp();

            std::cout << "----------------------------------------\n";
            std::cout << "Received " << n << " bytes\n\n";
            std::cout << "\nASCII:\n";
            printAscii(buffer);

            for (const std::string& line : accumulator.addBytes(buffer)) {
                std::string err;
                if (auto m = parseLine(line, err)) {
                    std::cout << "\n[MESURE] "
                              << "n=" << m->measurementNumber
                              << " | indice=" << std::fixed << std::setprecision(5) << m->refractiveIndex
                              << " | temperature=" << std::setprecision(2) << m->temperature
                              << " | brix=" << m->brix
                              << " | heure_reception=" << arrivalTime
                              << "\n";
                    csv.writeMeasurement(arrivalTime, *m);
                } else {
                    std::cout << "\n[ERREUR PARSING] " << err << "\n";
                }
            }
            std::cout << "\n";
        }


        // Evite de saturer le CPU en boucle serree entre deux lectures ;
        // la mesure n'arrivant qu'a ~1 Hz, ce delai est largement suffisant.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "\nArret demande, fermeture...\n";
    csv.close();
    port.close();
    std::cout << "Port et fichier CSV fermes proprement.\n";

    return 0;
}
