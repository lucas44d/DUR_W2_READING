#include "csv.hpp"

#include <iomanip>

CsvWriter::~CsvWriter() {
    close();
}

bool CsvWriter::open(const std::string& path) {
    file_.open(path, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        return false;
    }

    file_ << "timestamp_pc" << kDelimiter
          << "measurement_number" << kDelimiter
          << "refractive_index" << kDelimiter
          << "temperature" << kDelimiter
          << "brix" << kDelimiter
          << "concentration" << kDelimiter
          << "f_t" << kDelimiter
          << "device_date" << kDelimiter
          << "device_time" << "\n";
    file_.flush();
    return true;
}

void CsvWriter::writeMeasurement(const std::string& timestampPc, const Measurement& m, double concentration, double fT) {
    if (!file_.is_open()) return;
 
    file_ << timestampPc << kDelimiter
          << m.measurementNumber << kDelimiter
          << std::fixed << std::setprecision(5) << m.refractiveIndex << kDelimiter
          << std::setprecision(2) << m.temperature << kDelimiter
          << m.brix << kDelimiter
          << std::setprecision(3) << concentration << kDelimiter
          << std::setprecision(4) << fT << kDelimiter
          << m.date << kDelimiter
          << m.time << "\n";
 
    // Flush systématique : on privilégie la sécurité des données. la fréquence
    // d'écriture étant de toute façon d'environ 1 Hz.
    file_.flush();
}

void CsvWriter::close() {
    if (file_.is_open()) {
        file_.close();
    }
}