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
#include <winsock2.h>
#include <ws2tcpip.h>

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
#include <conio.h> 
#include <optional>
#include <algorithm>

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

// Coefficients de l'étalonnage linéaire indice de réfraction -> concentration
constexpr double kCalibA = 0.00146067;
constexpr double kCalibB = 1.33251321;
 
// Concentration initiale/finale de la solution (pour F(t))
constexpr double kConcentrationInit = 0.0;
constexpr double kConcentrationFinal = 4.0;

struct AnalysisResult {
    double concentration;
    double fT;
};

AnalysisResult analyzeMeasurement(const Measurement& m) {
    AnalysisResult r;
    r.concentration = (m.refractiveIndex - kCalibB) / kCalibA;
    r.fT = (r.concentration - kConcentrationInit) / (kConcentrationFinal - kConcentrationInit);
    // Bornage entre 0 et 1 pour limiter l'impact du bruit de mesure
    r.fT = std::clamp(r.fT, 0.0, 1.0);
    return r;
}

//Envoie des données vers teleplot pour affichage en temps réel
void sendToTeleplot(SOCKET sock, const sockaddr_in& destAddr, const Measurement& m,
                     const AnalysisResult& analysis) {
    std::ostringstream oss;
 
    oss << "RefractiveIndex:" << m.refractiveIndex << "|g\n"
        << "Brix:" << m.brix << "|g\n"
        << "Temperature:" << m.temperature << "|g\n"
        << "Concentration:" << analysis.concentration << "|g\n"
        << "F_t:" << analysis.fT << "|g\n";
 
    std::string msg = oss.str();
 
    sendto(sock, msg.c_str(), static_cast<int>(msg.length()), 0,
           reinterpret_cast<const sockaddr*>(&destAddr),
           sizeof(destAddr));
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

// Structure pour garder en mémoire le dernier point lu
struct LastSample {
    Measurement measurement;
    std::string timestamp;
};

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
    std::cout << " Appuyez sur [ESPACE] ou [ENTREE] pour enregistrer un point dans le CSV\n";
    std::cout << "Lecture en cours (Ctrl+C pour arreter proprement)...\n\n";

    LineAccumulator accumulator;
    std::vector<uint8_t> buffer;
    std::optional<LastSample> latestSample;

    // Initialisation du port Teleplot
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET teleplotSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in teleplotAddr{};
    teleplotAddr.sin_family = AF_INET;
    teleplotAddr.sin_port = htons(47269);
    inet_pton(AF_INET, "127.0.0.1", &teleplotAddr.sin_addr);

    // Variable pour suivre l'état de l'enregistrement
    bool recording = false;

    while (!g_stopRequested) {

        // Gestion de la saisie clavier (Bascule On / Off)
        if (_kbhit()) {
            int ch = _getch();
            if (ch == ' ' || ch == '\r' || ch == '\n') {
                recording = !recording; // Inverse l'état (Play / Pause)
                
                if (recording) {
                    std::cout << "\n>>> [DEBUT ENREGISTREMENT CONTINU] <<<\n\n";
                } else {
                    std::cout << "\n>>> [PAUSE ENREGISTREMENT] <<<\n\n";
                }
            }
        }
        
        // Lecture port série et traitement des données
        size_t n = port.readAvailable(buffer);
        if (n > 0) {
            std::string arrivalTime = nowTimestamp();

            for (const std::string& line : accumulator.addBytes(buffer)) {
                std::string err;
                if (auto m = parseLine(line, err)) {
                    
                    // calcul unique pour la concentration et F(t)
                    AnalysisResult analysis = analyzeMeasurement(*m);

                    std::cout << "[FLUX LIVE] "
                              << "n=" << m->measurementNumber
                              << " | RI=" << std::fixed << std::setprecision(5) << m->refractiveIndex
                              << " | Brix=" << std::setprecision(2) << m->brix
                              << " | Conc=" << std::setprecision(3) << analysis.concentration << " %"
                              << " | F(t)=" << std::setprecision(4) << analysis.fT;

                    // Enregistrement CSV automatique SI le mode enregistrement est actif
                    if (recording) {
                        csv.writeMeasurement(arrivalTime, *m, analysis.concentration, analysis.fT);
                        std::cout << " --> [REC]";
                    }

                    std::cout << "\n";

                    // Envoi vers Teleplot (toujours actif)
                    if (teleplotSock != INVALID_SOCKET) {
                        sendToTeleplot(teleplotSock, teleplotAddr, *m, analysis);
                    }
                } else {
                    std::cout << "\n[ERREUR PARSING] " << err << "\n";
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Nettoyage à la fermeture pour teleplot
    if (teleplotSock != INVALID_SOCKET) {
        closesocket(teleplotSock);
    }
    WSACleanup();

    // Fermeture du port série et du CSV
    std::cout << "\nArret demande, fermeture...\n";
    csv.close();
    port.close();
    std::cout << "Port et fichier CSV fermes proprement.\n";

    return 0;
}
