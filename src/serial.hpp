/*
Auteur : Lucas Durand
Objectif : Module d'acquisition serie 
Detail : 
    Ce module ne connait pas le format des trames du refractometre, il doit juste ouvrir le port COM, le configurer
    et fournir les octets qu'il recoit (bruts). Le parsing de la trame se fait dans un autre module

    NOTE : pour le moment pas de parsing car je n'ai pas le detail exacte du format de la trame;
*/

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    // Interdit la copie : la classe possède un handle Windows.
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Ouvre et configure le port COM indiqué
    // baudRate, byteSize, parity, stopBits reprennent les paramètres connus du réfractomètre (9600 8N1)
    //   parity   : NOPARITY, ODDPARITY, EVENPARITY (voir <windows.h>)
    //   stopBits : ONESTOPBIT, ONE5STOPBITS, TWOSTOPBITS
    // Renvoie true si le port a été ouvert et configuré avec succès.
    bool open(const std::string& portName,
              DWORD baudRate = 9600,
              BYTE byteSize = 8,
              BYTE parity = NOPARITY,
              BYTE stopBits = ONESTOPBIT);

    // Lit les octets actuellement disponibles sur le port, jusqu'à maxBytes
    // Ne bloque pas indéfiniment : les timeouts sont configurés dans configureTimeouts()
    // Renvoie le nombre d'octets effectivement lus (peut être 0 si rien n'est arrivé)
    size_t readAvailable(std::vector<uint8_t>& buffer, size_t maxBytes = 512);

    // Ferme proprement le port s'il est ouvert. Appelé aussi automatiquement par le destructeur.
    void close();

    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    // Dernier message d'erreur Windows lisible (strategie de diag)
    const std::string& lastError() const { return lastError_; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::string portName_;
    std::string lastError_;

    bool configureDcb(DWORD baudRate, BYTE byteSize, BYTE parity, BYTE stopBits);
    bool configureTimeouts();
    void setLastWin32Error(const std::string& context);
};
