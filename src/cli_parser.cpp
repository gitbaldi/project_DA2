/**
 * @file cli_parser.cpp
 * @brief recieves input from command line and reads the contents of the input file
 */
#include "../include/cli_parser.hpp"
#include "../include/solver.hpp"

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

/// @brief removes the unecessary whitespaces in a string
/// @param s input string to process
/// @return string without the whitespaces
std::string remove_espacos(const std::string &s){
    std::string res =s;
    const std::string whitespace= " \t\r\n";
    size_t first =res.find_first_not_of(whitespace);
    if (first == std::string::npos) return ""; 
    res.erase(0,first);
    size_t last= res.find_last_not_of(whitespace);
    if (last != std::string::npos){
        res.erase(last+1);
    }
    return res;
}

/// @brief removes the quotation marks from a string 
/// @param s input string to process 
/// @return string with just it's text (non-whitespaces/quotation marks)
std::string remove_aspas(const std::string &s){
    std::string result = remove_espacos(s);
    if (result[0] =='"' && result[result.size()-1]=='"' ) return result.substr(1,result.size()-2);
    return result; // se p qlq motivo n tiver aspas
}


/// @brief parses the live ranges file to build the interference graph nodes.
/// * reads variables and their associated program points( + for write, - for last read)
/// * it populates the solver with webs and helps define where variables interfere
/// @param inputFile path to the ranges file 
/// @param solver referernce to the Solver object to store the prsed data
/// @return 0 if sucessful, -1 if file could not be opened
///@note Time complexity: O(L*P) --- L: number of lines of the file ---P: averge number of program points per variable
int parseRanges(std::string inputFile,Solver &solver){
    std::ifstream file(inputFile);
    if(!file.is_open()){
        std::cerr<<"Error: was not possible to open ranges file"<<inputFile<<std::endl;
        return -1;
    }
    std:: string line;
    while (std::getline(file,line)){
        line = remove_espacos(line);
        if (line.empty() || line[0]=='#') continue;
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string varName= remove_espacos(line.substr(0,colonPos));
        std::string rangesPart= line.substr(colonPos+1);

        std::stringstream ss(rangesPart); //para utilizar na separacao das ,
        std::string point; // onde fica guardada a variavel 7+ p.e.

        //⚠️CHAMAR METODO DO SOLVER PARA GERIR A CRIACAO DE NODES
        while (std::getline(ss,point,',')){
            point = remove_espacos(point);
            if (point.empty()) continue;
            if (point.back()=='+'){
                int lineNum = std::stoi(point.substr(0,point.size()-1));
                solver.addLiveRangeStart(varName,lineNum);
            }
            else if (point.back()=='-'){
                int lineNum = std::stoi(point.substr(0,point.size()-1));
                solver.addLiveRangeEnd(varName,lineNum);
            }
            else{
                int lineNum = std::stoi(point);
                solver.addLiveRangePoint(varName,lineNum);
            }
        }
    }
    file.close();
    return 0;
}
/**
 * @brief Parses the register configuration file
 * * This function extracts the number of physical registers available and 
 * the specific allocation algorithm (basic, spilling, or splitting) 
 * including its optional parameter K.
 * * @param inputFile Path to the registers configuration file.
 * @param solver Reference to the Solver object to store these settings.
 * @return 0 on success, -1 if the file cannot be opened.
 * * @note Time Complexity: O(L), where L is the number of lines in the file.
 */
int parseRegisters(std::string inputFile,Solver &solver){
    std::ifstream file(inputFile);
    if(!file.is_open()){
        std::cerr<<"Error: was not possible to open registers file"<<inputFile<<std::endl;
        return -1;
    }
    std:: string line;
    while (std::getline(file,line)){
        line = remove_espacos(line);
        if (line.empty() || line[0]=='#') continue;
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key= remove_espacos(line.substr(0,colonPos));
        std::string value= line.substr(colonPos+1);

        if (key=="registers"){
            solver.setNumRegisters(std::stoi(value));
        }
        else if (key=="algorithm"){
            std::stringstream ss(value);
            std::string algType,param;
            std::getline(ss,algType,',');
            algType = remove_espacos(algType);
            if (std::getline(ss,param)){
                solver.setAlgorithm(algType,std::stoi(remove_espacos(param)));
            }
            else solver.setAlgorithm(algType,0); // sem parametro extra
        }
       

        
    }
    file.close();
    return 0;
}

/**
 * @brief Handles the command-line interface and program execution flow (T1.1).
 * * This function validates the command-line arguments to ensure the program
 * is running in batch mode ('-b'). It orchestrates the parsing of both input
 * files (ranges and registers), sets up the solver, and triggers the 
 * register allocation algorithm and output generation.
 * * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @param solver Reference to the Solver object that manages the allocation process.
 * @return  on success  or -1 if any error occurs 
 * * @note Time Complexity: O(1) for argument validation. The overall complexity 
 * depends on the subsequent calls to parseRanges and parseRegisters.
 */
int parseArguments(int argc, char *argv[], Solver &solver) {
    // check argument count and batch mode flag
    if (argc != 5 || strcmp(argv[1], "-b") != 0) {
        std::cerr << "Correct usage: myProg -b ranges.txt registers.txt allocation.txt" << std::endl;
        return -1;
    }

    std::string rangesFile = argv[2];
    std::string registersFile = argv[3];
    std::string outputFile = argv[4];
    
    
    if (parseRanges(rangesFile,solver)!=0){
        std::cerr<<"Error to process ranges file"<<std::endl;
        return -1;
    }
    if (parseRegisters(registersFile,solver)!=0){
        std::cerr<<"Error to process registers file"<<std::endl;
        return -1;
    }
    solver.updateOutputFile(outputFile);

    //⚠️AQUI VOU CHAMAR AS FUNCOES QUE FAZEM ALGUMA COISA!!!
    // DO PROJ ANTERIOR
    //solver.computeAssignment();
    // dps ver c eles (execucao principal)
    //solver.allocateRegisters();
    //coloquei a chamada na main
    solver.generateOutput(); //not sure

    return 0;
}


