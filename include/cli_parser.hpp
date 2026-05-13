#pragma once
#include <string>
std::string remove_espacos(const std::string &s);
std::string remove_aspas(const std::string &s);
int parseRanges(std::string inputFile,Solver &solver);
int parseRegisters(std::string inputFile,Solver &solver);
int parseArguments(int argc, char *argv[], Solver &solver);