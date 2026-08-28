/*
Auteur : Lucas Durand
Objectif : Module d'acquisition serie 
Detail : 
    Ce module ne connait pas le format des trames du refractometre, il doit juste ouvrir le port COM, le configurer
    et fournir les octets qu'il recoit (bruts). Le parsing de la trame se fait dans un autre module

    NOTE : pour le moment pas de parsing car je n'ai pas le detail exacte du format de la trame;
*/

#include "serial.hpp"

#include <sstream>

SerialPort::~SerialPort() {
    close();
}

void SerialPort::setLastWin32Error(const std::string& context) {
    DWORD err = GetLastError();
    LPSTR buf = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buf, 0, nullptr);

    std::ostringstream oss;
    oss << context << " (code " << err << ")";
    if (buf && size > 0) {
        std::string msg(buf, size);
        // Enlève le retour à la ligne final ajouté par FormatMessage.
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
        oss << " : " << msg;
    }
    if (buf) LocalFree(buf);
    lastError_ = oss.str();
}

bool SerialPort::open(const std::string& portName, DWORD baudRate, BYTE byteSize,
                       BYTE parity, BYTE stopBits) {
    if (isOpen()) close();
    portName_ = portName;

    // Windows exige le préfixe "\\.\" pour les ports COM10 et au-delà, on l'utilise systématiquement, il fonctionne aussi pour COM1-COM9.
    std::string fullName = "\\\\.\\" + portName;

    handle_ = CreateFileA(
        fullName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,              // pas de partage
        nullptr,
        OPEN_EXISTING,
        0,              // pas de FILE_FLAG_OVERLAPPED : E/S synchrones, plus simple pour le diagnostic
        nullptr);

    if (handle_ == INVALID_HANDLE_VALUE) {
        setLastWin32Error("Impossible d'ouvrir " + portName);
        return false;
    }

    if (!configureDcb(baudRate, byteSize, parity, stopBits)) {
        close();
        return false;
    }

    if (!configureTimeouts()) {
        close();
        return false;
    }

    // On flush/purge les buffer avant de commencer 
    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return true;
}

bool SerialPort::configureDcb(DWORD baudRate, BYTE byteSize, BYTE parity, BYTE stopBits) {
    DCB dcb;
    SecureZeroMemory(&dcb, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(handle_, &dcb)) {
        setLastWin32Error("GetCommState a échoué sur " + portName_);
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = byteSize;
    dcb.Parity = parity;
    dcb.StopBits = stopBits;
    dcb.fBinary = TRUE;
    dcb.fParity = (parity != NOPARITY);
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(handle_, &dcb)) {
        setLastWin32Error("SetCommState a échoué sur " + portName_ +
                           " (paramètres demandés : " + std::to_string(baudRate) + " bauds)");
        return false;
    }

    return true;
}

bool SerialPort::configureTimeouts() {
    // La mesure arrive environ toutes les 1000 ms. On configure des timeouts courts pour un ReadFile non bloquant "à la demande" :
    // readAvailable() renvoie rapidement, avec ou sans données, plutôt que d'attendre plusieurs secondes.
    COMMTIMEOUTS timeouts;
    SecureZeroMemory(&timeouts, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = 50;         // ms max entre deux octets consécutifs
    timeouts.ReadTotalTimeoutConstant = 100;   // ms max pour la lecture globale
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(handle_, &timeouts)) {
        setLastWin32Error("SetCommTimeouts a échoué sur " + portName_);
        return false;
    }
    return true;
}

size_t SerialPort::readAvailable(std::vector<uint8_t>& buffer, size_t maxBytes) {
    if (!isOpen()) return 0;

    buffer.resize(maxBytes);
    DWORD bytesRead = 0;

    BOOL ok = ReadFile(handle_, buffer.data(), static_cast<DWORD>(maxBytes), &bytesRead, nullptr);
    if (!ok) {
        setLastWin32Error("ReadFile a échoué sur " + portName_);
        buffer.clear();
        return 0;
    }

    buffer.resize(bytesRead);
    return bytesRead;
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
}
