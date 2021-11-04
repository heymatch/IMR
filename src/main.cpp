#include <bits/stdc++.h>
using namespace std;

#include "IMR_Partition.h"
#include "IMR_Sequential.h"
#include "IMR_Crosstrack.h"

int main(int argc, char **argv){
    if(argc != 8){
        cerr << "<error> you enter " << argc-1 << " arguments" << endl;
        for(int i = 1; i < argc; ++i){
            cerr << "<error> your " << i << " argument: " << argv[i] << endl;
        }
        cerr << "expected 7 arguments" << endl;
        cerr << "1 argument, mode" << endl;
        cerr << "2 argument, trace type" << endl;
        cerr << "3 argument, setting file" << endl;
        cerr << "4 argument, input file " << endl;
        cerr << "5 argument, output file" << endl;
        cerr << "6 argument, evaluation file" << endl;
        cerr << "7 argument, distribution file" << endl;
        exit(EXIT_FAILURE);
    }

    std::string MODE = argv[1];
    std::string TRACE_TYPE = argv[2];
    ifstream setting_file(argv[3]);
    ifstream input_file(argv[4]);
    ofstream output_file(argv[5]);
    ofstream evaluation_file(argv[6]);
    ofstream distribution_file(argv[7]);

    if(setting_file.fail() || input_file.fail() || output_file.fail() || evaluation_file.fail() || distribution_file.fail()){
        cerr << "<error> open file error" << endl;
        exit(EXIT_FAILURE);
    }

    clog << "<log> " << argv[5] << " start..." << endl;

    output_file << fixed;
    IMR_Base *disk = nullptr;

    // * redirect to differnt mode
    if(MODE == "partition"){
        disk = new IMR_Partition;
    }
    else if(MODE == "sequential-inplace"){
        disk = new IMR_Sequential;
        disk->options.UPDATE_METHOD = Update_Method::IN_PLACE;
    }
    else if(MODE == "sequential-outplace"){
        disk = new IMR_Sequential;
        disk->options.UPDATE_METHOD = Update_Method::OUT_PLACE;
    }
    else if(MODE == "crosstrack-inplace"){
        disk = new IMR_Crosstrack;
        disk->options.UPDATE_METHOD = Update_Method::IN_PLACE;
    }
    else if(MODE == "crosstrack-outplace"){
        disk = new IMR_Crosstrack;
        disk->options.UPDATE_METHOD = Update_Method::OUT_PLACE;
    }
    else{
        cerr << "<error> " << MODE << endl;
        cerr << "<error> wrong MODE argument" << endl;
        exit(EXIT_FAILURE);
    }

    if(TRACE_TYPE == "systor17"){
        disk->options.TRACE_TYPE = Trace_Type::SYSTOR17;
    }
    else if(TRACE_TYPE == "msr"){
        disk->options.TRACE_TYPE = Trace_Type::MSR;
    }
    else{
        cerr << "<error> " << TRACE_TYPE << endl;
        cerr << "<error> wrong TRACE_TYPE argument" << endl;
        exit(EXIT_FAILURE);
    }
    
    disk->initialize(setting_file);
    try{
        disk->run(input_file, output_file);
    }
    catch(const char *e){
        cerr << e << endl;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
        clog << "<log> " << "force to evaluation" << endl;
    }
    disk->evaluation(evaluation_file);

    clog << "<log> " << argv[3] << " finished" << endl;

    return 0;
}