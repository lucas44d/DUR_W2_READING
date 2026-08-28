#include "protocol.hpp"

#include <sstream>
#include <stdexcept>

std::vector<std::string> LineAccumulator::addBytes(const std::vector<uint8_t>& data) {
    if (!data.empty()) {
        buffer_.append(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::vector<std::string> lines;
    size_t pos;
    while ((pos = buffer_.find('\n')) != std::string::npos) {
        std::string line = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 1);
        // Retire un '\r' final s'il est present
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

std::optional<Measurement> parseLine(const std::string& line, std::string& errorOut) {
    // Decoupage sur les espaces : gere naturellement le nombre variable d'espaces entre les champs
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);

    if (tokens.size() != 6) {
        errorOut = "Nombre de champs inattendu (" + std::to_string(tokens.size()) +
                   " au lieu de 6) dans la ligne : \"" + line + "\"";
        return std::nullopt;
    }

    Measurement m;
    try {
        m.date = tokens[0];
        m.time = tokens[1];
        m.measurementNumber = std::stol(tokens[2]);
        m.refractiveIndex = std::stod(tokens[3]);
        m.temperature = std::stod(tokens[4]);
        m.brix = std::stod(tokens[5]);
    } catch (const std::exception& e) {
        errorOut = std::string("Erreur de conversion numerique (") + e.what() +
                   ") dans la ligne : \"" + line + "\"";
        return std::nullopt;
    }

    return m;
}
