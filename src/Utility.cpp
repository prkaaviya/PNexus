#include "Utility.h"

double roundToTwoDecimalPlaces(double value) { return std::round(value * 100.0) / 100.0; }

std::string formatToTwoDecimalPlaces(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value;
    return stream.str();
}