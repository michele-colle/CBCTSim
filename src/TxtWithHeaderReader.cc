// IPEMSpectraReader.cpp
#include "TxtWithHeaderReader.hh"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Constructor
TxtWithHeaderReader::TxtWithHeaderReader() : dataLoaded(false) {}

// Helper function to trim whitespace from strings
std::string TxtWithHeaderReader::trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

// Helper function to split string by delimiter
std::vector<std::string> TxtWithHeaderReader::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

// Convert scientific notation string to double
double TxtWithHeaderReader::parseScientificNotation(const std::string& str) {
    if (str.empty()) return 0.0;
    
    try {
        return std::stod(str);
    } catch (const std::exception& e) {
        // Handle special cases like "0.00E+00"
        if (str.find("E") != std::string::npos || str.find("e") != std::string::npos) {
            return 0.0;
        }
        throw std::runtime_error("Failed to parse number: " + str);
    }
}

// Load data from file
bool TxtWithHeaderReader::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return false;
    }

    try {
        std::string line;
        bool isFirstLine = true;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::vector<std::string> tokens = split(line, '\t');
            
            if (isFirstLine) {
                // Parse header row
                columnHeaders = tokens;
                
                // Initialize data vectors for each column
                for (const auto& header : columnHeaders) {
                    data[header] = std::vector<double>();
                }
                isFirstLine = false;
            } else {
                // Parse data rows
                if (tokens.size() != columnHeaders.size()) {
                    std::cerr << "Warning: Row has " << tokens.size() 
                              << " columns, expected " << columnHeaders.size() << std::endl;
                    continue;
                }

                for (size_t i = 0; i < tokens.size() && i < columnHeaders.size(); ++i) {
                    double value = parseScientificNotation(tokens[i]);
                    data[columnHeaders[i]].push_back(value);
                }
            }
        }

        file.close();
        dataLoaded = true;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error parsing file: " << e.what() << std::endl;
        file.close();
        return false;
    }
}

// Get column data by header name
const std::vector<double>& TxtWithHeaderReader::getColumn(const std::string& columnName) const {
    if (!dataLoaded) {
        throw std::runtime_error("No data loaded. Call loadFromFile() first.");
    }

    auto it = data.find(columnName);
    if (it == data.end()) {
        throw std::runtime_error("Column '" + columnName + "' not found.");
    }
    return it->second;
}

// Get all column headers
const std::vector<std::string>& TxtWithHeaderReader::getHeaders() const {
    return columnHeaders;
}

// Get number of rows
size_t TxtWithHeaderReader::getRowCount() const {
    if (!dataLoaded || columnHeaders.empty()) return 0;
    return data.at(columnHeaders[0]).size();
}

// Get number of columns
size_t TxtWithHeaderReader::getColumnCount() const {
    return columnHeaders.size();
}

// Check if data is loaded
bool TxtWithHeaderReader::isLoaded() const {
    return dataLoaded;
}

// Get value at specific row and column
double TxtWithHeaderReader::getValue(size_t row, const std::string& columnName) const {
    const auto& column = getColumn(columnName);
    if (row >= column.size()) {
        throw std::out_of_range("Row index out of range");
    }
    return column[row];
}

// Print summary of loaded data
void TxtWithHeaderReader::printSummary() const {
    if (!dataLoaded) {
        std::cout << "No data loaded." << std::endl;
        return;
    }

    std::cout << "IPEM Spectra Data Summary:" << std::endl;
    std::cout << "Rows: " << getRowCount() << std::endl;
    std::cout << "Columns: " << getColumnCount() << std::endl;
    std::cout << "Column Headers:" << std::endl;
    
    for (size_t i = 0; i < columnHeaders.size(); ++i) {
        std::cout << "  [" << i << "] " << columnHeaders[i] << std::endl;
    }
}

// Print first few rows for inspection
void TxtWithHeaderReader::printSample(size_t numRows) const {
    if (!dataLoaded) {
        std::cout << "No data loaded." << std::endl;
        return;
    }

    size_t rows = std::min(numRows, getRowCount());
    
    // Print headers
    for (const auto& header : columnHeaders) {
        std::cout << header << "\t";
    }
    std::cout << std::endl;

    // Print data rows
    for (size_t row = 0; row < rows; ++row) {
        for (const auto& header : columnHeaders) {
            std::cout << data.at(header)[row] << "\t";
        }
        std::cout << std::endl;
    }
}

// Check if a column exists
bool TxtWithHeaderReader::hasColumn(const std::string& columnName) const {
    return data.find(columnName) != data.end();
}