#include <windows.h>
#include <iostream>

int main()
{
    HANDLE serialPort = CreateFileA(
        "\\\\.\\COM10",
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (serialPort == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Impossible d'ouvrir le port COM." << std::endl;
        return 1;
    }

    std::cout << "Port COM ouvert !" << std::endl;

    // Configuration du port
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);

    GetCommState(serialPort, &dcb);

    dcb.BaudRate = CBR_9600;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

    SetCommState(serialPort, &dcb);

    // Timeout
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = 100;
    timeouts.ReadTotalTimeoutConstant    = 500;
    timeouts.ReadTotalTimeoutMultiplier  = 10;

    SetCommTimeouts(serialPort, &timeouts);

    // Lecture
    unsigned char buffer[256];
    DWORD bytesRead;

    while (true)
    {
        if (ReadFile(
            serialPort,
            buffer,
            sizeof(buffer),
            &bytesRead,
            nullptr))
        {
            if (bytesRead > 0)
            {
                std::cout << "Recu " << bytesRead << " octets : ";

                for (DWORD i = 0; i < bytesRead; i++)
                {
                    printf("%02X ", buffer[i]);
                }

                std::cout << std::endl;
            }
        }
    }

    CloseHandle(serialPort);

    return 0;
}