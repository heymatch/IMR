#include <bits/stdc++.h>
using namespace std;

#include "IMR_Partition.h"
#include "IMR_Sequential.h"
#include "IMR_Crosstrack.h"

int main(int argc, char **argv){
    if(argc != 7){
        cerr << "<error> you enter " << argc-1 << " arguments" << endl;
        for(int i = 1; i < argc; ++i){
            cerr << "<error> your " << i << " argument: " << argv[i] << endl;
        }
        cerr << "expected 6 arguments" << endl;
        cerr << "1 argument, mode" << endl;
        cerr << "2 argument, trace type" << endl;
        cerr << "3 argument, setting file" << endl;
        cerr << "4 argument, input file " << endl;
        cerr << "5 argument, output trace file" << endl;
        cerr << "6 argument, evaluation file" << endl;
        exit(EXIT_FAILURE);
    }

    std::string MODE = argv[1];
    std::string TRACE_TYPE = argv[2];
    std::string SETTING_FILE = argv[3];
    std::string INPUT_FILE = argv[4];
    std::string OUTPUT_FILE = argv[5];
    std::string EVALUATION_FILE = argv[6]; 

    clog << "<log> " << argv[4] << " start..." << endl;

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
    
    // * start processing
    try{
        std::ifstream setting_stream(SETTING_FILE);

        if(setting_stream.fail()){
            cerr << "<error> open setting file error" << endl;
            exit(EXIT_FAILURE);
        }

        std::ifstream input_stream(INPUT_FILE);
        std::ofstream output_stream(OUTPUT_FILE);
        output_stream << fixed;

        if(input_stream.fail() || output_stream.fail()){
            cerr << "<error> open I/O file error" << endl;
            exit(EXIT_FAILURE);
        }

        disk->initialize(setting_stream);
        disk->run(input_stream, output_stream);
    }
    catch(const char *e){
        cerr << e << endl;
    }
    catch(std::exception &e){
        cerr << e.what() << endl;
        clog << "<log> " << "force to evaluation" << endl;
    }
    disk->evaluation(EVALUATION_FILE);
    disk->distribution_stream.close();
    disk->evaluation_stream.close();

    clog << "<log> " << argv[3] << " finished" << endl;

    return 0;
}