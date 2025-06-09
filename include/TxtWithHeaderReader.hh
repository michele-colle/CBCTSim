// IPEMSpectraReader.h
#ifndef IPEM_SPECTRA_READER_H
#define IPEM_SPECTRA_READER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>

class TxtWithHeaderReader {
private:
    std::unordered_map<std::string, std::vector<double>> data;
    std::vector<std::string> columnHeaders;
    bool dataLoaded;

    // Helper functions
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    double parseScientificNotation(const std::string& str);

public:
    // Constructor
    TxtWithHeaderReader();

    // Load data from file
    bool loadFromFile(const std::string& filename);

    // Data access methods
    const std::vector<double>& getColumn(const std::string& columnName) const;
    const std::vector<std::string>& getHeaders() const;
    double getValue(size_t row, const std::string& columnName) const;

    // Information methods
    size_t getRowCount() const;
    size_t getColumnCount() const;
    bool isLoaded() const;
    bool hasColumn(const std::string& columnName) const;

    // Display methods
    void printSummary() const;
    void printSample(size_t numRows = 5) const;
};

#endif // IPEM_SPECTRA_READER_H
