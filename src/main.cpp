#include <bits/stdc++.h>
using namespace std;

#include "IMR_Partition.h"
#include "IMR_Sequential.h"
#include "IMR_Crosstrack.h"

static std::string MODE;

void mode_parser(std::ifstream &setting_file){
    std::string line;
    std::getline(setting_file, line);

    std::stringstream line_split(line);
    std::string parameter, value;
    std::getline(line_split, parameter, '=');
    std::getline(line_split, value);

    if(parameter != "MODE"){
        std::cerr << "<error> wrong MODE parameter" << std::endl;
        exit(EXIT_FAILURE);
    }
    MODE = value;
}

int main(int argc, char **argv){
    if(argc != 4){
        cerr << "<error> expected 3 arguments" << endl;
        cerr << "1st argument, setting file" << endl;
        cerr << "2nd argument, input file " << endl;
        cerr << "3rd argument, output file" << endl;
    }

    ifstream setting_file(argv[1]);
    ifstream input_file(argv[2]);
    ofstream output_file(argv[3]);

    if(input_file.fail() || output_file.fail()){
        cerr << "<error> open file error" << endl;
    }

    clog << "<log> " << argv[3] << " start..." << endl;

    output_file << fixed;

    mode_parser(setting_file);

    // * redirect to differnt mode
    if(MODE == "PARTITION"){
        using namespace IMR_Partition;

        initialize(setting_file);
        run(input_file, output_file);
        evaluation();
    }
    else if(MODE == "SEQUENTIAL"){
        using namespace IMR_Sequential;
        
        initialize(setting_file);
        run(input_file, output_file);
        evaluation();
    }
    else if(MODE == "CROSSTRACK"){
        using namespace IMR_Crosstrack;
        
        initialize(setting_file);
        run(input_file, output_file);
        evaluation();
    }
    else{
        cerr << "<error> " << MODE << endl;
        cerr << "<error> wrong MODE argument" << endl;
        exit(EXIT_FAILURE);
    }

    clog << "<log> " << argv[2] << " finished" << endl;

    return 0;
}